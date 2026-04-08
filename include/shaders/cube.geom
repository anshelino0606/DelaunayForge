#version 330 core
layout(triangles) in;
layout(line_strip, max_vertices = 24) out;

void main()
{
    // Define edges for the cube as a one-dimensional array
    int edges[24] = int[24](
    0, 1, 1, 2, 2, 3, 3, 0, // front face edges
    4, 5, 5, 6, 6, 7, 7, 4, // back face edges
    0, 4, 1, 5, 2, 6, 3, 7  // side edges connecting front and back
    );

    for (int i = 0; i < 24; i += 2)
    {
        // Emit the edge
        gl_Position = gl_in[edges[i]].gl_Position;
        EmitVertex();
        gl_Position = gl_in[edges[i + 1]].gl_Position;
        EmitVertex();
        EndPrimitive();
    }
}