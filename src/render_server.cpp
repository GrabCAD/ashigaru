#include "render_server.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <utility>

using namespace Ashigaru;

// We are rendering off-screen, but a window is still needed for the context
// creation. There are hints that this is no longer needed in GL 3.3, but that
// windows still wants it. So just in case. We generate a window of size 1x1 px,
// and set it to be hidden.
static bool CreateWindow()
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

RenderServer::RenderServer(unsigned int tile_width, unsigned int tile_height) : 
    // I know it's ugly, but in this case we save a move().
    m_render_thread {std::thread(
        [this]() { RenderThreadFunction(); }
    ) },
    m_keep_running {true},
    m_tile_width {tile_width},
    m_tile_height {tile_height}
{}

RenderServer::~RenderServer() {
    m_keep_running = false;
    if (m_render_thread.joinable())
        m_render_thread.join();
}

void RenderServer::RenderThreadFunction() {
    CreateWindow();
    
    while (m_keep_running) {
        // Check for requests for new views.
        {
            std::lock_guard<std::mutex> lck {m_view_reqs_lock};
            
            if (!m_view_requests.empty()) {
                auto& req = m_view_requests.front();
                
                // Create new handle. Revisit this when views become removable.
                ViewHandle handle = static_cast<ViewHandle>(m_views.size());

                m_views.emplace(handle, 
                    TiledView(req.render_action, req.full_width, req.full_height, m_tile_width, m_tile_height, req.geometry)
                );
                
                req.ready.set_value(handle);
                m_view_requests.pop();
                
                continue;
            }
        }
        
        // Handle requested slices.
        {
            std::lock_guard<std::mutex> lck {m_slice_reqs_lock};
            
            if (!m_slice_requests.empty()) {
                auto& req = m_slice_requests.front();
                m_views.at(req.view).Render(req.slice_num, req.promises);
                m_slice_requests.pop();
            }
        }
    } // requests loop.
}

std::vector<RenderServer::ModelHandle> RenderServer::RegisterModels(const std::vector<std::shared_ptr<Model>> models)
{
    std::vector<ModelHandle> ret;
    for (auto model : models) {
        ret.push_back(m_models.size());
        m_models.push_back(std::make_shared<Model>(Model{*model})); // pointer to copy.
    }
    
    return ret;
}

std::future<RenderServer::ViewHandle> RenderServer::RegisterView(RenderAction& render_action,
    unsigned int full_width, unsigned int full_height, 
    const std::vector<ModelHandle>& models)
{
    std::vector<std::shared_ptr<const Model>> view_models;
    
    for (auto modelH : models) {
        if (modelH >= m_models.size())
            throw std::runtime_error("Bad model handle requested for view.");
        
        view_models.push_back(m_models[modelH]);
    }
    
    std::lock_guard<std::mutex> lck{m_view_reqs_lock};
    m_view_requests.push(ViewRequest{
        render_action, full_width, full_height, std::move(view_models), std::promise<ViewHandle>(),
    });
    
    return m_view_requests.back().ready.get_future();
}

std::vector<std::future<std::unique_ptr<char>>>
RenderServer::ViewSlice(ViewHandle view, size_t slice_num)
{
    TiledView& tview = m_views.at(view);
    SliceRequest req;
    req.slice_num = slice_num;
    req.view = view;
    
    for (size_t output = 0; output < tview.NumOutputs(); ++output)
        req.promises.push_back(std::promise<std::unique_ptr<char>>{});
    
    std::vector<std::future<std::unique_ptr<char>>> images;
    for (auto& promise : req.promises)
        images.push_back(promise.get_future());
    
    std::lock_guard<std::mutex> lck{m_slice_reqs_lock};
    m_slice_requests.push(std::move(req));
    
    return images;
}
