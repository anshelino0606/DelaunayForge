#version 330 core

in vec3 geoNormal; // Interpolated normal from geometry shader
out vec4 FragColor;

// Function to map a normal component to a rainbow gradient
vec3 rainbowColor(vec3 normal) {
    // Map the normal vector from [-1, 1] to [0, 1]
    vec3 n = normalize(normal) * 0.5 + 0.5;

    // Combine components to create a rainbow effect
    return vec3(n.x, n.y, n.z);
}

void main()
{
    vec3 color = rainbowColor(geoNormal);
    FragColor = vec4(color, 1.0);
}
