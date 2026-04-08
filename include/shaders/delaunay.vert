#version 330 core

layout(location = 0) in vec3 aPos;    // Vertex position
layout(location = 1) in vec3 aNormal; // Normal vector for lighting calculations

out vec3 fragPosition; // Pass world position to geometry shader
out vec3 fragNormal;   // Pass transformed normal to geometry shader

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    fragPosition = vec3(model * vec4(aPos, 1.0)); // Transform to world coordinates
    fragNormal = normalize(mat3(transpose(inverse(model))) * aNormal);
    gl_Position = projection * view * vec4(fragPosition, 1.0); // Final vertex position
}
