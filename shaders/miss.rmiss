#version 460

#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 hitValue;

void main() {
    // 设置背景色
    hitValue = vec3(0.0, 0.0, 0.0);
} 