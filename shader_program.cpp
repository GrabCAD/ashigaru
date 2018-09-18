/* References:
 * 
 * [1] // https://stackoverflow.com/questions/12157646/how-to-render-offscreen-on-opengl/12159293#12159293
 */

#include <glm/gtc/matrix_transform.hpp>
#include "shader_program.h"
#include "geometry.h"

#include <iostream>

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
    GLuint render_buf[2];
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glGenRenderbuffers(2, render_buf);
    
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    
    glBindRenderbuffer(GL_RENDERBUFFER, render_buf[0]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, render_buf[0]);
    
    glBindRenderbuffer(GL_RENDERBUFFER, render_buf[1]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, render_buf[1]);
    
    // No side effects, please.
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    return fbo;
}

void print_mat(const glm::mat4& PV) 
{
    std::cout << PV[0][0] << " " << PV[0][1] << " " << PV[0][2] << " " << PV[0][3] << std::endl;
    std::cout << PV[1][0] << " " << PV[1][1] << " " << PV[1][2] << " " << PV[1][3] << std::endl;
    std::cout << PV[2][0] << " " << PV[2][1] << " " << PV[2][2] << " " << PV[2][3] << std::endl;
    std::cout << PV[3][0] << " " << PV[3][1] << " " << PV[3][2] << " " << PV[3][3] << std::endl;    
}

bool Ashigaru::TestShaderProgram::PrepareTile(Rect<unsigned int> tile_rect) {
    Rect<unsigned int>::Corner tl = tile_rect.getTopLeft();
    Rect<unsigned int>::Corner br = tile_rect.getBottomRight();
    unsigned int tw = tile_rect.Width();
    unsigned int th = tile_rect.Height();
    std::cout << (float)(-tw/2) << " . " << (float)(tw/2) << " . " << (float)(-th/2) << " . " << (float)(th/2) << std::endl;
    m_tile_projection = glm::ortho(-(float)(tw/2), (float)(tw/2), -(float)(th/2), (float)(th/2), 0.f, 2048.f);
    
    glm::mat4 view = glm::lookAt(
        glm::vec3{br[1] + tw/2, br[0] + th/2, 0},
        glm::vec3{br[1] + tw/2, br[0] + th/2, 10000},
        glm::vec3{0, 1, 0}
    );
    glm::mat4 PV = m_tile_projection*view;
    
    print_mat(view);
    print_mat(m_tile_projection);
    
    glUseProgram(m_program_ID);
    GLuint MatrixID = glGetUniformLocation(m_program_ID, "projection");
    glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &PV[0][0]);
    
    return true;
}

std::vector<RenderAsyncResult> TestShaderProgram::StartRender(GLuint PosBufferID, size_t num_verts) {
    std::vector<RenderAsyncResult> ret;
    
    // Make positions an attribute of the vertex array used for drawing:
    glEnableVertexAttribArray(pos_attribute);
    glBindBuffer(GL_ARRAY_BUFFER, PosBufferID);
    glVertexAttribPointer(pos_attribute, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    
    // Actual drawing:
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_width, m_height);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClearColor(0.0, 0.0, 0.4, 1.0);
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(m_program_ID);
    glDrawArrays(GL_TRIANGLES, 0, num_verts);
    glDisableVertexAttribArray(0);
    
    // Target for reading pixels:
    GLuint pbo;
    glGenBuffers(1,&pbo);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_PACK_BUFFER, m_width*m_height*4, NULL, GL_STREAM_READ);
    
    // Get the result, async.
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    GLsync read_fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    ret.push_back(std::make_pair(read_fence, pbo));
    
    // Target for reading depth buffer:
    glGenBuffers(1,&pbo);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_PACK_BUFFER, m_width*m_height*2, NULL, GL_STREAM_READ);
    
    // Get the depth, async.
    glReadBuffer(GL_DEPTH_ATTACHMENT);
    glReadPixels(0, 0, m_width, m_height, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, 0);
    read_fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    ret.push_back(std::make_pair(read_fence, pbo));
    
    // Return sync objects:
    return ret;
}
