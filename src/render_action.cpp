/* References:
 * 
 * [1] // https://stackoverflow.com/questions/12157646/how-to-render-offscreen-on-opengl/12159293#12159293
 */

#include <glm/gtc/matrix_transform.hpp>
#include "render_action.h"
#include "geometry.h"

#include <iostream>

using namespace Ashigaru;

const float TestRenderAction::quad_vertices[][3] = {
    {-1., -1., 0.},
    {+1., -1., 0.},
    {+1., +1., 0.},
    {-1., +1., 0.}
};

const float TestRenderAction::quad_UV[][2] = {
    {0., 0.},
    {1., 0.},
    {1., 1.},
    {0., 1.}
};

TestRenderAction::TestRenderAction(unsigned int width, unsigned int height) 
    : m_width{width}, m_height{height}
{}

void TestRenderAction::InitGL()
{
    // Create and compile our GLSL program from the shaders
    m_full_program = LoadShaders("shaders/vertex.glsl", "shaders/frag.glsl");
    m_height_program = LoadShaders("shaders/passthrough.vertex.glsl", "shaders/take_min.glsl");
    m_fbo = SetupRenderTarget(m_width, m_height);
    
    // Prepare a quad for deferred-shading methods.
    glGenBuffers(1, &m_quad_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_quad_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);
    
    glGenBuffers(1, &m_quad_uv_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_quad_uv_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_UV), quad_UV, GL_STATIC_DRAW);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

GLuint TestRenderAction::SetupRenderTarget(unsigned int width, unsigned int height)
{
    GLuint render_buf;
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glGenRenderbuffers(1, &render_buf);
    
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    
    glBindRenderbuffer(GL_RENDERBUFFER, render_buf);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, render_buf);
    
    // Generate two textures for depth (looking up, looking down). The textures will later be 
    // Combined by quad rendering ("deferred shading")
    glGenTextures(2, m_depth_tex);
    for (auto tex : m_depth_tex) 
    {
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0,GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
        
    // No side effects, please.
    glBindTexture(GL_TEXTURE_2D, 0);
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

bool TestRenderAction::PrepareTile(Rect<unsigned int> tile_rect) {
    Rect<unsigned int>::Corner tl = tile_rect.getTopLeft();
    Rect<unsigned int>::Corner br = tile_rect.getBottomRight();
    unsigned int tw = tile_rect.Width();
    unsigned int th = tile_rect.Height();
    glm::mat4 projection { glm::ortho(-(float)(tw/2), (float)(tw/2), -(float)(th/2), (float)(th/2), 0.f, 2048.f) };
    
    glm::mat4 view = glm::lookAt(
        glm::vec3{br[1] + tw/2, br[0] + th/2, m_slice},
        glm::vec3{br[1] + tw/2, br[0] + th/2, m_slice + 1},
        glm::vec3{0, 1, 0}
    );
    
    // Since we look from below, but want the image as if viewed from above,
    // We just mirror the X axis of the final image, so this is applied after 
    // the orthographic projection.
    glm::mat4 mirror_image = glm::scale(glm::mat4(1.0f), glm::vec3{-1, 1, 1});
    m_look_up = mirror_image*projection*view;
    
    // Now look down from the same place:
    view = glm::lookAt(
        glm::vec3{br[1] + tw/2, br[0] + th/2, m_slice},
        glm::vec3{br[1] + tw/2, br[0] + th/2, m_slice - 1},
        glm::vec3{0, 1, 0}
    );
    m_look_down = projection*view;
        
    return true;
}

std::vector<RenderAsyncResult> TestRenderAction::StartRender(GLuint PosBufferID, size_t num_verts) {
    std::vector<RenderAsyncResult> ret;
    
    // Make positions an attribute of the vertex array used for drawing:
    glEnableVertexAttribArray(pos_attribute);
    glBindBuffer(GL_ARRAY_BUFFER, PosBufferID);
    glVertexAttribPointer(pos_attribute, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glUseProgram(m_full_program);
    
    GLuint MatrixID = glGetUniformLocation(m_full_program, "projection");
    
    // First render: look up.
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_depth_tex[0], 0);
    glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &m_look_up[0][0]);
    
    // Actual drawing:
    glViewport(0, 0, m_width, m_height);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClearColor(0.0, 0.0, 0.4, 1.0);
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, num_verts);
    
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
    
    // Second render: looking down. Only depth is needed. However, if we set draw 
    // buffer to GL_NONE, color is trampled and nobody cares that it's been a subject 
    // of glReadPixels either, so for the demo we just render everything again.
    //glDrawBuffer(GL_NONE);
    
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_depth_tex[1], 0);
    glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &m_look_down[0][0]);
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, num_verts);
    glDisableVertexAttribArray(0);
    
    
    // Combine depth buffers:
    glUseProgram(m_height_program);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_depth_tex[0]);
    glUniform1i(glGetUniformLocation(m_height_program, "tex1"), 0);
    
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, m_depth_tex[1]);
    glUniform1i(glGetUniformLocation(m_height_program, "tex2"), 1);
    
    glEnableVertexAttribArray(pos_attribute);
    glBindBuffer(GL_ARRAY_BUFFER, m_quad_buffer);
    glVertexAttribPointer(pos_attribute, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, m_quad_uv_buffer);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

    glDisable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
    
    // Target for reading depth buffer:
    glGenBuffers(1,&pbo);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_PACK_BUFFER, m_width*m_height*2, NULL, GL_STREAM_READ);
    
    // Get the depth, async.
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, m_width, m_height, GL_RED, GL_UNSIGNED_SHORT, 0);
    read_fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    ret.push_back(std::make_pair(read_fence, pbo));
    
    // Return sync objects:
    return ret;
}
