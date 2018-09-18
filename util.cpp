
#include "util.h"

#include <stdio.h>
#include <png.h>
#include <malloc.h>

#include <utility>
#include <fstream>

// http://www.labbookpages.co.uk/software/imgProc/libPNG.html
int writeImage(const char* filename, int width, int height, ImageType type, const char *buffer, const char* title)
{
    int code = 0;
    FILE *fp = NULL;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    png_bytep row = NULL;
    
    // Open file for writing (binary mode)
    fp = fopen(filename, "wb");
    if (fp == NULL) {
        fprintf(stderr, "Could not open file %s for writing\n", filename);
        code = 1;
        goto finalise;
    }
    
    // Initialize write structure
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL) {
        fprintf(stderr, "Could not allocate write struct\n");
        code = 1;
        goto finalise;
    }

    // Initialize info structure
    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
        fprintf(stderr, "Could not allocate info struct\n");
        code = 1;
        goto finalise;
    }
    
    // Setup Exception handling
   if (setjmp(png_jmpbuf(png_ptr))) {
      fprintf(stderr, "Error during png creation\n");
      code = 1;
      goto finalise;
   }
   
   png_init_io(png_ptr, fp);

   // Write header (8 bit colour depth, 16-bit gray depth)
   unsigned int pixel_size;
   if (type == ImageType::Color) {
      png_set_IHDR(png_ptr, info_ptr, width, height,
        8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
      pixel_size = 4;
   }
   else {
      png_set_IHDR(png_ptr, info_ptr, width, height,
        16, PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
      pixel_size = 2;
   }
   
   // Set title
   if (title != NULL) {
      png_text title_text;
      title_text.compression = PNG_TEXT_COMPRESSION_NONE;
      title_text.key = (png_charp)"Title";
      title_text.text = (png_charp)title;
      png_set_text(png_ptr, info_ptr, &title_text, 1);
   }

   png_write_info(png_ptr, info_ptr);
   
    // Allocate memory for one row
    row = (png_bytep) malloc(pixel_size * width * sizeof(png_byte));

    // Write image data
    for (int y=0 ; y<height ; y++) {
        if (type == ImageType::Color)
            std::copy(&(buffer[y*width*pixel_size]), &(buffer[(y + 1)*width*pixel_size]), row);
        else { 
            // One little / two litte / three little endians :)
            // The data to the PNG library hyas to be big-endian.
            for (int x = 0; x < width; ++x) {
                unsigned short grayval = ((unsigned short *)buffer)[y*width + x];
                row[x*2] = grayval >> 8;
                row[x*2 + 1] = grayval & 0xFF;
            }
        }
        png_write_row(png_ptr, row);
    }

    // End write
    png_write_end(png_ptr, NULL);
    
finalise:
   if (fp != NULL) fclose(fp);
   if (info_ptr != NULL) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
   if (png_ptr != NULL) png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
   if (row != NULL) free(row);

   return code;
}

/////// Next 3 functions based on https://github.com/dillonhuff/stl_parser   ///////////

float parse_float(std::ifstream& s) {
	char f_buf[sizeof(float)];
	s.read(f_buf, 4);
	float* fptr = (float*)f_buf;
	return *fptr;
}

Vertex parse_point(std::ifstream& s) {
	float x = parse_float(s);
	float y = parse_float(s);
	float z = parse_float(s);
	return Vertex{ x, y, z };
}

std::pair<VertexVec, TriangleVec> readBinarySTL(const char *filename)
{
	std::ifstream stl_file(filename, std::ios::in | std::ios::binary);
	if (!stl_file) 
		throw std::runtime_error("COULD NOT READ FILE");

	char header_info[80] = "";
	char n_triangles[4];
	stl_file.read(header_info, 80);
	stl_file.read(n_triangles, 4);
	
	unsigned int* r = (unsigned int*)n_triangles;
	unsigned int num_triangles = *r;
	
	VertexVec vertices;
	TriangleVec faces;

	for (unsigned int i = 0; i < num_triangles; i++) 	{
		auto num_verts = vertices.size();
		Triangle face = { num_verts, num_verts + 1, num_verts + 2 };
		faces.push_back(face);
		
		auto normal = parse_point(stl_file); // skipping it for now, not needed.
		auto v1 = parse_point(stl_file);
		auto v2 = parse_point(stl_file);
		auto v3 = parse_point(stl_file);
		vertices.insert(vertices.end(), {v1, v2, v3});
		char dummy[2];
		stl_file.read(dummy, 2);
	}
	return std::make_pair(std::move(vertices), std::move(faces));
}
