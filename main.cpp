#include <iostream>
// #include <fstream> // needed when dumping raw data instead of PNG image, for debugging.

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <boost/program_options.hpp>

#include "util.h"
#include "geometry.h"
#include "opengl_utils.h"
#include "tiled_view.h"

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
    
    // A shaderProgram is responsible for drawing into its own frame buffer.
    unsigned int width = vm["img-size"].as<unsigned int>();
    unsigned int height = width;
    unsigned int tile_width = vm["tile-size"].as <unsigned int>();
    unsigned int tile_height = tile_width;
    
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
    
    Ashigaru::TiledView tv(width, height, tile_width, tile_height, geometry);
    std::vector<std::future<std::unique_ptr<char>>> res = tv.StartRender();
    
    std::unique_ptr<char> data = std::move(res[0].get());
    writeImage("dump.png", width, height, ImageType::Color, data.get(), "Ashigaru slice");
    
    data = res[1].get();
    writeImage("depth.png", width, height, ImageType::Gray, data.get(), "Ashigaru depth");
    
    std::cout << "Healthy finish!" << std::endl;
    return 0;
}
