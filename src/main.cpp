#include <iostream>
#include <chrono>
// #include <fstream> // needed when dumping raw data instead of PNG image, for debugging.

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <boost/program_options.hpp>

#include "util.h"
#include "geometry.h"
#include "opengl_utils.h"
#include "render_server.h"
#include "triple_action.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

int main(int argc, char **argv) {
    namespace po = boost::program_options;

    po::options_description desc("Allowed options");
    desc.add_options()
            ("model-path", po::value<std::string>(), "STL file to render.")
            ("repeats", po::value<unsigned int>()->default_value(1u), "The model will be repeated this many columns and this many rows.")
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
    unsigned int tile_width = vm["tile-size"].as<unsigned int>();
    unsigned int tile_height = tile_width;
    unsigned int repeats = vm["repeats"].as<unsigned int>();
    
    // Load a model, do Q&D size-to-fit and then duplicate it.
    // the two models are the (possibly) rendered scene.
    std::shared_ptr<Model> geometry = std::make_shared<Model>( readBinarySTL(vm["model-path"].as<std::string>().c_str()) );
    Vertex maxV{ 0., 0., 0. }, minV{ 20000, 20000, 20000 };
    for (auto& vertex : geometry->first) // find bounding box
    {
		maxV = glm::max(vertex, maxV);
        minV = glm::min(vertex, minV); 
    }
    glm::vec3 dims = maxV - minV;
	Vertex::value_type maxDim = std::max({ dims.x, dims.y, dims.z });

	float targetModelWidth = float(width) / float(repeats);
    glm::vec3 scale_factor = { targetModelWidth, targetModelWidth, targetModelWidth };
	for (auto& vertex : geometry->first) {
		vertex = (vertex - minV) / maxDim * scale_factor;
	}
	std::cout << glm::to_string(minV) << std::endl;
	std::cout << glm::to_string(maxV) << std::endl;

    // replicate the model for all repeats, moving the vertices accordingly.
    // This might be handled with a transform later, but not now.
	std::vector<std::shared_ptr<Model>> duplicateModels;
	for (unsigned int row = 0; row < repeats; ++row) {
		for (unsigned int col = 0; col < repeats; ++col) {
			std::shared_ptr<Model> model_ptr{ new Model{ *geometry } };
			duplicateModels.push_back(model_ptr);

			for (auto& vertex : duplicateModels.back()->first) {
				vertex.x += targetModelWidth * col;
				vertex.y += targetModelWidth * row;
			}
		}
	}
    
    // Start the render server:
    Ashigaru::RenderServer server(tile_width, tile_height);
    
    // Create the view we want to render:
    Ashigaru::TripleAction program{tile_width, tile_height};
    
    auto models = server.RegisterModels(duplicateModels);
    auto view = server.RegisterView(program, width, height, models).get();
    
    // Render slices:
	std::cout << "Slicing: " << std::endl;
	bool batch = vm["slice"].as<size_t>() == 0;
	if (batch) {
		std::vector<std::vector<std::future<std::unique_ptr<char>>>> slices;

		auto start = std::chrono::system_clock::now();
		size_t num_slices = 10;
		for (size_t slice = 0; slice < num_slices; ++slice) {
			slices.push_back(server.ViewSlice(view, slice));
		}
		auto end = std::chrono::system_clock::now();

		while (slices.size()) {
			auto& slice = slices.back();
            for (auto& map : slice)
                map.get();
			
			slices.pop_back();
		}
		auto fullEnd = std::chrono::system_clock::now();

		std::cout << "Sending slice instructions: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()/num_slices << std::endl;
		std::cout << "Finish all slices: " << std::chrono::duration_cast<std::chrono::milliseconds>(fullEnd - start).count()/num_slices << std::endl;
	}
	else {
		std::vector<std::future<std::unique_ptr<char>>> res = server.ViewSlice(view, vm["slice"].as<size_t>());
		// wait for results and save them:
		std::unique_ptr<char> data = std::move(res[0].get());
		writeImage("height.png", width, height, ImageType::Gray, data.get(), "Ashigaru height");

		data = res[1].get();
		writeImage("heightID.png", width, height, ImageType::Gray, data.get(), "Ashigaru height ID");
        
        data = res[2].get();
		writeImage("cross.png", width, height, ImageType::Gray, data.get(), "Ashigaru cross section");
	}
    std::cout << "Healthy finish!" << std::endl;
    return 0;
}
