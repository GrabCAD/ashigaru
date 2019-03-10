#version 330 core

in vec2 UV;
uniform sampler2D tex1, tex2;

layout(location = 0) out vec3 color;
layout(location = 1) out vec3 id;

void main() {
    float val1 = texture(tex1, UV).r;
    float val2 = texture(tex2, UV).r;
	color.r = min( val1, val2 );
	
	float st = step(0, val2 - val1);
	id.r = 0.45*st;
}
