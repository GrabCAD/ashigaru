#include <iostream>
// #include <fstream> // needed when dumping raw data instead of PNG image, for debugging.

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <boost/program_options.hpp>

#include "util.h"
#include "opengl_utils.h"

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
 * storage is 32 bit (RGBA).
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
    namespace po = boost::program_options;

    po::options_description desc("Allowed options");
    desc.add_options()
            ("img-size", po::value<int>()->default_value(2048), "Side of square image generated.")
            ("tile-size", po::value<int>()->default_value(1024), "Side of square tile for rendering.")
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    // Which vertex attribute is the position?
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
    int width = vm["img-size"].as<int>();
    int height = width;
    GLuint fbo = SetupRenderTarget(width, height);
    
    // Create and compile our GLSL program from the shaders
    GLuint programID = LoadShaders("shaders/vertex.glsl", "shaders/frag.glsl");

    // Here we start representing the model. The vertex array holds
    // a series of vertex attribute buffers.
    GLuint VertexArrayID;
    glGenVertexArrays(1, &VertexArrayID);
    glBindVertexArray(VertexArrayID);
    
    // Positions of the vertices:
    // Do Q&D size-to-fit just so I can get a fast answer.
    auto geometry = readBinarySTL("models/crystal.stl");
    Vertex maxV{ { 0., 0., 0. } }, minV{ { 20000, 20000, 20000 } };
    for (auto& vertex : geometry.first) // find bounding box
    {
        maxV = { {
            std::max(vertex[0], maxV[0]),
            std::max(vertex[1], maxV[1]),
            std::max(vertex[2], maxV[2])
        } };
        minV = { {
            std::min(vertex[0], minV[0]),
            std::min(vertex[1], minV[1]),
            std::min(vertex[2], minV[2])
        } };
    }
    for (auto& vertex : geometry.first)
    {
            vertex[0] = 2*(vertex[0] - minV[0]) / (maxV[0] - minV[0]) - 1;
            vertex[1] = 2*(vertex[1] - minV[1]) / (maxV[1] - minV[1]) - 1;
            vertex[2] = 2*(vertex[2] - minV[2]) / (maxV[2] - minV[2]) - 1;
    }
    GLfloat* positions = (float *)geometry.first.data();
    
    GLuint PosBufferID;
    glGenBuffers(1, &PosBufferID);
    glBindBuffer(GL_ARRAY_BUFFER, PosBufferID);
    glBufferData(GL_ARRAY_BUFFER, geometry.first.size()*3*sizeof(float), positions, GL_STATIC_DRAW);
    
    // Make positions an attribute of the vertex array used for drawing:
    glEnableVertexAttribArray(pos_attribute);
    glBindBuffer(GL_ARRAY_BUFFER, PosBufferID);
    glVertexAttribPointer(pos_attribute, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    
    // Actual drawing:
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width, height);
    glClearColor(0.0, 0.0, 0.4, 1.0);
    glClear( GL_COLOR_BUFFER_BIT );
    glUseProgram(programID);
    gDlrawArrays(GL_TRIANGLES, 0, geometry.first.size());
    glDisableVertexAttribArray(0);
    
    // https://stackoverflow.com/questions/12157646/how-to-render-offscreen-on-opengl/12159293#12159293
    // Target for reading pixels:
    GLuint pbo;
    glGenBuffers(1,&pbo);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_PACK_BUFFER, width*height*4, NULL, GL_DYNAMIC_READ);
    
    // Get the result and dump it.
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    
    // Busy wait for render finish
    GLsync read_fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    while (true)
    {
        auto wait_state = glClientWaitSync(read_fence, 0, 0);
        if (wait_state == GL_ALREADY_SIGNALED)
            break;
        std::cout << "Waiting..." << std::endl;
    }
    
    const char *data = (char *)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
    
    writeImage("dump.png", width, height, data, "Ashigaru slice");
    //std::fstream dumpfile("dump.dump", std::fstream::out);
    //dumpfile.write((char *)data, width*height*4);
    
    std::cout << "Healthy finish!" << std::endl;
    return 0;
}
