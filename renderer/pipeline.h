#ifndef PIPELINE_H
#define PIPELINE_H

#include <vulkan/vulkan.h>

class Pipeline {
public:
    Pipeline(VkDevice device, VkRenderPass renderPass);
    ~Pipeline();

    VkPipeline getPipeline() const { return m_pipeline; }

private:
    VkDevice m_device;
    VkPipeline m_pipeline;
    VkRenderPass m_renderPass;

    void createPipeline();
};

#endif
