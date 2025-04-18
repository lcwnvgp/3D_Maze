#include "shader.h"
#include <fstream>
#include <stdexcept>

Shader::Shader(VkDevice device, const std::string& filepath, VkShaderStageFlagBits stage)
    : m_device(device), m_stage(stage) {

    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open shader file");
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = buffer.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

    if (vkCreateShaderModule(m_device, &createInfo, nullptr, &m_shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module");
    }
}

Shader::~Shader() {
    if (m_shaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(m_device, m_shaderModule, nullptr);
    }
}

VkPipelineShaderStageCreateInfo Shader::getShaderStageCreateInfo() const {
    VkPipelineShaderStageCreateInfo stageInfo = {};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = m_stage;
    stageInfo.module = m_shaderModule;
    stageInfo.pName = "main";
    return stageInfo;
}
