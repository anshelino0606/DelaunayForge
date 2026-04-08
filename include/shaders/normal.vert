#version 330 core

layout(location = 0) in vec3 aPos;    // Vertex position
layout(location = 1) in vec3 aNormal; // Vertex normal

out vec3 fragNormal;   // Pass normal to geometry shader
out vec3 fragPosition; // Pass position to geometry shader

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    fragPosition = vec3(model * vec4(aPos, 1.0)); // Transform to world coordinates
    fragNormal = mat3(transpose(inverse(model))) * aNormal; // Correctly transform normals
    gl_Position = projection * view * vec4(fragPosition, 1.0);
}
