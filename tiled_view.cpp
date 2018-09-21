#include "tiled_view.h"
#include "geometry.h"

using namespace Ashigaru;

void Ashigaru::CopyTileToResult(GLuint pbo, Rect<unsigned int> tile_rect, char *img_buf, unsigned int stride, unsigned int elem_size)
{
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
    const char *data = (char *)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
    
    unsigned int tw = tile_rect.Width()*elem_size;
    for (unsigned int row = 0; row < tile_rect.Height(); ++row) {
        unsigned int image_row = row + tile_rect.bottom();
        std::copy(data + row*tw, data + (row + 1)*tw, img_buf + (image_row*stride + tile_rect.right())*elem_size);
    }
}

std::unique_ptr<char> Ashigaru::PlaceTiles(std::vector<RenderAsyncResult> tiles, std::vector<Rect<unsigned int>> tile_rects, 
    unsigned int image_width, unsigned int image_height, unsigned int elem_size)
{
    // Allocate the result
    char* img = new char[image_height*image_width*elem_size];
    
    // Place all tiles 
    for (size_t tile_ix = 0; tile_ix < tiles.size(); ++tile_ix) {
        // wait for GPU->CPU transfer finish.
        GLsync read_fence = tiles[tile_ix].first;
        while (true)
        {
            auto wait_state = glClientWaitSync(read_fence, 0, 0);
            if (wait_state == GL_ALREADY_SIGNALED)
                break;
            std::this_thread::yield();
        }
        CopyTileToResult(tiles[tile_ix].second, tile_rects[tile_ix], img, image_width, elem_size);
    }
    
    // return the image.
    return std::unique_ptr<char>(img);
}

TiledView::TiledView(
    unsigned int full_width, unsigned int full_height, unsigned int tile_width, unsigned int tile_height, Model geometry
)
    : m_full_width{full_width}, m_full_height{full_height}, m_tile_width{tile_width}, m_tile_height{tile_height}, m_geometry{geometry} 
{}

std::vector<std::future<std::unique_ptr<char>>> 
TiledView::StartRender()
{
    unsigned int num_width_tiles = m_full_width / m_tile_width;
    unsigned int num_height_tiles = m_full_height / m_tile_height;
    
    auto program = TestShaderProgram(m_tile_width, m_tile_height);
    
    std::vector<unsigned int> output_sizes = program.OutputPixelSizes();
    std::vector<std::vector<RenderAsyncResult>> per_image_results {};
    for (unsigned int image = 0; image < (unsigned int)output_sizes.size(); ++image)
        per_image_results.push_back(std::vector<RenderAsyncResult>{});
    
    std::vector<Rect<unsigned int>> tile_rects;
    
    // All this block should be done in construction phase,
    // when we prepare per-tile structures. But for now,
    // all tiles share the same PBO.
    GLfloat* positions = (float *)m_geometry.first.data();
    GLuint PosBufferID;
    glGenBuffers(1, &PosBufferID);
    glBindBuffer(GL_ARRAY_BUFFER, PosBufferID);
    glBufferData(GL_ARRAY_BUFFER, m_geometry.first.size()*3*sizeof(float), positions, GL_STATIC_DRAW);
    
    for (unsigned int wtile = 0; wtile < num_width_tiles; ++wtile) {
        for (unsigned int htile = 0; htile < num_height_tiles; ++htile) 
        {
            Rect<unsigned int> tile_rect = {
                (htile + 1)*m_tile_height, 
                (wtile + 1)*m_tile_width, 
                (htile)*m_tile_height, 
                (wtile)*m_tile_width,
            };
            
            program.PrepareTile(tile_rect);
            auto tile_res = program.StartRender(PosBufferID, m_geometry.first.size());
            
            for (unsigned int image = 0; image < (unsigned int)output_sizes.size(); ++image)
                per_image_results[image].push_back(tile_res[image]);
            
            tile_rects.push_back(tile_rect);
        }
    }
    
    // Generate full images in the background.
    std::vector<std::future<std::unique_ptr<char>>> images;
    for (unsigned int image = 0; image < (unsigned int)output_sizes.size(); ++image)
        images.push_back(std::async(std::launch::deferred,
            PlaceTiles, per_image_results[image], tile_rects, m_full_width, m_full_height, output_sizes[image])
        );
    
    return images;
}
