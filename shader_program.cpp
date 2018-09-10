/* References:
 * 
 * [1] // https://stackoverflow.com/questions/12157646/how-to-render-offscreen-on-opengl/12159293#12159293
 */

#include "shader_program.h"

using namespace Ashigaru;

TestShaderProgram::TestShaderProgram(unsigned int width, unsigned int height) 
    : m_width{width}, m_height{height}
{
    // Create and compile our GLSL program from the shaders
    m_program_ID = LoadShaders("shaders/vertex.glsl", "shaders/frag.glsl");
    m_fbo = SetupRenderTarget(width, height);
}

GLuint TestShaderProgram::SetupRenderTarget(unsigned int width, unsigned int height)
{
    GLuint render_buf;
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glGenRenderbuffers(1, &render_buf);
    glBindRenderbuffer(GL_RENDERBUFFER, render_buf);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, render_buf);
    
    // No side effects, please.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    return fbo;
}

std::vector<RenderAsyncResult> TestShaderProgram::StartRender(GLuint PosBufferID, size_t num_verts) {
    // Make positions an attribute of the vertex array used for drawing:
    glEnableVertexAttribArray(pos_attribute);
    glBindBuffer(GL_ARRAY_BUFFER, PosBufferID);
    glVertexAttribPointer(pos_attribute, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    
    // Actual drawing:
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_width, m_height);
    glClearColor(0.0, 0.0, 0.4, 1.0);
    glClear( GL_COLOR_BUFFER_BIT );
    glUseProgram(m_program_ID);
    glDrawArrays(GL_TRIANGLES, 0, num_verts);
    glDisableVertexAttribArray(0);
    
    // Target for reading pixels:
    GLuint pbo;
    glGenBuffers(1,&pbo);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_PACK_BUFFER, m_width*m_height*4, NULL, GL_DYNAMIC_READ);
    
    // Get the result, async.
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    
    // Return sync objects:
    GLsync read_fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    return std::vector<RenderAsyncResult>{std::make_pair(read_fence, pbo)};
}
