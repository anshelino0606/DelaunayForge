#version 330 core

in vec3 gsPosition;
in vec3 gsNormal;

out vec4 FragColor;

void main()
{
    vec3 normalizedNormal = normalize(gsNormal);
    vec3 color = normalizedNormal * 0.5 + 0.5; // Convert normal to color
    FragColor = vec4(color, 1.0);
}
