#version 330 core

layout(location = 0) in vec3 vertexPosition_modelspace;
layout(location = 1) in uint vertex_shellID;

uniform mat4 projection;

flat out uint shellID;

void main()
{
	gl_Position = projection*vec4(vertexPosition_modelspace, 1);
	shellID = vertex_shellID;
}
