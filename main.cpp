#include <iostream>
// #include <fstream> // needed when dumping raw data instead of PNG image, for debugging.

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <boost/program_options.hpp>

#include "util.h"
#include "geometry.h"
#include "opengl_utils.h"
#include "shader_program.h"

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

int main(int argc, char **argv) {
    namespace po = boost::program_options;

    po::options_description desc("Allowed options");
    desc.add_options()
            ("img-size", po::value<unsigned int>()->default_value(2048u), "Side of square image generated.")
            ("tile-size", po::value<unsigned int>()->default_value(1024u), "Side of square tile for rendering.")
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

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
    
    // A shaderProgram is responsible for drawing into its own frame buffer.
    unsigned int width = vm["img-size"].as<unsigned int>();
    unsigned int height = width;
    unsigned int tile_width = vm["tile-size"].as <unsigned int>();
    unsigned int tile_height = tile_width;
    auto program = Ashigaru::TestShaderProgram(tile_width, tile_height);

    // Here we start representing the model. The vertex array holds
    // a series of vertex attribute buffers.
    GLuint VertexArrayID;
    glGenVertexArrays(1, &VertexArrayID);
    glBindVertexArray(VertexArrayID);
    
    // Positions of the vertices:
    // Do Q&D size-to-fit just so I can get a fast answer.
    auto geometry = readBinarySTL("models/crystal.stl");
    Vertex maxV{ 0., 0., 0. }, minV{ 20000, 20000, 20000 };
    for (auto& vertex : geometry.first) // find bounding box
    {
        maxV = {
            std::max(vertex[0], maxV[0]),
            std::max(vertex[1], maxV[1]),
            std::max(vertex[2], maxV[2])
        };
        minV = {
            std::min(vertex[0], minV[0]),
            std::min(vertex[1], minV[1]),
            std::min(vertex[2], minV[2])
        };
    }
    glm::vec3 dims = maxV - minV;
    glm::vec3 scale_factor = {width, width, width};
    for (auto& vertex : geometry.first) {
        vertex = (vertex - minV) / dims * scale_factor;
        std::cout << vertex.x << ", " << vertex.y << ", " << vertex.z << std::endl;
    }
    program.PrepareTile(Ashigaru::Rect<unsigned int>{tile_width*2, tile_width*2, tile_width, tile_width});
    
    GLfloat* positions = (float *)geometry.first.data();
    
    GLuint PosBufferID;
    glGenBuffers(1, &PosBufferID);
    glBindBuffer(GL_ARRAY_BUFFER, PosBufferID);
    glBufferData(GL_ARRAY_BUFFER, geometry.first.size()*3*sizeof(float), positions, GL_STATIC_DRAW);
    
    auto res = program.StartRender(PosBufferID, geometry.first.size());
    
    // Busy wait for render finish
    GLsync read_fence = res[0].first;
    while (true)
    {
        auto wait_state = glClientWaitSync(read_fence, 0, 0);
        if (wait_state == GL_ALREADY_SIGNALED)
            break;
        std::cout << "Waiting..." << std::endl;
    }
    
    glBindBuffer(GL_PIXEL_PACK_BUFFER, res[0].second);
    const char *data = (char *)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
    
    writeImage("dump.png", tile_width, tile_height, ImageType::Color, data, "Ashigaru slice");
    
    read_fence = res[1].first;
    while (true)
    {
        auto wait_state = glClientWaitSync(read_fence, 0, 0);
        if (wait_state == GL_ALREADY_SIGNALED)
            break;
        
    }
    
    glBindBuffer(GL_PIXEL_PACK_BUFFER, res[1].second);
    data = (char *)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
    
    writeImage("depth.png", tile_width, tile_height, ImageType::Gray, data, "Ashigaru depth");
    
    std::cout << "Healthy finish!" << std::endl;
    return 0;
}
