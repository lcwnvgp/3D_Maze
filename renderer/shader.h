#ifndef SHADER_H
#define SHADER_H

#include <vulkan/vulkan.h>
#include <string>

class Shader {
public:
    Shader(VkDevice device, const std::string& filepath, VkShaderStageFlagBits stage);
    ~Shader();

    VkShaderModule getShaderModule() const { return m_shaderModule; }
    VkPipelineShaderStageCreateInfo getShaderStageCreateInfo() const;

private:
    VkDevice m_device;
    VkShaderModule m_shaderModule;
    VkShaderStageFlagBits m_stage;
};

#endif
