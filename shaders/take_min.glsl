#version 330 core

in vec2 UV;
uniform sampler2D tex1, tex2;

out vec3 color;

void main(){
	color.r = min( texture(tex1, UV), texture(tex2, UV) );
}
