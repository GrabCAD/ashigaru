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
        /* No OpenGL actions can happen outside the render thread. This is a problem
        * when we want to do things like construction and whatever in the user thread.
        * Since we want the user to control the render *instructions* which this
        * interface embodies, there will be no template magic or other kind of wizardry
        * to construct this elsewhere. 
        * 
        * Instead, we require the object to be used in this 
        * way: do whatever non-GL thing you want in the subclass' constructor and other 
        * new methods. Methods defined in this interface can use OpenGL but can only
        * be called in the render thread. The render thread must call InitGL once
        * before usage.
        */
        virtual void InitGL() = 0;
        
        /* All subclasses are expected to work within a tiling loop. Therefore,
        * this step is here for setting tile parameters before rendering.
        * the implementation can set uniforms or do whatever is necessary.
        * If a child needs more uniforms then those describing the tile,
        * it should implement its own functions for setting them, as in this
        * case the user will be handling a child class directly.
        * If you're running a non-tiled render, just think of this one as 
        * `PrepareImage()`, ok?
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
        
        // How many elements per tile result? That is, what is sizeof(pixel) per result?
        virtual std::vector<unsigned int> OutputPixelSizes() const = 0;
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
        
        virtual void InitGL() override;
        virtual bool PrepareTile(Rect<unsigned int> tile_rect) override;
        virtual std::vector<RenderAsyncResult> StartRender(GLuint PosBufferID, size_t num_verts) override;
        
        // first return is RGBA color, 1 byte per channel. Second is ushort.
        virtual std::vector<unsigned int> OutputPixelSizes() const { return std::vector<unsigned int>{4, 2}; }
    };
}
