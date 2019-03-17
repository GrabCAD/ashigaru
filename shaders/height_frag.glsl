#version 330 core

uniform uint shellID;
out vec4 color;

void main(){
    color = vec4(float(shellID + 1u)*0.3, 0, 0, 1);
}
