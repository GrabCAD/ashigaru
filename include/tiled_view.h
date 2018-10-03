#pragma once

#include <vector>
#include <future>
#include <thread>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "opengl_utils.h"
#include "util.h"
#include "render_action.h"

namespace Ashigaru {
    template <typename DT>
    class Rect; // Will come from geometry.h when it comes.
    
    /* This class should hold all persistent tile data. For example, the
     * per-tile VBOs and per-tile model lookup database that allows only
     * parts of a VBO to be used.
     * 
     * TiledView is expected to be used *only* in the render thread, and
     * therefore can execute OpenGL calls with impunity.
     */
    class TiledView {
        RenderAction& m_render_action;
        unsigned int m_full_width, m_full_height;
        unsigned int m_tile_width, m_tile_height;
        Model& m_geometry; 
        
        GLuint m_varray;
        GLuint m_posbuff;
        
    public:
        // For now, assume integer number of tiles in each dimension.
        // The neccessry adjustments to non-integer will wait.
        TiledView(
            RenderAction& render_action,
            unsigned int full_width, unsigned int full_height, 
            unsigned int tile_width, unsigned int tile_height,
            Model& geometry
        );
        
        size_t NumOutputs() { return m_render_action.OutputPixelSizes().size(); }
        
        /* This generates the GPU instructions for all tiles, and returns future
         * pointers to the images generated. The future becomes valid after all tiles
         * have been rendered and copied to their final place, in the background.
         */
        void Render(size_t slice_num, std::vector<std::promise<std::unique_ptr<char>>>& promises);
    };
}
