
#ifndef UTIL_H
#define UTIL_H

#include <vector>
#include <array>

enum class ImageType {Color, Gray};
int writeImage(const char* filename, int width, int height, ImageType type, const char *buffer, const char* title);

using Vertex = std::array<float, 3>;
using VertexVec = std::vector<Vertex>;
using Triangle = std::array<unsigned int, 3>;
using TriangleVec = std::vector<Triangle>;

std::pair<VertexVec, TriangleVec> readBinarySTL(const char *filename);

#endif
