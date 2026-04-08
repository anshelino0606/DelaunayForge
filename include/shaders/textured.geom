#version 330 core

// 1) Input layout: we take in triangles from the vertex shader
layout(triangles) in;

// 2) Output layout: we re‐emit triangles
layout(triangle_strip, max_vertices = 3) out;

// 3) Declare the per‐vertex inputs (from the vertex shader)
in vec3 FragPos[];  // world‐space position, per‐vertex
in vec3 Normal[];   // world‐space normal,   per‐vertex

// 4) Declare the per‐vertex outputs (to the fragment shader)
out vec3 gs_FragPos;  // to be consumed by fragment shader
out vec3 gs_Normal;   // to be consumed by fragment shader

void main()
{
    // For each of the three vertices of the incoming triangle:
    for(int i = 0; i < 3; ++i)
    {
        // Pass through the world‐space position and normal
        gs_FragPos  = FragPos[i];
        gs_Normal   = Normal[i];

        // Copy the clip‐space position so fragment shader sees the same projection:
        gl_Position = gl_in[i].gl_Position;

        EmitVertex();
    }

    // Finish this triangle strip
    EndPrimitive();
}
