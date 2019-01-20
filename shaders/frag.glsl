#version 330 core

flat in uint shellID;
out vec4 color;

void main(){
        if (shellID == 0u)
            color = vec4(0.5, 0, 0, 1);
        else 
            color = vec4(1, 0, 0, 1);
}
