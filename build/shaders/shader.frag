#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragPos;
layout(location = 2) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

layout(binding = 1) uniform LightUniformBufferObject {
    vec4 lightPos;
    vec4 lightColor;
    float ambientStrength;
} light;

void main() {
    vec3 normal = normalize(fragNormal);
    vec3 lightDir = normalize(light.lightPos.xyz - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * light.lightColor.xyz;
    vec3 ambient = light.ambientStrength * light.lightColor.xyz;
    
    vec3 objectColor = vec3(0.5, 0.5, 0.5);
    vec3 result = (ambient + diffuse) * objectColor;
    outColor = vec4(result, 1.0);
}
