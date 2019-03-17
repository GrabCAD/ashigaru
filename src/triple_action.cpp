#include <iostream>
#include "triple_action.h"
#include "geometry.h"

#include <glm/gtc/matrix_transform.hpp>

using namespace Ashigaru;

TripleAction::TripleAction(unsigned int width, unsigned int height)
    : m_width { width }, m_height { height } 
{
    
}

void TripleAction::InitGL() {
    m_height_program = LoadShaders("shaders/vertex.glsl", "shaders/height_frag.glsl", "shaders/height_geom.glsl");
    m_stencil_program = LoadShaders("shaders/vertex.glsl", nullptr, nullptr);
    m_color_program = LoadShaders("shaders/vertex.glsl", "shaders/frag.glsl", nullptr);
    
    // Prepare frame buffers for the separate renders, for convenience.
    // This needs to be seriously benchmarked.
    {
        glGenFramebuffers(1, &m_height_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, m_height_fbo);
        
        // Depth buffer:
        GLuint render_buf;
        glGenRenderbuffers(1, &render_buf);
        glBindRenderbuffer(GL_RENDERBUFFER, render_buf);
        
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, m_width, m_height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, render_buf);
        
        // Height buffer:
        glGenRenderbuffers(1, &render_buf);
        glBindRenderbuffer(GL_RENDERBUFFER, render_buf);
        
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA16, m_width, m_height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, render_buf);
        
        if (!(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE))
            throw std::runtime_error("Failed to initialize height framebuf.");
    }
    
    // Stencil and cross-section:
    {
        glGenFramebuffers(1, &m_stencil_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, m_stencil_fbo);
        
        // Depth/stencil buffer:
        GLuint render_buf;
        glGenRenderbuffers(1, &render_buf);
        glBindRenderbuffer(GL_RENDERBUFFER, render_buf);
        
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_width, m_height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, render_buf);       
        
        // Cross-section (color) buffer:
        glGenRenderbuffers(1, &render_buf);
        glBindRenderbuffer(GL_RENDERBUFFER, render_buf);
        
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA16, m_width, m_height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, render_buf);
        
        if (!(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE))
            throw std::runtime_error("Failed to initialize stencil framebuf.");
    }
    
    // Finish without side effects:
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

bool TripleAction::PrepareTile(Rect<unsigned int> tile_rect) {
    Rect<unsigned int>::Corner tl = tile_rect.getTopLeft();
    Rect<unsigned int>::Corner br = tile_rect.getBottomRight();
    unsigned int tw = tile_rect.Width();
    unsigned int th = tile_rect.Height();
    glm::mat4 projection { glm::ortho(-(float)(tw/2), (float)(tw/2), -(float)(th/2), (float)(th/2), -2048.f, 2048.f) };
    
    glm::mat4 view = glm::lookAt(
        glm::vec3{tile_rect.left() + tw/2, tile_rect.bottom() + th/2, m_slice},
        glm::vec3{tile_rect.left() + tw/2, tile_rect.bottom() + th/2, m_slice + 1},
        glm::vec3{0, 1, 0}
    );
    
    // Since we look from below, but want the image as if viewed from above,
    // We just mirror the X axis of the final image, so this is applied after 
    // the orthographic projection.
    glm::mat4 mirror_image = glm::scale(glm::mat4(1.0f), glm::vec3{-1, 1, 1});
    m_look_up = mirror_image*projection*view;
    
    glm::mat4 projection_crop { glm::ortho(-(float)(tw/2), (float)(tw/2), -(float)(th/2), (float)(th/2), 0.f, 2048.f) };
    m_crop_up = mirror_image*projection_crop*view;

    // Now look down from the same place:   
    view = glm::lookAt(
        glm::vec3{tile_rect.left() + tw/2, tile_rect.bottom() + th/2, m_slice},
        glm::vec3{tile_rect.left() + tw/2, tile_rect.bottom() + th/2, m_slice - 1},
        glm::vec3{0, 1, 0}
    );
    m_look_down = projection*view;
        
    return true;
}

