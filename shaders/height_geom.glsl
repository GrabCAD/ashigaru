#version 330 core

layout (triangles) in;
layout (triangle_strip, max_vertices=6) out;

flat in uint shellID[];
flat out uint shellID_out;

void main()
{
    // Pass-through.
    if (gl_in[0].gl_Position.z >= 0 || gl_in[1].gl_Position.z >= 0 || gl_in[2].gl_Position.z >= 0)
    {
        for (int i = 0; i < 3; i++) {
            gl_Position = gl_in[i].gl_Position;
            shellID_out = shellID[i];
            EmitVertex();
        }
        EndPrimitive();
    }
    
    // Mirror the faces below slice height. 
    if (gl_in[0].gl_Position.z < 0 || gl_in[1].gl_Position.z < 0 || gl_in[2].gl_Position.z < 0)
    {
        for (int i = 0; i < 3; i++) {
            gl_Position = vec4( gl_in[i].gl_Position.xy, -gl_in[i].gl_Position.z, 1.);
            shellID_out = shellID[i];
            EmitVertex();
        }
        EndPrimitive();
    }
}
