#include <list>
#include <set>
#include <algorithm>
#include <iostream>

#include "tiled_view.h"

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
        std::copy(source + row*tw, source + (row + 1)*tw, img_buf + (image_row*stride + tile_rect.left())*elem_size);
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
    std::vector<std::shared_ptr<std::promise<std::unique_ptr<char>>>> promises, 
    std::vector<char*> image_bufs)
{
    for (auto& job : *waiting_copies)
        job.get();
    
    for (unsigned int image = 0; image < (unsigned int)image_bufs.size(); ++image)
        promises[image]->set_value(std::unique_ptr<char>(image_bufs[image]));
}

/* TakeTouchingFaces() records all vertices of a model that belong to a face 
 * incident on a given tile.
 * 
 * Arguments:
 * model - containing the vertex and face info.
 * region - the tile corners.
 * taken_verts - output. Vertices are appended to the back.
 * 
 * Returns:
 * number of vertices taken.
 */
using VertexBucketTable = std::vector<std::vector<std::vector<Vertex>>>; // vector for rows, vector for cols, vector for vertices.
using VertexCountTable = std::vector<std::vector<size_t>>;
static void BucketTouchingFaces(
    const Model& model, unsigned int img_width, unsigned int img_height, unsigned int tile_width, unsigned int tile_height,
    VertexBucketTable& taken_verts, VertexCountTable& num_taken)
{
    // for now, assume img_* is an integer multiple of tile_* respectively.
    
    unsigned int num_cols = img_width / tile_width;
    unsigned int num_rows = img_height / tile_height;

    std::vector<std::vector<std::vector<bool>>> taken(num_rows);
    if (num_taken.size() != num_rows)
        num_taken.resize(num_rows);

    for (unsigned int row = 0; row < num_rows; ++row) {
        taken[row].resize(num_cols);
        if (num_taken[row].size() != num_cols)
            num_taken[row].resize(num_cols);

        for (unsigned int col = 0; col < num_cols; ++col) {
            taken[row][col].resize(model.second.size());
            num_taken[row][col] = 0;
        }
    }

    for (size_t faceIx = 0; faceIx < model.second.size(); ++faceIx) {
        for (Triangle::value_type ind : model.second[faceIx]) {
            const Vertex& vert = model.first[ind];

            unsigned int col = (unsigned int)(vert.x / float(tile_width));
            unsigned int row = (unsigned int)(vert.y / float(tile_height));

            if (row < num_rows && col < num_cols && !taken[row][col][faceIx]) {
                num_taken[row][col] += 3;
                taken[row][col][faceIx] = true;
            }
        }
    }

    if (taken_verts.size() != num_rows)
        taken_verts.resize(num_rows);
    for (unsigned int row = 0; row < num_rows; ++row) {
        if (taken_verts[row].size() != num_cols)
            taken_verts[row].resize(num_cols);

        for (unsigned int col = 0; col < num_cols; ++col) {
            auto num_existing = taken_verts[row][col].size();
            taken_verts[row][col].resize(num_existing + num_taken[row][col]);

            size_t verts_recorded = num_existing;
            for (size_t faceIx = 0; faceIx < model.second.size(); ++faceIx) {
                if (!taken[row][col][faceIx]) continue;

                for (Triangle::value_type ind : model.second[faceIx]) 
                    taken_verts[row][col][verts_recorded++] = model.first[ind];
            }
        }
    }
	
    // Another future improvement: hold the vertices in a way more conducive to 
    // tile division. Anyway, this very suboptimal version will do for now.
}

TiledView::TiledView(
    RenderAction& render_action,
    unsigned int full_width, unsigned int full_height, unsigned int tile_width, unsigned int tile_height, 
    std::vector<std::shared_ptr<const Model>>& geometry
)
    : m_render_action{render_action},
      m_full_width{full_width}, m_full_height{full_height}, m_tile_width{tile_width}, m_tile_height{tile_height}
{
	m_models = geometry;
    m_render_action.InitGL();
    
    // Here we start representing the model. The vertex array holds
    // a series of vertex attribute buffers.
    glGenVertexArrays(1, &m_varray);
    glBindVertexArray(m_varray);
    
    unsigned int num_width_tiles = m_full_width / m_tile_width;
    unsigned int num_height_tiles = m_full_height / m_tile_height;

    for (unsigned int wtile = 0; wtile < num_width_tiles; ++wtile) {
        for (unsigned int htile = 0; htile < num_height_tiles; ++htile) 
        {
            Tile tile;
            tile.region = {
                (htile + 1)*m_tile_height, 
                (wtile)*m_tile_width, 
                (htile)*m_tile_height, 
                (wtile + 1)*m_tile_width,
            };
            
            m_tiles.push_back(tile);
        }
    }

    VertexBucketTable buckets;
    for (auto model : m_models) {
        VertexCountTable vert_counts;
        BucketTouchingFaces(*model, m_full_width, m_full_height, m_tile_width, m_tile_height, buckets, vert_counts);

        for (unsigned int wtile = 0; wtile < num_width_tiles; ++wtile) {
            for (unsigned int htile = 0; htile < num_height_tiles; ++htile) {
                auto tileIx = wtile * num_height_tiles + htile;
                size_t start = 0;
                auto& modelIndex = m_tiles[tileIx].vertices.GetModelIndex();

                if (modelIndex.size() != 0) {
                    auto& lastIndex = modelIndex[modelIndex.size() - 1];
                    start = lastIndex.first + lastIndex.second;
                }

                if (vert_counts[htile][wtile] > 0)
                    m_tiles[tileIx].vertices.AddModelIndex(start, vert_counts[htile][wtile]);
            }
        }
    }

    for (unsigned int wtile = 0; wtile < num_width_tiles; ++wtile) {
        for (unsigned int htile = 0; htile < num_height_tiles; ++htile) {
            std::vector<Vertex>& tile_verts = buckets[htile][wtile];
            GLuint vert_buf;

            glGenBuffers(1, &vert_buf);
            glBindBuffer(GL_ARRAY_BUFFER, vert_buf);
            glBufferData(GL_ARRAY_BUFFER, tile_verts.size() * sizeof(Vertex), tile_verts.data(), GL_STATIC_DRAW);

            m_tiles[wtile*num_height_tiles + htile].vertices.AddBuffer("positions", vert_buf);
        }
    }

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

void TiledView::Render(size_t slice_num, std::vector<std::shared_ptr<std::promise<std::unique_ptr<char>>>>& promises)
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
    for (auto& tile : m_tiles) {
        m_render_action.PrepareTile(tile.region);
        auto tile_res = m_render_action.StartRender(tile.vertices);
        
        for (unsigned int image = 0; image < (unsigned int)output_sizes.size(); ++image) {
            tile_jobs.push_back(TileJob{
                tile_res[image].first, tile_res[image].second, tile.region, image_bufs[image], m_full_width, output_sizes[image]
            });
        }
    }
    
    // Wait for GPU to finish tiles, and send finished tiles to placement.
    using WaitingVec = std::vector<std::future<bool>>;
    std::shared_ptr<WaitingVec> waiting_copies = std::make_shared<WaitingVec>();
	std::vector<GLuint> discardablePBOs;

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
			discardablePBOs.push_back(job->pbo);
            tile_jobs.erase(erase_pos);
        }
        else {
            ++job;
        }
    }
    
    // Ensure copies finished. This can be done async because it has no OpenGL in it.
    std::async(std::launch::async, 
        SetPromisesWhenDone, waiting_copies, promises, image_bufs);

	for (auto pbo : discardablePBOs)
		glDeleteBuffers(1, &pbo);
}
