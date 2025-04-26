#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <windows.h>
#include <iostream>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_win32.h>
#include <vulkan/vulkan_beta.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <string>
#include <array>
#include <set>
#include <optional>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Types.h"
#include "model.h"

// 定义加速结构相关的结构体
typedef struct VkBindAccelerationStructureMemoryInfoKHR {
    VkStructureType    sType;
    const void*        pNext;
    VkAccelerationStructureKHR accelerationStructure;
    VkDeviceMemory     memory;
    VkDeviceSize       memoryOffset;
    uint32_t           deviceIndexCount;
    const uint32_t*    pDeviceIndices;
} VkBindAccelerationStructureMemoryInfoKHR;

#define VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_KHR ((VkStructureType)1000167006)

// 定义光线追踪相关的函数指针类型
typedef VkResult (VKAPI_PTR *PFN_vkBindAccelerationStructureMemoryKHR)(VkDevice device, uint32_t bindInfoCount, const VkBindAccelerationStructureMemoryInfoKHR* pBindInfos);

const uint32_t WINDOW_WIDTH = 800;
const uint32_t WINDOW_HEIGHT = 600;

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

class VulkanContext {
public:
    VulkanContext(GLFWwindow* window = nullptr);
    ~VulkanContext();

    void initWindow(int width, int height, const std::string& title);
    void initVulkan();
    void cleanup();

    void beginFrame();
    void endFrame();
    void drawFrame();

    VkInstance getInstance() const { return instance; }
    VkDevice getDevice() const { return device; }
    VkSurfaceKHR getSurface() const { return surface; }
    VkSwapchainKHR getSwapChain() const { return swapChain; }
    VkQueue getGraphicsQueue() const { return graphicsQueue; }
    VkCommandPool getCommandPool() const { return commandPool; }
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }
    GLFWwindow* getWindow() const { return window; }
    uint32_t getCurrentFrame() const { return currentFrame; }

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, 
                     VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    void setLightPosition(const glm::vec3& position) { lightPosition = position; }
    void setLightColor(const glm::vec3& color) { lightColor = color; }
    void setAmbientStrength(float strength) { ambientStrength = strength; }
    
    glm::vec3 getLightPosition() const { return lightPosition; }
    glm::vec3 getLightColor() const { return lightColor; }
    float getAmbientStrength() const { return ambientStrength; }

    void setViewMatrix(const glm::mat4& view) { viewMatrix = view; }
    void setProjectionMatrix(const glm::mat4& proj) { projectionMatrix = proj; }

    void setModels(const std::vector<Model*>& ms) { models = ms; }

    // 添加 getter 函数
    VkCommandBuffer getCommandBuffer() const { return commandBuffers[currentFrame]; }
    VkPipeline getRayTracingPipeline() const { return rayTracingPipeline; }
    VkPipelineLayout getRayTracingPipelineLayout() const { return rayTracingPipelineLayout; }
    const std::vector<VkDescriptorSet>& getRayTracingDescriptorSets() const { return rayTracingDescriptorSets; }
    VkBuffer getRaygenShaderBindingTable() const { return raygenShaderBindingTable; }
    VkBuffer getMissShaderBindingTable() const { return missShaderBindingTable; }
    VkBuffer getHitShaderBindingTable() const { return hitShaderBindingTable; }
    const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& getRayTracingPipelineProperties() const { return rayTracingPipelineProperties; }
    VkDeviceAddress getBufferDeviceAddress(VkBuffer buffer);

private:
    void createInstance();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapChain();
    void createImageViews();
    void createCommandPool();
    void createSyncObjects();
    uint32_t findQueueFamily(VkPhysicalDevice device);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    VkShaderModule createShaderModule(const std::vector<char>& code);
    std::vector<char> readFile(const std::string& filename);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

    // 光线追踪相关
    void createAccelerationStructure();
    void createStorageImage();
    void createShaderBindingTable();
    void createRayTracingPipeline();
    void createRayTracingDescriptorSetLayout();
    void createRayTracingDescriptorSets();
    void updateRayTracingUniformBuffer(uint32_t currentImage);

    // 多模型支持
    std::vector<Model*> models;
    std::vector<VkAccelerationStructureKHR> bottomLevelASs;
    std::vector<VkBuffer> bottomLevelASBuffers;
    std::vector<VkDeviceMemory> bottomLevelASMemories;

    // 场景 TLAS
    VkAccelerationStructureKHR topLevelAS;
    VkBuffer topLevelASBuffer;
    VkDeviceMemory topLevelASMemory;

    // 光线追踪资源
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties{};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{};
    VkPipeline rayTracingPipeline;
    VkPipelineLayout rayTracingPipelineLayout;
    VkDescriptorSetLayout rayTracingDescriptorSetLayout;
    VkDescriptorPool rayTracingDescriptorPool;
    std::vector<VkDescriptorSet> rayTracingDescriptorSets;
    VkBuffer raygenShaderBindingTable;
    VkBuffer missShaderBindingTable;
    VkBuffer hitShaderBindingTable;
    VkDeviceMemory raygenShaderBindingTableMemory;
    VkDeviceMemory missShaderBindingTableMemory;
    VkDeviceMemory hitShaderBindingTableMemory;
    VkBuffer raygenShaderBindingTableBuffer;
    VkBuffer missShaderBindingTableBuffer;
    VkBuffer hitShaderBindingTableBuffer;
    VkImage storageImage;
    VkDeviceMemory storageImageMemory;
    VkImageView storageImageView;
    std::vector<VkBuffer> rayTracingUniformBuffers;
    std::vector<VkDeviceMemory> rayTracingUniformBuffersMemory;
    std::vector<void*> rayTracingUniformBuffersMapped;

    // 相机、光源等
    glm::mat4 viewMatrix;
    glm::mat4 projectionMatrix;
    glm::vec3 lightPosition = glm::vec3(0.0f, 20.0f, 0.0f); 
    glm::vec3 lightColor = glm::vec3(1.0f);
    float ambientStrength = 0.1f;

    // Vulkan基础设施
    GLFWwindow* window = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    const int MAX_FRAMES_IN_FLIGHT = 2;
    uint32_t currentFrame = 0;
    uint32_t imageIndex;

    // 验证层和扩展
    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
    };
        
    bool enableValidationLayers = true;

    // Helper methods for one-time commands and scratch buffer
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    VkBuffer createScratchBuffer(VkDeviceSize size);
    VkDeviceAddress getAccelerationStructureDeviceAddress(VkAccelerationStructureKHR as);
};