std::vector<RenderAsyncResult> TripleAction::StartRender(VertexDB vertices) {
    const GLuint pos_attribute = 0;
    
    std::vector<RenderAsyncResult> ret;
    
    GLuint PosBufferID = vertices.GetBuffer("positions");
    auto modelIndex = vertices.GetModelIndex();

    // Height render setup:
    glEnableVertexAttribArray(pos_attribute);
    glBindBuffer(GL_ARRAY_BUFFER, PosBufferID);
    glVertexAttribPointer(pos_attribute, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
        
    glBindFramebuffer(GL_FRAMEBUFFER, m_height_fbo);
    glUseProgram(m_height_program);
    GLuint shellUniLoc = glGetUniformLocation(m_height_program, "shellID");
    
    GLuint MatrixID = glGetUniformLocation(m_height_program, "projection");
    glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &m_look_up[0][0]);
    
    // Actual drawing:
    glViewport(0, 0, m_width, m_height);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    for (size_t modelNum = 0; modelNum < modelIndex.size(); modelNum++) {
        auto& model = modelIndex[modelNum];
        glUniform1ui(shellUniLoc, modelNum);
        glDrawArrays(GL_TRIANGLES, model.first, model.second);
    }
    
    ret.push_back(CommitBufferAsync(GL_DEPTH_ATTACHMENT, 2, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT));
    ret.push_back(CommitBufferAsync(GL_COLOR_ATTACHMENT0, 2, GL_RED, GL_UNSIGNED_SHORT));
    
    // ---- Now the cross-section in two renders: stencil followed by color.
    glBindFramebuffer(GL_FRAMEBUFFER, m_stencil_fbo);
    glClear(GL_STENCIL_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    for (size_t modelNum = 0; modelNum < modelIndex.size(); modelNum++) {
        auto& model = modelIndex[modelNum];

        glUseProgram(m_stencil_program);
        glUniform1ui(glGetUniformLocation(m_stencil_program, "shellID"), modelNum);

        MatrixID = glGetUniformLocation(m_stencil_program, "projection");
        glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &m_crop_up[0][0]);

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_STENCIL_TEST);

        glStencilFunc(GL_NEVER, 0, -1);
        glStencilOpSeparate(GL_FRONT, GL_INCR_WRAP, GL_KEEP, GL_KEEP);
        glStencilOpSeparate(GL_BACK, GL_DECR_WRAP, GL_KEEP, GL_KEEP);

        glDrawArrays(GL_TRIANGLES, model.first, model.second);

        glEnable(GL_DEPTH_TEST);
        glStencilFunc(GL_NOTEQUAL, 0, -1);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

        glUseProgram(m_color_program);
        glUniform1ui(glGetUniformLocation(m_color_program, "shellID"), modelNum);

        MatrixID = glGetUniformLocation(m_color_program, "projection");
        glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &m_crop_up[0][0]);
        glEnableVertexAttribArray(pos_attribute + 1);

        glDrawArrays(GL_TRIANGLES, model.first, model.second);
    }

    // Clean up.
    glDisableVertexAttribArray(pos_attribute);
    ret.push_back(CommitBufferAsync(GL_COLOR_ATTACHMENT0, 2, GL_RED, GL_UNSIGNED_SHORT));
    
    return ret;
}

RenderAsyncResult TripleAction::CommitBufferAsync(GLenum which, unsigned short elem_size, GLenum format,  GLenum type)
{
    GLuint pbo;
    glGenBuffers(1,&pbo);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_PACK_BUFFER, m_width*m_height*elem_size, NULL, GL_STREAM_READ);
    
    // Get the depth, async.
    glReadBuffer(which);
    glReadPixels(0, 0, m_width, m_height, format, type, 0);
    
    GLsync read_fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    return std::make_pair(read_fence, pbo);
}
