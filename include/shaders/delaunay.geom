#version 330 core

layout(triangles) in; // Input primitive: triangles
layout(triangle_strip, max_vertices = 3) out; // Output primitive: triangle strip

in vec3 fragPosition[]; // Input from vertex shader
in vec3 fragNormal[];   // Input from vertex shader

out vec3 geoFragPosition; // Pass to fragment shader
out vec3 geoFragNormal;   // Pass to fragment shader

void main()
{
    for (int i = 0; i < 3; ++i) // Loop through the vertices of the triangle
    {
        geoFragPosition = fragPosition[i]; // Forward world position
        geoFragNormal = fragNormal[i];     // Forward normal
        gl_Position = gl_in[i].gl_Position; // Forward position
        EmitVertex();
    }
    EndPrimitive(); // End the triangle primitive
}
