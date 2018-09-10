#pragma once

#include <vector>
#include "opengl_utils.h"

namespace Ashigaru {
    // A result is represented by fence and a PBO.
    // When the fence signals completion, we should have finished reading into the PBO.
    using RenderAsyncResult = std::pair<GLsync, GLuint>; 
    
    /* Abstract class. Each child represent all of the shell for running a 
    shader program and waiting for the results, as many of them as there are.
    */
    class ShaderProgram {
    public:
        virtual std::vector<RenderAsyncResult> StartRender(GLuint PosBufferID, size_t num_verts) = 0;
    };

    class TestShaderProgram : public ShaderProgram {
        GLuint m_program_ID;
        GLuint m_fbo = 0;
        unsigned int m_width, m_height;
        
        static const GLuint pos_attribute = 0;
        
        /* SetupRenderTarget() creates a Frame Buffer Object with one
        * RenderBuffer, sized to the given image dimensions. The internal
        * storage is 32 bit (RGBA).
        * 
        * Arguments:
        * width, height - image dimensions, in [px].
        * 
        * Returns:
        * the GL handle for the FBO, for use with `glBindFramebuffer()`.
        */
        GLuint SetupRenderTarget(unsigned int width, unsigned int height);
        
    public:
        TestShaderProgram(unsigned int width, unsigned int height);
        virtual std::vector<RenderAsyncResult> StartRender(GLuint PosBufferID, size_t num_verts) override;
    };
}
