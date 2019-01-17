#include <iostream>
// #include <fstream> // needed when dumping raw data instead of PNG image, for debugging.

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <boost/program_options.hpp>

#include "util.h"
#include "geometry.h"
#include "opengl_utils.h"
#include "render_server.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

int main(int argc, char **argv) {
    namespace po = boost::program_options;

    po::options_description desc("Allowed options");
    desc.add_options()
            ("img-size", po::value<unsigned int>()->default_value(2048u), "Side of square image generated.")
            ("tile-size", po::value<unsigned int>()->default_value(1024u), "Side of square tile for rendering.")
            ("slice", po::value<size_t>()->default_value(0u))
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
    
    // Load a model, do Q&D size-to-fit and then duplicate it.
    // the two models are the (possibly) rendered scene.
    std::shared_ptr<Model> geometry = std::make_shared<Model>(readBinarySTL("models/donkey.stl"));
    Vertex maxV{ 0., 0., 0. }, minV{ 20000, 20000, 20000 };
    for (auto& vertex : geometry->first) // find bounding box
    {
		maxV = glm::max(vertex, maxV);
        minV = glm::min(vertex, minV); 
    }
    glm::vec3 dims = maxV - minV;
	Vertex::value_type maxDim = std::max({ dims.x, dims.y, dims.z });
    glm::vec3 scale_factor = {width, width, width};
	for (auto& vertex : geometry->first) {
		vertex = (vertex - minV) / maxDim * scale_factor;
	}
	std::cout << glm::to_string(minV) << std::endl;
	std::cout << glm::to_string(maxV) << std::endl;
    
    std::shared_ptr<Model> partner_geom {new Model{*geometry}};
    for (auto& vertex : partner_geom->first) {
        vertex.x += width;
    }
    
    // Start the render server:
    Ashigaru::RenderServer server(tile_width, tile_height);
    
    // Create the view we want to render:
    Ashigaru::TestRenderAction program{tile_width, tile_height};
    auto models = server.RegisterModels(std::vector<std::shared_ptr<Model>>{geometry, partner_geom});
    auto view = server.RegisterView(program, 2*width, height, models).get();
    
    // Render slices:
	std::cout << "Slicing: " << std::endl;
	std::vector<std::future<std::unique_ptr<char>>> res;
	bool batch = vm["slice"].as<size_t>() == 0;
	if (batch) {
		for (size_t slice = 0; slice < 100; ++slice) {
			res = server.ViewSlice(view, slice);
			std::unique_ptr<char> data = res[0].get();
			data = res[1].get();
		}
	}
	else {
		res = server.ViewSlice(view, vm["slice"].as<size_t>());
		// wait for results and save them:
		std::unique_ptr<char> data = std::move(res[0].get());
		writeImage("dump.png", 2 * width, height, ImageType::Color, data.get(), "Ashigaru slice");

		data = res[1].get();
		writeImage("depth.png", 2 * width, height, ImageType::Gray, data.get(), "Ashigaru depth");
	}
    std::cout << "Healthy finish!" << std::endl;
    return 0;
}
