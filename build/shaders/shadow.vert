#version 450

layout(location = 0) in vec3 inPosition;

layout(binding = 0) uniform UniformBufferObject {
    mat4 lightSpace; // Light space transformation matrix (proj * view from light)
} ubo;

layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

void main() {
    gl_Position = ubo.lightSpace * pc.model * vec4(inPosition, 1.0);
} 