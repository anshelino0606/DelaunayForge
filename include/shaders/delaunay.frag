#version 330 core

in vec3 geoFragPosition; // World position from geometry shader
in vec3 geoFragNormal;   // Normal from geometry shader

out vec4 FragColor;

uniform vec3 lightPos;    // Position of the light source
uniform vec3 viewPos;     // Position of the camera/viewer
uniform vec3 lightColor;  // Color of the light
uniform vec3 objectColor; // Base color of the object

float proceduralPattern(vec3 position) {
    // Smooth gradient pattern using a combination of axes
    return 0.5 + 0.5 * sin(position.x * 5.0 + position.y * 3.0);
}

void main()
{
    // Ambient lighting
    float ambientStrength = 0.2;
    vec3 ambient = ambientStrength * lightColor;

    // Diffuse lighting
    vec3 norm = normalize(geoFragNormal);
    vec3 lightDir = normalize(lightPos - geoFragPosition);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Specular lighting
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - geoFragPosition);
    vec3 reflectDir = reflect(-lightDir, norm);
    float shininess = 32.0;
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specular = specularStrength * spec * lightColor;

    // Combine lighting components
    vec3 lighting = (ambient + diffuse + specular);

    // Apply procedural pattern
    float pattern = proceduralPattern(geoFragPosition);
    vec3 patternColor = mix(objectColor, vec3(1.0, 1.0, 1.0), pattern); // Blend with white

    // Final color output
    vec3 result = lighting * patternColor;
    FragColor = vec4(result, 1.0);
}
