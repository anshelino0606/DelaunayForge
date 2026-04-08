#version 330 core

layout(triangles) in;
layout(line_strip, max_vertices = 6) out;

in vec3 fragNormal[];   // Normals from vertex shader
in vec3 fragPosition[]; // Positions from vertex shader

out vec3 geoNormal; // Pass normal to fragment shader

void main()
{
    for (int i = 0; i < 3; ++i)
    {
        // Start vertex of the edge
        gl_Position = gl_in[i].gl_Position;
        geoNormal = fragNormal[i]; // Forward the normal
        EmitVertex();

        // End vertex of the edge
        gl_Position = gl_in[(i + 1) % 3].gl_Position;
        geoNormal = fragNormal[(i + 1) % 3]; // Forward the normal
        EmitVertex();

        EndPrimitive(); // Ends the line primitive
    }
}
