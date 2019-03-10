#include "opengl_utils.h"

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>

GLuint LoadShaderFromSource(const char *shader_path, GLenum shader_type) {
    GLuint shaderID = glCreateShader(shader_type);
    
    // Read the Vertex Shader code from the file
	std::string code;
	std::ifstream shader_stream(shader_path, std::ios::in);
	if (shader_stream.is_open()){
		std::stringstream sstr;
		sstr << shader_stream.rdbuf();
		code = sstr.str();
		shader_stream.close();
	}
	else{
        std::stringstream msg;
        msg << "Impossible to open shader from " << shader_path;
		throw std::runtime_error(msg.str());
	}
	
	GLint Result = GL_FALSE;
	int InfoLogLength;

	// Compile 
	std::cout << "Compiling shader: " << shader_path << std::endl;
	char const* src_ptr = code.c_str();
	glShaderSource(shaderID, 1, &src_ptr, NULL);
	glCompileShader(shaderID);

	// Check compilation result
	glGetShaderiv(shaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	if (InfoLogLength > 0){
		std::vector<char> error_message(InfoLogLength + 1);
		glGetShaderInfoLog(shaderID, InfoLogLength, NULL, error_message.data());
		throw std::runtime_error(error_message.data());
	}

	return shaderID;
}

GLuint LoadShaders(const char* vertex_file_path, const char* fragment_file_path, const char* geom_file_path)
{
    GLuint ProgramID = glCreateProgram();
	
	// Create the shaders
    GLuint vshader = LoadShaderFromSource(vertex_file_path, GL_VERTEX_SHADER);
	glAttachShader(ProgramID, vshader);
    
    GLuint fshader = 0;
    if (fragment_file_path != nullptr) {
        fshader = LoadShaderFromSource(fragment_file_path, GL_FRAGMENT_SHADER);
        glAttachShader(ProgramID, fshader);
    }
    
    GLuint geom_shader = 0;
    if (geom_file_path != nullptr) {
        geom_shader = LoadShaderFromSource(geom_file_path, GL_GEOMETRY_SHADER);
        glAttachShader(ProgramID, geom_shader);
    }
    
	// Link the program
	printf("Linking program\n");
	glLinkProgram(ProgramID);

	// Check the program
    GLint Result = GL_FALSE;
	int InfoLogLength;

	glGetProgramiv(ProgramID, GL_LINK_STATUS, &Result);
	glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	if (InfoLogLength > 0){
		std::vector<char> error_message(InfoLogLength + 1);
		glGetProgramInfoLog(ProgramID, InfoLogLength, NULL, error_message.data());
		throw std::runtime_error(error_message.data());
	}

	glDetachShader(ProgramID, vshader);
	glDetachShader(ProgramID, fshader);
	glDetachShader(ProgramID, geom_shader);

	glDeleteShader(vshader);
	glDeleteShader(fshader);
    glDeleteShader(geom_shader);

	return ProgramID;
}
