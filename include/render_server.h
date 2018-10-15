#pragma once

#include <vector>
#include <queue>
#include <mutex>
#include <future>
#include <memory>
#include <unordered_map>

#include "util.h"
#include "tiled_view.h"

namespace Ashigaru 
{
    /* Manages the render thread and shared data coming from the user and 
     * required for rendering.
     */
    class RenderServer {
    public:
        using ViewHandle = unsigned int;
        using ModelHandle = size_t;
    
    // Core properties:
    private:
        std::thread m_render_thread; // well, that's what it's all about!
        bool m_keep_running;
        
        unsigned int m_tile_width, m_tile_height;

    // Parallel processing machinery:        
    private:
        std::vector<std::shared_ptr<Model>> m_models; 
        
        struct ViewRequest {
            RenderAction& render_action;
            unsigned int full_width, full_height;
            std::vector<std::shared_ptr<const Model>> geometry;
            std::promise<ViewHandle> ready;
        };
        std::queue<ViewRequest> m_view_requests;
        std::mutex m_view_reqs_lock;
        std::unordered_map<ViewHandle, TiledView> m_views;
        
        struct SliceRequest {
            ViewHandle view;
            size_t slice_num;
            std::vector<std::promise<std::unique_ptr<char>>> promises; // Where to put the result.
        };
        std::queue<SliceRequest> m_slice_requests;
        std::mutex m_slice_reqs_lock;
        
        void RenderThreadFunction();
    
    // Public interface:
    public:
        RenderServer(unsigned int tile_width, unsigned int tile_height);
        ~RenderServer();
        
        /* Copy models into the server and get handles for refering to them later. 
         * Why copy? beause then we are free to transform the objects into the 
         * tray just once, without altering the user's copy. That transform is
         * necessary to do in advance so that the tiling procedure can check which
         * vertices apply to which tile.
         * 
         * In this demo models have no transform so it's not done, but the 
         * architecture is valid in the more general case.
         */
        std::vector<ModelHandle> RegisterModels(const std::vector<std::shared_ptr<Model>> models);
        
        /* Instruct the render thread to cunstruct a new view and ready it for 
         * rendering - tiled or otherwise. 
         * 
         * Arguments:
         * geometry - a rendering scene (3D objects) to copy into the render thread.
         * 
         * Returns:
         * a future that would give a handle to the new view when it's done.
         */
        std::future<ViewHandle> RegisterView(RenderAction& render_action,
            unsigned int full_width, unsigned int full_height, 
            const std::vector<ModelHandle>& models);
        
        /* ViewSlice() instructs the render thread to render a slice.
         * 
         * Arguments:
         * view - a handle to an already created view (see RegisterView). 
         * slice_num - number of slice to render (currently ignored).
         */
        std::vector<std::future<std::unique_ptr<char>>>
        ViewSlice(ViewHandle view, size_t slice_num);
    };
}
