#version 330 core

in vec3 gs_FragPos;  // world‐space position (as passed from geom shader)
in vec3 gs_Normal;   // world‐space normal   (as passed from geom shader)

out vec4 FragColor;

uniform vec3 lightPos;       // world‐space light position
uniform vec3 viewPos;        // world‐space camera position
uniform vec3 lightColor;     // e.g. vec3(1.0,1.0,1.0)
uniform vec3 baseColor;      // e.g. vec3(0.8,0.7,0.6)

uniform float ambientStrength;    // e.g. 0.1
uniform float diffuseStrength;    // e.g. 1.0
uniform float specularStrength;   // e.g. 0.5
uniform float shininess;          // e.g. 32.0

// Simple 3D “checker” pattern based on world position
float proceduralPattern(vec3 pos)
{
    float scale = 10.0;
    float s = sin(pos.x * scale) * sin(pos.y * scale) * sin(pos.z * scale);
    return (s > 0.0) ? 1.0 : 0.5;
}

void main()
{
    // Ambient
    vec3 ambient = ambientStrength * lightColor;

    // Diffuse
    vec3 norm = normalize(gs_Normal);
    vec3 lightDir = normalize(lightPos - gs_FragPos);
    float diffFactor = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diffuseStrength * diffFactor * lightColor;

    // Specular (Blinn‐Phong)
    vec3 viewDir = normalize(viewPos - gs_FragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float specFactor = pow(max(dot(norm, halfwayDir), 0.0), shininess);
    vec3 specular = specularStrength * specFactor * lightColor;

    // Procedural “texture” factor
    float pattern = proceduralPattern(gs_FragPos);
    vec3 texturedColor = baseColor * pattern;

    // Combine
    vec3 result = (ambient + diffuse + specular) * texturedColor;
    FragColor = vec4(result, 1.0);
}
