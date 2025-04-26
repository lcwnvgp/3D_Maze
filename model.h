#pragma once

#include "Types.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

class Model {
public:
    Model(VkDevice device, VkPhysicalDevice physicalDevice, 
          VkCommandPool commandPool, VkQueue graphicsQueue,
          const std::string& modelPath);
    ~Model();

    void draw(VkCommandBuffer commandBuffer);
    void updateUniformBuffer(const glm::mat4& view, const glm::mat4& projection, const glm::mat4& model);

    const std::vector<Vertex>& GetVertices() const { return vertices; }
    const std::vector<uint32_t>& GetIndices() const { return indices; }

    VkBuffer getVertexBuffer() const { return vertexBuffer; }
    VkBuffer getIndexBuffer() const { return indexBuffer; }
    glm::vec3 getPosition() const { return position; }
    glm::vec3 getScale() const { return scale; }

private:
    void loadModel(const std::string& modelPath);
    void createVertexBuffer(VkPhysicalDevice physicalDevice);
    void createBuffer(VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                     VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkCommandPool commandPool;
    VkQueue graphicsQueue;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    VkBuffer uniformBuffer;
    VkDeviceMemory uniformBufferMemory;
    void* uniformBufferMapped;

    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
};
