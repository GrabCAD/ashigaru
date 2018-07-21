#include <iostream>
// #include <fstream> // needed when dumping raw data instead of PNG image, for debugging.

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "util.h"

// We are rendering off-screen, but a window is still needed for the context
// creation. There are hints that this is no longer needed in GL 3.3, but that
// windows still wants it. So just in case. We generate a window of size 1x1 px,
// and set it to be hidden.
bool CreateWindow()
{
    glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // We want OpenGL 3.3
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // We don't want the old OpenGL 

    // Open a window and create its OpenGL context
    GLFWwindow* window; 
    window = glfwCreateWindow(1, 1, "Ashigaru dummy window", NULL, NULL);
    if( window == NULL ){
        std::cerr << "Failed to open GLFW window." << std::endl;
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(window);
    return true;
}

/* SetupRenderTarget() creates a Frame Buffer Object with one
 * RenderBuffer, sized to the given image dimensions. The internal
 * storeage is 32 bit (RGBA).
 * 
 * Arguments:
 * width, height - image dimensions, in [px].
 * 
 * Returns:
 * the GL handle for the FBO, for use with `glBindFramebuffer()`.
 */
GLuint SetupRenderTarget(int width, int height)
{
    GLuint fbo, render_buf;
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

int main(int argc, char **argv) {
    const GLuint pos_attribute = 0;
    
    // Initialise GLFW
    glewExperimental = true; // Needed for core profile
    if( !glfwInit() )
    {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }
    
    if (!CreateWindow())
        return -1;
    
    if( glewInit() != GLEW_OK)
    {
        std::cerr << "Failed to initialize GLEW\n";
        return -1;
    }
    
    // We will actually draw to this frame buffer, not the default one.
    int width = 200, height = 200;
    GLuint fbo = SetupRenderTarget(width, height);
    
    // Here we start representing the model. The vertex array holds
    // a series of vertex attribute buffers.
    GLuint VertexArrayID;
    glGenVertexArrays(1, &VertexArrayID);
    glBindVertexArray(VertexArrayID);
    
    // Positions of the vertices:
    GLfloat positions[] = {
        -1.0f, -1.0f, 0.0f,
        1.0f, -1.0f, 0.0f,
        0.0f,  1.0f, 0.0f,
    };
    
    GLuint PosBufferID;
    glGenBuffers(1, &PosBufferID);
    glBindBuffer(GL_ARRAY_BUFFER, PosBufferID);
    glBufferData(GL_ARRAY_BUFFER, sizeof(positions), positions, GL_STATIC_DRAW);
    
    // Make positions an attribute of the vertex array used for drawing:
    glEnableVertexAttribArray(pos_attribute);
    glBindBuffer(GL_ARRAY_BUFFER, PosBufferID);
    glVertexAttribPointer(pos_attribute, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    
    // Actual drawing:
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width, height);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear( GL_COLOR_BUFFER_BIT );
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDisableVertexAttribArray(0);
    
    // Get the result and dump it.
    std::uint8_t data[width*height*4];
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
    
    writeImage("dump.png", width, height, (const char*)data, "Ashigaru slice");
    //std::fstream dumpfile("dump.dump", std::fstream::out);
    //dumpfile.write((char *)data, 100*100*4);
    
    std::cout << "Healthy finish!" << std::endl;
    return 0;
}
