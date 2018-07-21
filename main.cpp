#include <iostream>
#include <fstream>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "util.h"

int main(int argc, char **argv) {
    const GLuint pos_attribute = 0;
    
    // Initialise GLFW
    glewExperimental = true; // Needed for core profile
    if( !glfwInit() )
    {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }
    
    // We are rendering off-screen, but a window is still needed for the context
    // creation. There are hints that this is no longer needed in GL 3.3, but that
    // windows still wants it. So just in case.
    glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // We want OpenGL 3.3
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // We don't want the old OpenGL 

    // Open a window and create its OpenGL context
    GLFWwindow* window; 
    window = glfwCreateWindow( 100, 100, "Ashigaru dummy window", NULL, NULL);
    if( window == NULL ){
        fprintf( stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials.\n" );
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window); // Initialize GLEW
    if( glewInit() != GLEW_OK)
    {
        std::cerr << "Failed to initialize GLEW\n";
        return -1;
    }
    
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
    
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDisableVertexAttribArray(0);
    
    // Get the result and dump it.
    std::uint8_t data[100*100*4];
    glReadBuffer(GL_BACK);
    glReadPixels(0,0,100, 100, GL_BGRA, GL_UNSIGNED_BYTE, data);
    
    writeImage("dump.png", 100, 100, (const char*)data, "Ashigaru slice");
    //std::fstream dumpfile("dump.dump", std::fstream::out);
    //dumpfile.write((char *)data, 100*100*4);
    
    std::cout << "Healthy finish!" << std::endl;
    return 0;
}
