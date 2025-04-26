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

layout(binding = 2) uniform sampler2D shadowMap;

layout(binding = 3) uniform ShadowUniformBufferObject {
    mat4 lightSpace;
} shadow;

float calculateShadow(vec4 fragPosLightSpace) {
    // 执行透视除法
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // 变换到[0,1]范围
    projCoords = projCoords * 0.5 + 0.5;
    // 获取当前片段在光源视角下的深度
    float currentDepth = projCoords.z;
    // 获取阴影贴图中的深度
    float closestDepth = texture(shadowMap, projCoords.xy).r;
    // 检查当前片段是否在阴影中
    float shadow = currentDepth > closestDepth ? 1.0 : 0.0;
    return shadow;
}

void main() {
    vec3 normal = normalize(fragNormal);
    vec3 lightDir = normalize(fragPos - light.lightPos.xyz);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * light.lightColor.xyz;
    vec3 ambient = light.ambientStrength * light.lightColor.xyz;
    
    // 计算阴影
    vec4 fragPosLightSpace = shadow.lightSpace * vec4(fragPos, 1.0);
    float shadow = calculateShadow(fragPosLightSpace);
    
    vec3 objectColor = vec3(0.5, 0.5, 0.5);
    vec3 result = (ambient + (1.0 - shadow) * diffuse) * objectColor;
    outColor = vec4(result, 1.0);
}
