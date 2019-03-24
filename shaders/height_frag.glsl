#version 330 core

flat in uint shellID_out;
out vec4 color;

void main(){
    color = vec4(float(shellID_out + 1u)*0.3, 0, 0, 1);
}
