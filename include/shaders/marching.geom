#version 330 core

layout(triangles) in;
//layout(line_strip, max_vertices = 6) out; // Emitting 6 vertices for 3 lines
layout(triangle_strip, max_vertices = 3) out; // Emitting 3 vertices for 1 triangle

in vec3 fragPosition[];
in vec3 fragNormal[];

out vec3 gsPosition;
out vec3 gsNormal;

void main()
{
//        for (int i = 0; i < 3; ++i)
//        {
//            // Emit first edge
//            gsPosition = fragPosition[i];
//            gsNormal = fragNormal[i];
//            gl_Position = gl_in[i].gl_Position;
//            EmitVertex();
//
//            gsPosition = fragPosition[(i+1) % 3];
//            gsNormal = fragNormal[(i+1) % 3];
//            gl_Position = gl_in[(i+1) % 3].gl_Position;
//            EmitVertex();
//
//            EndPrimitive(); // End the line primitive
//        }
//     emit for triangle
    for (int i = 0; i < 3; ++i)
    {
        gsPosition = fragPosition[i];
        gsNormal = fragNormal[i];
        gl_Position = gl_in[i].gl_Position;
        EmitVertex();
    }
    EndPrimitive();
}
