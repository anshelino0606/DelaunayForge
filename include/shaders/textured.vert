#version 330 core

layout(location = 0) in vec3 aPos;     // object‐space position
layout(location = 1) in vec3 aNormal;  // object‐space normal

out vec3 FragPos;   // will be forwarded (world‐space) 
out vec3 Normal;    // will be forwarded (world‐space)

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    // Compute world‐space position:
    vec4 worldPos = model * vec4(aPos, 1.0);
    FragPos = worldPos.xyz;

    // Transform normal correctly into world‐space:
    Normal = mat3(transpose(inverse(model))) * aNormal;

    // Project to clip space:
    gl_Position = projection * view * worldPos;
}
