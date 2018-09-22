#include <list>
#include <algorithm>
#include <iostream>

#include "tiled_view.h"
#include "geometry.h"

using namespace Ashigaru;

// We are rendering off-screen, but a window is still needed for the context
// creation. There are hints that this is no longer needed in GL 3.3, but that
// windows still wants it. So just in case. We generate a window of size 1x1 px,
// and set it to be hidden.
bool CreateWindow()
{
    glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // We want OpenGL 3.3
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // We don't want the old OpenGL 

    // Open a window and create its OpenGL context
    GLFWwindow* window; 
    window = glfwCreateWindow(1, 1, "Ashigaru dummy window", NULL, NULL);
    if( window == NULL ){
        std::cerr << "Failed to open GLFW window." << std::endl;
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(window);
    
        
    if( glewInit() != GLEW_OK)
    {
        std::cerr << "Failed to initialize GLEW\n";
        return false;
    }

    return true;
}

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

TiledView::TiledView(
    unsigned int full_width, unsigned int full_height, unsigned int tile_width, unsigned int tile_height, Model geometry
)
    : m_full_width{full_width}, m_full_height{full_height}, m_tile_width{tile_width}, m_tile_height{tile_height}, m_geometry{geometry} 
{}

// Each tile result generates a Sync and PBO object. These are stored 
// in a tileJob struct together with the necessary tile/image information
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

void TiledView::RenderThreadFunction()
{
    CreateWindow();
    TestShaderProgram program{m_tile_width, m_tile_height};
    
    std::vector<unsigned int> output_sizes = program.OutputPixelSizes();
    std::vector<char*> image_bufs(output_sizes.size());
    for (unsigned int image = 0; image < (unsigned int)output_sizes.size(); ++image)
        image_bufs[image] = new char[m_full_height*m_full_width*output_sizes[image]];
    
    std::list<TileJob> tile_jobs {};
    
    unsigned int num_width_tiles = m_full_width / m_tile_width;
    unsigned int num_height_tiles = m_full_height / m_tile_height;
        
    // Here we start representing the model. The vertex array holds
    // a series of vertex attribute buffers.
    GLuint VertexArrayID;
    glGenVertexArrays(1, &VertexArrayID);
    glBindVertexArray(VertexArrayID);
    
    // All this block should be done in construction phase,
    // when we prepare per-tile structures. But for now,
    // all tiles share the same PBO.
    GLfloat* positions = (float *)m_geometry.first.data();
    GLuint PosBufferID;
    glGenBuffers(1, &PosBufferID);
    glBindBuffer(GL_ARRAY_BUFFER, PosBufferID);
    glBufferData(GL_ARRAY_BUFFER, m_geometry.first.size()*3*sizeof(float), positions, GL_STATIC_DRAW);
    
    // Give the GPU its day's orders:
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
            
            for (unsigned int image = 0; image < (unsigned int)output_sizes.size(); ++image) {
                tile_jobs.push_back(TileJob{
                    tile_res[image].first, tile_res[image].second, tile_rect, image_bufs[image], m_full_width, output_sizes[image]
                });
            }
        } // end tile.
    }
    
    // Wait for GPU to finish tiles, and send finished tiles to placement.
    auto job = tile_jobs.cbegin();
    while(!tile_jobs.empty()) {
        if (job == tile_jobs.cend())
            job = tile_jobs.cbegin();
        
        auto wait_state = glClientWaitSync(job->fence, 0, 0);
        if (wait_state == GL_ALREADY_SIGNALED) {
            CopyTileToResult(job->pbo, job->tile_rect, job->img, job->img_width, job->elem_size);
            
            auto erase_pos = job++;
            tile_jobs.erase(erase_pos);
        }
        else {
            ++job;
        }
    }
    
    for (unsigned int image = 0; image < (unsigned int)output_sizes.size(); ++image)
        m_promises[image].set_value(std::move(std::unique_ptr<char>(image_bufs[image])));
}

std::vector<std::future<std::unique_ptr<char>>> 
TiledView::StartRender()
{
    m_promises.clear();
    m_promises.push_back(std::promise<std::unique_ptr<char>>{});
    m_promises.push_back(std::promise<std::unique_ptr<char>>{});
    
    std::vector<std::future<std::unique_ptr<char>>> images(m_promises.size());
    
    std::transform(
        m_promises.begin(), m_promises.end(), images.begin(), 
        [](std::promise<std::unique_ptr<char>>& p) { return p.get_future(); } 
    );
    m_render_thread = std::move(std::thread(
        [this](){
            RenderThreadFunction();
        }
    ));
    
    return images;
}
