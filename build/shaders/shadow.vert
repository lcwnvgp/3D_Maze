#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(binding = 1) uniform LightUniformBufferObject {
    vec4 lightPos;
    vec4 lightColor;
    float ambientStrength;
} lightUbo;

layout(binding = 2) uniform ShadowUniformBufferObject {
    mat4 lightSpaceMatrix;
} shadowUbo;

void main() {
    gl_Position = shadowUbo.lightSpaceMatrix * ubo.model * vec4(inPosition, 1.0);
} 