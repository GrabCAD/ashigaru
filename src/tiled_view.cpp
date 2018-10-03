#include <list>
#include <algorithm>
#include <iostream>

#include "tiled_view.h"
#include "geometry.h"

using namespace Ashigaru;

/* CopyTileToResult() places a rendering result from a tile buffer
* into the full image.
* 
* Arguments:
* source - points to contiguous tile memory. Should be of size 
*   tile_width*tile_height*`elem_size`, as infered from `tile_rect` (below).
* tile_rect - the top/left/bottom right locations in the global
*    XY plane, where this tile views.
* img_buf - data buffer for the image. Assumed to contain enough
*    space for all tiles, otherwise it will segfault. Checking
*    the input is the user's responsibility.
* stride - how many pixels in a row for the full image.
* elem_size - number of bytes per element in `source`.
* 
* Note: obviously, the last two items should be replaces with some 
*    image class when we get around to it.
* 
* Returns: 
* always true, to signal completion.
*/
static bool CopyTileToResult(const char* source, Rect<unsigned int> tile_rect, char* img_buf, unsigned int stride, unsigned int elem_size)
{
    unsigned int tw = tile_rect.Width()*elem_size;
    for (unsigned int row = 0; row < tile_rect.Height(); ++row) {
        unsigned int image_row = row + tile_rect.bottom();
        std::copy(source + row*tw, source + (row + 1)*tw, img_buf + (image_row*stride + tile_rect.right())*elem_size);
    }
    
    return true;
}

/* SetPromisesWhenDone() receives a vector of copy-job futures, waits for the 
 * copies to complete, then sets promises for the images these copies create.
 * Meant to be called async.
 * 
 * Arguments:
 * waiting_copies - futures from async calls to the tile copy operation.
 * promises - the promises to set when images are done.
 * image_bufs - the respective values to set in the promises when the 
 *      time comes.
 */
static void SetPromisesWhenDone(
    std::shared_ptr<std::vector<std::future<bool>>> waiting_copies, 
    std::vector<std::promise<std::unique_ptr<char>>>& promises, 
    std::vector<char*> image_bufs)
{
    for (auto& job : *waiting_copies)
        job.get();
    
    for (unsigned int image = 0; image < (unsigned int)image_bufs.size(); ++image)
        promises[image].set_value(std::unique_ptr<char>(image_bufs[image]));
}

TiledView::TiledView(
    RenderAction& render_action,
    unsigned int full_width, unsigned int full_height, unsigned int tile_width, unsigned int tile_height, 
    Model &geometry
)
    : m_render_action{render_action},
      m_full_width{full_width}, m_full_height{full_height}, m_tile_width{tile_width}, m_tile_height{tile_height}, 
      m_geometry{geometry} 
{
    m_render_action.InitGL();
    
    // Here we start representing the model. The vertex array holds
    // a series of vertex attribute buffers.
    glGenVertexArrays(1, &m_varray);
    glBindVertexArray(m_varray);
    
    // Future: per-tile structures. But for now, all tiles share the same VBO.
    GLfloat* positions = (float *)m_geometry.first.data();
    glGenBuffers(1, &m_posbuff);
    glBindBuffer(GL_ARRAY_BUFFER, m_posbuff);
    glBufferData(GL_ARRAY_BUFFER, m_geometry.first.size()*3*sizeof(float), positions, GL_STATIC_DRAW);
}

// Each tile result generates a Sync and PBO object. These are stored 
// in a TileJob struct together with the necessary tile/image information
// for later processing. Then, tile jobs are async executed whenever their 
// fence is ready.
struct TileJob {
    GLsync fence;
    GLuint pbo;
    Rect<unsigned int> tile_rect;
    
    // Yeah, these 3 should be in some Image class. Later.
    char* img;
    unsigned int img_width;
    unsigned int elem_size;
};

void TiledView::Render(size_t slice_num, std::vector<std::promise<std::unique_ptr<char>>>& promises)
{
    std::vector<unsigned int> output_sizes = m_render_action.OutputPixelSizes();
    std::vector<char*> image_bufs(output_sizes.size());
    for (unsigned int image = 0; image < (unsigned int)output_sizes.size(); ++image)
        image_bufs[image] = new char[m_full_height*m_full_width*output_sizes[image]];
    
    std::list<TileJob> tile_jobs {};
    
    unsigned int num_width_tiles = m_full_width / m_tile_width;
    unsigned int num_height_tiles = m_full_height / m_tile_height;
    
    glBindVertexArray(m_varray);
    
    // Give the GPU its day's orders:
    m_render_action.PrepareSlice(slice_num);
    for (unsigned int wtile = 0; wtile < num_width_tiles; ++wtile) {
        for (unsigned int htile = 0; htile < num_height_tiles; ++htile) 
        {
            Rect<unsigned int> tile_rect {
                (htile + 1)*m_tile_height, 
                (wtile + 1)*m_tile_width, 
                (htile)*m_tile_height, 
                (wtile)*m_tile_width,
            };
            
            m_render_action.PrepareTile(tile_rect);
            auto tile_res = m_render_action.StartRender(m_posbuff, m_geometry.first.size());
            
            for (unsigned int image = 0; image < (unsigned int)output_sizes.size(); ++image) {
                tile_jobs.push_back(TileJob{
                    tile_res[image].first, tile_res[image].second, tile_rect, image_bufs[image], m_full_width, output_sizes[image]
                });
            }
        } // end tile.
    }
    
    
    // Wait for GPU to finish tiles, and send finished tiles to placement.
    using WaitingVec = std::vector<std::future<bool>>;
    std::shared_ptr<WaitingVec> waiting_copies = std::make_shared<WaitingVec>();
    
    auto job = tile_jobs.cbegin();
    while(!tile_jobs.empty()) {
        if (job == tile_jobs.cend())
            job = tile_jobs.cbegin();
        
        auto wait_state = glClientWaitSync(job->fence, 0, 0);
        if (wait_state == GL_ALREADY_SIGNALED) {
            glBindBuffer(GL_PIXEL_PACK_BUFFER, job->pbo);
            const char *data = (char *)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
                
            waiting_copies->push_back(std::async(
                std::launch::async, CopyTileToResult, 
                data, job->tile_rect, job->img, job->img_width, job->elem_size
            ));
            
            auto erase_pos = job++;
            tile_jobs.erase(erase_pos);
        }
        else {
            ++job;
        }
    }
    
    // Ensure copies finished. This can be done async because it has no OpenGL in it.
    std::async(std::launch::async, 
        SetPromisesWhenDone, waiting_copies, std::ref(promises), image_bufs);
}
