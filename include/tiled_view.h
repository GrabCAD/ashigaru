#pragma once

#include <vector>
#include <future>
#include <thread>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "opengl_utils.h"
#include "util.h"
#include "shader_program.h"

namespace Ashigaru {
    template <typename DT>
    class Rect; // Will come from geometry.h when it comes.
    
    /* CopyTileToResult() places a rendering result from a PBO representing a
     * tile into the full image.
     * 
     * Arguments:
     * pbo - the Pixel Buffer Object representing the rendered tile. 
     *    It is assumed that all memory transferred have completed,
     *    whatever the user needs to do to ensure this.
     * tile_rect - the top/left/bottom right locations in the global
     *    XY plane, where this tile views.
     * img_buf - data buffer for the image. Assumed to contain enough
     *    space for all tiles, otherwise it will segfault. Checking
     *    the input is the user's responsibility.
     * stride - how many pixels in a row for the full image.
     * elem_size - number of bytes per element in the pbo.
     * 
     * Note: obviously, the last two items should be replaces with some 
     *    image class when we get around to it.
     * 
     * Returns: 
     * always true, to signal completion.
     */
    bool CopyTileToResult(const char* source, Rect<unsigned int> tile_rect, char *img_buf, unsigned int stride, unsigned int elem_size);
    
    /* This class should hold all persistent tile data. For example, the
     * per-tile VBOs and per-tile model lookup database that allows only
     * parts of a VBO to be used.
     */
    class TiledView {
        ShaderProgram& m_render_action;
        unsigned int m_full_width, m_full_height;
        unsigned int m_tile_width, m_tile_height;
        Model m_geometry; // reference? moved inside? Will decide later.
        
        std::thread m_render_thread;
        std::vector<std::promise<std::unique_ptr<char>>> m_promises; 
        
        void RenderThreadFunction();
        
    public:
        // For now, assume integer number of tiles in each dimension.
        // The neccessry adjustments to non-integer will wait.
        TiledView(
            ShaderProgram& render_action,
            unsigned int full_width, unsigned int full_height, 
            unsigned int tile_width, unsigned int tile_height,
            Model geometry
        );
        
        ~TiledView() {
            if (m_render_thread.joinable())
                m_render_thread.join();
        }
        
        /* This generates the GPU instructions for all tiles, and returns future
         * pointers to the images generated. The future becomes valid after all tiles
         * have been rendered and copied to their final place, in the background.
         */
        std::vector<std::future<std::unique_ptr<char>>> StartRender();
    };
}
