#version 330 core

layout (lines) in;
layout (line_strip, max_vertices = 2) out;

void main() {
    // Emit vertices for each line segment
    for (int i = 0; i < gl_in.length(); i += 2) {
        if (gl_in.length() >= 2) {
            // Ensure we have at least 2 vertices to form a line segment
            gl_Position = gl_in[i].gl_Position;
            EmitVertex();
            gl_Position = gl_in[i + 1].gl_Position;
            EmitVertex();
            EndPrimitive();
        }
    }
}
