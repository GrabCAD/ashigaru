#pragma once

#include <vector>
#include <glm/glm.hpp>
#include "opengl_utils.h"
#include "vertex_db.h"

namespace Ashigaru {
    template <typename DT>
    class Rect; // Will come from geometry.h when it comes.
    
    // A result is represented by fence and a PBO.
    // When the fence signals completion, we should have finished reading into the PBO.
    using RenderAsyncResult = std::pair<GLsync, GLuint>; 
    
    /* Abstract class. Each child represent all of the shell for running a 
    shader program and waiting for the results, as many of them as there are.
    
    The model is that the RenderAction knows how to run itself, but the user knows 
    the context and circumstances. So, for example, the user is responsible for setting 
    uniforms, separately from the render step itself.
    */
    class RenderAction {
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
        
        virtual bool PrepareSlice(size_t slice_num) = 0;
        
        // Well, the description of triangles given to StartRender will evolve yet.
        virtual std::vector<RenderAsyncResult> StartRender(VertexDB vertices) = 0;
        
        // How many elements per tile result? That is, what is sizeof(pixel) per result?
        virtual std::vector<unsigned int> OutputPixelSizes() const = 0;
    };

    class TestRenderAction : public RenderAction {
        GLuint m_full_program, m_height_program;
        GLuint m_fbo = 0;
        unsigned int m_width, m_height;
        
        static const GLuint pos_attribute = 0;
        
        size_t m_slice;
        
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
        TestRenderAction(unsigned int width, unsigned int height);
        
        virtual void InitGL() override;
        virtual bool PrepareTile(Rect<unsigned int> tile_rect) override;
        virtual bool PrepareSlice(size_t slice_num) override { m_slice = slice_num; return true; }
        virtual std::vector<RenderAsyncResult> StartRender(VertexDB vertices) override;
        
        // first return is RGBA color, 1 byte per channel. Second is ushort.
        virtual std::vector<unsigned int> OutputPixelSizes() const override { return std::vector<unsigned int>{2, 2}; }
        
    // Scratch data for rendering. Generated in preparation of slice or tile,
    // and used in the actual rendering.
    private:
        glm::mat4 m_look_up, m_look_down;
        GLuint m_depth_tex[2]; // first looking up, then looking down.
        GLuint m_quad_buffer, m_quad_uv_buffer;
        
        static const float quad_vertices[][3];
        static const float quad_UV[][2];
    
    // Internal operations.
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
