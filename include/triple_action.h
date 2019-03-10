#pragma once

#include "render_action.h"

namespace Ashigaru {
    
    class TripleAction : public RenderAction {
        unsigned int m_width, m_height;
        size_t m_slice;
        
        // 3 renders per tile, generating 3 maps. 
        // currently considering no priorities (which would require more renders).
        GLuint m_height_program, m_stencil_program, m_color_program;
        GLuint m_height_fbo;
        
        // Scratch data for rendering. Generated in preparation of slice or tile,
        // and used in the actual rendering.
        glm::mat4 m_look_up, m_look_down;
        
    public:
        TripleAction(unsigned int width, unsigned int height);
        
        virtual void InitGL() override;
        virtual bool PrepareTile(Rect<unsigned int> tile_rect) override;
        virtual bool PrepareSlice(size_t slice_num) override { m_slice = slice_num; return true; }
        virtual std::vector<RenderAsyncResult> StartRender(VertexDB vertices) override;
        
        virtual std::vector<unsigned int> OutputPixelSizes() const override { return std::vector<unsigned int>{2}; }
        
    private:
        /* CommitBufferAsync() starts a read from GL to memory of one of the buffers in the frame buffer.
        * It generates the pair of fence/PBO required by external code to track completion and act of 
        * the copied buffer.
        * Assumes that a proper FBO is bound.
        * 
        * Arguments:
        * which - designation of the read buffer as would be given to glReadBuffer (e.g. GL_COLOR_ATTACHMENT0)
        * elem_size - bytes per pixel in the read buffer.
        * format, type - passed along to glReadPixels(), so see that.
        */
        RenderAsyncResult CommitBufferAsync(GLenum which, unsigned short elem_size, GLenum format,  GLenum type);
    };
    
}
