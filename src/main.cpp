#include <iostream>
#include <fstream>
#include <chrono>
// #include <fstream> // needed when dumping raw data instead of PNG image, for debugging.

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>   

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
		("help", "produce help message")
        ("models-file", po::value<std::string>(),
            "Text file, each line is an STL file to render. "
            "Model vertices are assumed to be in assembly coordinates. "
            "Can be a single STL file name instead (detected by extension).")
        ("repeats", po::value<unsigned int>()->default_value(1u), "The model will be repeated this many columns and this many rows.")
        ("single", "Merge all repeats to a single object (reduce draw calls).")
        ("img-size", po::value<unsigned int>()->default_value(2048u), "Side of square image generated.")
        ("tile-size", po::value<unsigned int>()->default_value(1024u), "Side of square tile for rendering.")
        ("slice", po::value<size_t>()->default_value(0u), "If given, do this slice with PNG outputs. Otherwise perform a no-output benchmark")
    ;

	po::positional_options_description p;
	p.add("models-file", -1);

    po::variables_map vm;
	po::store(po::command_line_parser(argc, argv).
		options(desc).positional(p).run(), vm);
    po::notify(vm);

	if (vm.count("help")) {
		std::cout << desc << "\n";
		return 1;
	}

    // Extract command-line arguments for more convenient use.
    unsigned int width = vm["img-size"].as<unsigned int>();
    unsigned int height = width;
    unsigned int tile_width = vm["tile-size"].as<unsigned int>();
    unsigned int tile_height = tile_width;
    unsigned int repeats = vm["repeats"].as<unsigned int>();
    bool singleObject = vm.count("single") != 0;

    // Determine which models to load.
    std::vector<std::string> modelNames;
    std::string modelsFile = vm["models-file"].as<std::string>();
    if (boost::algorithm::to_lower_copy(modelsFile.substr(modelsFile.length() - 3)) == "stl") {
        modelNames.push_back(modelsFile);
    }
    else {
        std::ifstream is(modelsFile);
        std::istream_iterator<std::string> start(is), end;
        modelNames = std::vector<std::string>(start, end);
    }

    // Load aach model, get its dimentions for sizing/fitting.
    std::vector<std::shared_ptr<Model>> masterAssembly;

    float fltMax = std::numeric_limits<float>::max();
    size_t assemblyVertCount = 0;
    Vertex maxV{ 0., 0., 0. }, minV{ fltMax, fltMax, fltMax };
    for (auto& modelName : modelNames) {
        std::shared_ptr<Model> geometry = std::make_shared<Model>(readBinarySTL(modelName.c_str()));
        std::cout << geometry->first.size() << std::endl;
        assemblyVertCount += geometry->first.size();

        for (auto& vertex : geometry->first) // find bounding box
        {
            maxV = glm::max(vertex, maxV);
            minV = glm::min(vertex, minV);
        }
        masterAssembly.push_back(geometry);
    }
    std::cout << "Assembly total vertex count: " << assemblyVertCount << std::endl;

    glm::vec3 dims = maxV - minV;
    Vertex::value_type maxDim = std::max({ dims.x, dims.y, dims.z });

    // Size the model so that the requested number of repeats fits in width.
	float targetModelWidth = float(width) / float(repeats);
    glm::vec3 scale_factor = { targetModelWidth, targetModelWidth, targetModelWidth };
    for (auto model : masterAssembly) {
        for (auto& vertex : model->first) {
            vertex = (vertex - minV) / maxDim * scale_factor;
        }
    }

    // replicate the model for all repeats, moving the vertices accordingly.
    // This might be handled with a transform later, but not now.
	std::vector<std::shared_ptr<Model>> duplicateModels;
	for (unsigned int row = 0; row < repeats; ++row) {
		for (unsigned int col = 0; col < repeats; ++col) {
            for (auto model : masterAssembly) {
                std::shared_ptr<Model> model_ptr{ new Model{ *model } };
                duplicateModels.push_back(model_ptr);

                for (auto& vertex : duplicateModels.back()->first) {
                    vertex.x += targetModelWidth * col;
                    vertex.y += targetModelWidth * row;
                }
            }
		}
	}

    // If user requested, merge the transformed objects to one:
    if (singleObject) {
        std::shared_ptr<Model> monster = std::make_shared<Model>();

        for (auto model : duplicateModels) {
            auto& vertices = monster->first;
            size_t offset = vertices.size();
            vertices.insert(vertices.end(), model->first.cbegin(), model->first.cend());

            // This part wasn't really tested.
            auto& faces = monster->second;
            std::transform(model->second.begin(), model->second.end(), std::back_inserter(faces),
                [offset](Triangle t) { return Triangle{t[0] + offset, t[1] + offset, t[2] + offset}; }
            );
        }

        duplicateModels = { monster };
    }


    // ------   Scene is ready, now process it. ------------ //

    // Initialise GLFW
    glewExperimental = true; // Needed for core profile
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
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
