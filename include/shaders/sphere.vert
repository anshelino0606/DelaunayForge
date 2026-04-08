#version 330 core

in vec3 aPos;         // Position of sphere vertices
in vec3 instancePos;  // Position of each sphere instance

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    vec4 worldPosition = model * vec4(aPos, 1.0);
    worldPosition.xyz += instancePos;

    gl_Position = projection * view * worldPosition;
}
