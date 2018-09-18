#pragma once

#include <vector>
#include <glm/glm.hpp>
#include "opengl_utils.h"

namespace Ashigaru {
    template <typename DT>
    class Rect; // Will come from geometry.h when it comes.
    
    // A result is represented by fence and a PBO.
    // When the fence signals completion, we should have finished reading into the PBO.
    using RenderAsyncResult = std::pair<GLsync, GLuint>; 
    
    /* Abstract class. Each child represent all of the shell for running a 
    shader program and waiting for the results, as many of them as there are.
    
    The model is that the ShaderProgram knows how to run itself, but the user knows 
    the context and circumstances. So, for example, the user is responsible for setting 
    uniforms, separately from the render step itself.
    */
    class ShaderProgram {
    public:
        /* All subclasses are expected to work within a tiling loop. Therefore,
        * this step is here for setting tile parameters before rendering.
        * the implementation can set uniforms or do whatever is necessary.
        * If a child needs more uniforms then those describing the tile,
        * it should implement its own functions for setting them, as in this
        * case the user will be handling a child class directly.
        * 
        * Arguments: 
        * tile_rect - defines the position and size of the tile to prepare.
        * 
        * Returns:
        * success value - maybe it fails when trying to set uniforms or any other
        * surprise.
        */
        virtual bool PrepareTile(Rect<unsigned int> tile_rect) = 0;
        
        // Well, the description of triangles given to StartRender will evolve yet.
        virtual std::vector<RenderAsyncResult> StartRender(GLuint PosBufferID, size_t num_verts) = 0;
    };

    class TestShaderProgram : public ShaderProgram {
        GLuint m_program_ID;
        GLuint m_fbo = 0;
        unsigned int m_width, m_height;
        
        static const GLuint pos_attribute = 0;
        
        glm::mat4 m_tile_projection;
        
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
        virtual bool PrepareTile(Rect<unsigned int> tile_rect) override;
        virtual std::vector<RenderAsyncResult> StartRender(GLuint PosBufferID, size_t num_verts) override;
    };
}
