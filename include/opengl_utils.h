#pragma once
#include <GL/glew.h>

GLuint LoadShaders(const char* vertex_file_path, const char* fragment_file_path=nullptr, const char* geom_file_path=nullptr);
