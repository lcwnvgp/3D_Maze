#include "pipeline.h"
#include "shader.h"

Pipeline::Pipeline(VkDevice device, VkRenderPass renderPass)
    : m_device(device), m_renderPass(renderPass) {
    createPipeline();
}

Pipeline::~Pipeline() {
    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_pipeline, nullptr);
    }
}

void Pipeline::createPipeline() {
    Shader vertexShader(m_device, "vertex_shader.spv", VK_SHADER_STAGE_VERTEX_BIT);
    Shader fragmentShader(m_device, "fragment_shader.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    VkPipelineShaderStageCreateInfo shaderStages[] = {
        vertexShader.getShaderStageCreateInfo(),
        fragmentShader.getShaderStageCreateInfo()
    };

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.stageCount = 2;
    pipelineCreateInfo.pStages = shaderStages;
    pipelineCreateInfo.renderPass = m_renderPass;

    if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_pipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline");
    }
}
