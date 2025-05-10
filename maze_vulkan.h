#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_win32.h>
#include <vulkan/vulkan.hpp>
#include "shaders/host_device.h"
#include "obj_loader.h"
#include <vector>
#include <algorithm>
#include <glm/glm.hpp>

float calculateImpulse(
        float massA, float massB,
        const glm::vec3& velocityA, const glm::vec3& velocityB,
        const glm::vec3& normal, // normal pointing from A to B
        float restitution = 1f // coefficient of restitution, 1 = elastic, 0 = inelastic
);


class AppBase {
public:
    virtual void setup(const VkInstance& instance, const VkDevice& device, const VkPhysicalDevice& physicalDevice, uint32_t queueFamily) = 0;
    virtual void onResize(int width, int height) = 0;
    virtual void destroy() = 0;
    
    VkDevice m_device;
    VkPhysicalDevice m_physicalDevice;
    VkInstance m_instance;
    uint32_t m_graphicsQueueIndex;
    VkExtent2D m_size;
    
    VkSurfaceKHR getVkSurface(VkInstance instance, GLFWwindow* window);
    void createSwapchain(VkSurfaceKHR surface, int width, int height);
    void createDepthBuffer();
    void createRenderPass();
    void createFrameBuffers();
    void initGUI(int minImageCount);
    void setupGlfwCallbacks(GLFWwindow* window);
    bool isMinimized();
    void prepareFrame();
    uint32_t getCurFrame();
    std::vector<VkCommandBuffer>& getCommandBuffers();
    VkExtent2D getSize();
    void submitFrame();
};

class MazeVulkan : public AppBase
{
public:
  void setup(const VkInstance& instance, const VkDevice& device, const VkPhysicalDevice& physicalDevice, uint32_t queueFamily) override;
  void createDescriptorSetLayout();
  void createGraphicsPipeline();
  void loadModel(const std::string& filename, glm::mat4 transform = glm::mat4(1));
  void updateDescriptorSet();
  void createUniformBuffer();
  void createObjDescriptionBuffer();
  void createTextureImages(const VkCommandBuffer& cmdBuf, const std::vector<std::string>& textures);
  void updateUniformBuffer(const VkCommandBuffer& cmdBuf);
  void onResize(int , int ) override;
  void destroyResources();
  void rasterize(const VkCommandBuffer& cmdBuff);

  struct ObjModel
  {
    uint32_t     nbIndices{0};
    uint32_t     nbVertices{0};
    vk::Buffer vertexBuffer;    
    vk::Buffer indexBuffer;     
    vk::Buffer matColorBuffer;  
    vk::Buffer matIndexBuffer;  
    std::vector<VertexObj> hostVertices;
    std::vector<uint32_t>  hostIndices;
  };

  struct ObjInstance
  {
    glm::mat4 transform;    
    uint32_t  objIndex{0};  
  };

  struct Triangle { glm::vec3 a, b, c; };
  std::vector<Triangle> mazeTris;
  std::vector<Triangle> springTris;
  std::vector<Triangle> shieldTris;
  std::vector<Triangle> fanTris;

  PushConstantRaster m_pcRaster{
      {5, 0, 0, 0, 0, 5, 0, 0, 0, 0, 5, 0, 0, 0, 0, 1},  
      {0.f, 20.f, 0.f},                                 
      0,                                                 
      100.f,                                             
      0                                                  
  };

  std::vector<ObjModel>    m_objModel;   
  std::vector<ObjDesc>     m_objDesc;    
  std::vector<ObjInstance> m_instances;  


  VkPipelineLayout            m_pipelineLayout;
  VkPipeline                  m_graphicsPipeline;
  vk::DescriptorSetBindings m_descSetLayoutBind;
  VkDescriptorPool            m_descPool;
  VkDescriptorSetLayout       m_descSetLayout;
  VkDescriptorSet             m_descSet;

  vk::Buffer m_bGlobals;  
  vk::Buffer m_bObjDesc;  

  std::vector<vk::Texture> m_textures;  


  vk::ResourceAllocatorDma m_alloc;  
  vk::DebugUtil            m_debug;  

  glm::vec3 closestPointTriangle(const glm::vec3& P, const glm::vec3& A, const glm::vec3& B, const glm::vec3& C);

  void createOffscreenRender();
  void createPostPipeline();
  void createPostDescriptor();
  void updatePostDescriptorSet();
  void drawPost(VkCommandBuffer cmdBuf);

  vk::DescriptorSetBindings m_postDescSetLayoutBind;
  VkDescriptorPool            m_postDescPool{VK_NULL_HANDLE};
  VkDescriptorSetLayout       m_postDescSetLayout{VK_NULL_HANDLE};
  VkDescriptorSet             m_postDescSet{VK_NULL_HANDLE};
  VkPipeline                  m_postPipeline{VK_NULL_HANDLE};
  VkPipelineLayout            m_postPipelineLayout{VK_NULL_HANDLE};
  VkRenderPass                m_offscreenRenderPass{VK_NULL_HANDLE};
  VkFramebuffer               m_offscreenFramebuffer{VK_NULL_HANDLE};
  vk::Texture               m_offscreenColor;
  vk::Texture               m_offscreenDepth;
  VkFormat                    m_offscreenColorFormat{VK_FORMAT_R32G32B32A32_SFLOAT};
  VkFormat                    m_offscreenDepthFormat{VK_FORMAT_X8_D24_UNORM_PACK32};

  void initRayTracing();
  auto objectToVkGeometryKHR(const ObjModel& model);
  void createBottomLevelAS();
  void createTopLevelAS();
  void createRtDescriptorSet();
  void updateRtDescriptorSet();
  void createRtPipeline();
  void createRtShaderBindingTable();
  void raytrace(const VkCommandBuffer& cmdBuf, const glm::vec4& clearColor);

  void handleCollision(const glm::vec3& next, const glm::mat4& transform, glm::vec3& ballPos, glm::vec3& ballVel, 
                      float ballRadius, const std::vector<Triangle>& triangles, float restitution = 0.15f, float friction = 0.05f);

  VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rtProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
  vk::RaytracingBuilderKHR                        m_rtBuilder;
  vk::DescriptorSetBindings                       m_rtDescSetLayoutBind;
  VkDescriptorPool                                  m_rtDescPool;
  VkDescriptorSetLayout                             m_rtDescSetLayout;
  VkDescriptorSet                                   m_rtDescSet;
  std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_rtShaderGroups;
  VkPipelineLayout                                  m_rtPipelineLayout;
  VkPipeline                                        m_rtPipeline;

  vk::Buffer                    m_rtSBTBuffer;
  VkStridedDeviceAddressRegionKHR m_rgenRegion{};
  VkStridedDeviceAddressRegionKHR m_missRegion{};
  VkStridedDeviceAddressRegionKHR m_hitRegion{};
  VkStridedDeviceAddressRegionKHR m_callRegion{};

  PushConstantRay m_pcRay{};
};
