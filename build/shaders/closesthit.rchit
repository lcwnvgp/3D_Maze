#version 460

#extension GL_EXT_ray_tracing : require

struct Sphere {
    vec3 center;
    float radius;
    vec3 color;
};

layout(binding = 3) uniform SceneData {
    int numSpheres;
    Sphere spheres[10];
} scene;

layout(binding = 4) uniform Light {
    vec3 position;
    vec3 color;
    float intensity;
} light;

layout(location = 0) rayPayloadInEXT vec3 hitValue;
hitAttributeEXT vec3 hitNormal;

void main() {
    // 获取击中点的法线
    vec3 normal = normalize(hitNormal);
    
    // 计算光照方向
    vec3 lightDir = normalize(light.position - gl_WorldRayOriginEXT);
    
    // 计算漫反射
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * light.color * light.intensity;
    
    // 计算环境光
    vec3 ambient = vec3(0.1) * light.color;
    
    // 获取球体颜色
    int sphereIndex = gl_PrimitiveID;
    vec3 sphereColor = scene.spheres[sphereIndex].color;
    
    // 计算最终颜色
    hitValue = (ambient + diffuse) * sphereColor;
} 