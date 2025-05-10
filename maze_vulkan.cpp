#include <sstream>

#define STB_IMAGE_IMPLEMENTATION
#include "obj_loader.h"
#include "stb_image.h"

#include "maze_vulkan.h"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_win32.h>
#include <vulkan/vulkan.hpp>

extern std::vector<std::string> defaultSearchPaths;

void MazeVulkan::setup(const VkInstance& instance, const VkDevice& device, const VkPhysicalDevice& physicalDevice, uint32_t queueFamily)
{
  AppBaseVk::setup(instance, device, physicalDevice, queueFamily);
  m_alloc.init(instance, device, physicalDevice);
  m_debug.setup(m_device);
  m_offscreenDepthFormat = vk::findDepthFormat(physicalDevice);
}

void MazeVulkan::updateUniformBuffer(const VkCommandBuffer& cmdBuf)
{
  const float    aspectRatio = m_size.width / static_cast<float>(m_size.height);
  GlobalUniforms hostUBO     = {};
  const auto&    view        = CameraManip.getMatrix();
  glm::mat4      proj        = glm::perspectiveRH_ZO(glm::radians(CameraManip.getFov()), aspectRatio, 0.1f, 1000.0f);
  proj[1][1] *= -1;  

  hostUBO.viewProj    = proj * view;
  hostUBO.viewInverse = glm::inverse(view);
  hostUBO.projInverse = glm::inverse(proj);

  VkBuffer deviceUBO      = m_bGlobals.buffer;
  auto     uboUsageStages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;

  VkBufferMemoryBarrier beforeBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
  beforeBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  beforeBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  beforeBarrier.buffer        = deviceUBO;
  beforeBarrier.offset        = 0;
  beforeBarrier.size          = sizeof(hostUBO);
  vkCmdPipelineBarrier(cmdBuf, uboUsageStages, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_DEVICE_GROUP_BIT, 0,
                       nullptr, 1, &beforeBarrier, 0, nullptr);


  vkCmdUpdateBuffer(cmdBuf, m_bGlobals.buffer, 0, sizeof(GlobalUniforms), &hostUBO);

  VkBufferMemoryBarrier afterBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
  afterBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  afterBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  afterBarrier.buffer        = deviceUBO;
  afterBarrier.offset        = 0;
  afterBarrier.size          = sizeof(hostUBO);
  vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, uboUsageStages, VK_DEPENDENCY_DEVICE_GROUP_BIT, 0,
                       nullptr, 1, &afterBarrier, 0, nullptr);
}

void MazeVulkan::createDescriptorSetLayout()
{
  auto nbTxt = static_cast<uint32_t>(m_textures.size());

  m_descSetLayoutBind.addBinding(SceneBindings::eGlobals, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                 VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR);
  m_descSetLayoutBind.addBinding(SceneBindings::eObjDescs, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                                 VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
  m_descSetLayoutBind.addBinding(SceneBindings::eTextures, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nbTxt,
                                 VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);


  m_descSetLayout = m_descSetLayoutBind.createLayout(m_device);
  m_descPool      = m_descSetLayoutBind.createPool(m_device, 1);
  m_descSet       = vk::allocateDescriptorSet(m_device, m_descPool, m_descSetLayout);
}

void MazeVulkan::updateDescriptorSet()
{
  std::vector<VkWriteDescriptorSet> writes;

  VkDescriptorBufferInfo dbiUnif{m_bGlobals.buffer, 0, VK_WHOLE_SIZE};
  writes.emplace_back(m_descSetLayoutBind.makeWrite(m_descSet, SceneBindings::eGlobals, &dbiUnif));

  VkDescriptorBufferInfo dbiSceneDesc{m_bObjDesc.buffer, 0, VK_WHOLE_SIZE};
  writes.emplace_back(m_descSetLayoutBind.makeWrite(m_descSet, SceneBindings::eObjDescs, &dbiSceneDesc));

  std::vector<VkDescriptorImageInfo> diit;
  for(auto& texture : m_textures)
  {
    diit.emplace_back(texture.descriptor);
  }
  writes.emplace_back(m_descSetLayoutBind.makeWriteArray(m_descSet, SceneBindings::eTextures, diit.data()));

  vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}


void MazeVulkan::createGraphicsPipeline()
{
  VkPushConstantRange pushConstantRanges = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstantRaster)};

  VkPipelineLayoutCreateInfo createInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  createInfo.setLayoutCount         = 1;
  createInfo.pSetLayouts            = &m_descSetLayout;
  createInfo.pushConstantRangeCount = 1;
  createInfo.pPushConstantRanges    = &pushConstantRanges;
  vkCreatePipelineLayout(m_device, &createInfo, nullptr, &m_pipelineLayout);


  std::vector<std::string>                paths = defaultSearchPaths;
  vk::GraphicsPipelineGeneratorCombined gpb(m_device, m_pipelineLayout, m_offscreenRenderPass);
  gpb.depthStencilState.depthTestEnable = true;
  gpb.addShader(loadFile("spv/vert_shader.vert.spv", true, paths, true), VK_SHADER_STAGE_VERTEX_BIT);
  gpb.addShader(loadFile("spv/frag_shader.frag.spv", true, paths, true), VK_SHADER_STAGE_FRAGMENT_BIT);
  gpb.addBindingDescription({0, sizeof(VertexObj)});
  gpb.addAttributeDescriptions({
      {0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(VertexObj, pos))},
      {1, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(VertexObj, nrm))},
      {2, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(VertexObj, color))},
      {3, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<uint32_t>(offsetof(VertexObj, texCoord))},
  });

  m_graphicsPipeline = gpb.createPipeline();
  m_debug.setObjectName(m_graphicsPipeline, "Graphics");
}

void MazeVulkan::loadModel(const std::string& filename, glm::mat4 transform)
{
  LOGI("Loading File:  %s \n", filename.c_str());
  ObjLoader loader;
  loader.loadModel(filename);

  for(auto& m : loader.m_materials)
  {
    m.ambient  = glm::pow(m.ambient, glm::vec3(2.2f));
    m.diffuse  = glm::pow(m.diffuse, glm::vec3(2.2f));
    m.specular = glm::pow(m.specular, glm::vec3(2.2f));
  }

  ObjModel model;
  model.nbIndices  = static_cast<uint32_t>(loader.m_indices.size());
  model.nbVertices = static_cast<uint32_t>(loader.m_vertices.size());
  model.hostVertices = loader.m_vertices;
  model.hostIndices  = loader.m_indices;

  if (filename.find("maze.obj") != std::string::npos) {
    mazeTris.clear();
    for (size_t i = 0; i < model.hostIndices.size(); i += 3) {
      auto &V = model.hostVertices;
      uint32_t i0 = model.hostIndices[i + 0];
      uint32_t i1 = model.hostIndices[i + 1];
      uint32_t i2 = model.hostIndices[i + 2];
      mazeTris.push_back({ V[i0].pos, V[i1].pos, V[i2].pos });
    }
    buildBVH();
  }

  if (filename.find("spring.obj") != std::string::npos) {
    springTris.clear();
    for (size_t i = 0; i < model.hostIndices.size(); i += 3) {
      auto &V = model.hostVertices;
      uint32_t i0 = model.hostIndices[i + 0];
      uint32_t i1 = model.hostIndices[i + 1];
      uint32_t i2 = model.hostIndices[i + 2];
      springTris.push_back({ V[i0].pos, V[i1].pos, V[i2].pos });
    }
  }

  if (filename.find("shield.obj") != std::string::npos) {
    shieldTris.clear();
    for (size_t i = 0; i < model.hostIndices.size(); i += 3) {
      auto &V = model.hostVertices;
      uint32_t i0 = model.hostIndices[i + 0];
      uint32_t i1 = model.hostIndices[i + 1]; 
      uint32_t i2 = model.hostIndices[i + 2]; 
      shieldTris.push_back({ V[i0].pos, V[i1].pos, V[i2].pos });
    }
  }

  if (filename.find("fan.obj") != std::string::npos) {
    fanTris.clear();
    for (size_t i = 0; i < model.hostIndices.size(); i += 3) {
      auto &V = model.hostVertices;
      uint32_t i0 = model.hostIndices[i + 0];
      uint32_t i1 = model.hostIndices[i + 1]; 
      uint32_t i2 = model.hostIndices[i + 2]; 
      fanTris.push_back({ V[i0].pos, V[i1].pos, V[i2].pos });
    }
  } 


  vk::CommandPool  cmdBufGet(m_device, m_graphicsQueueIndex);
  VkCommandBuffer    cmdBuf          = cmdBufGet.createCommandBuffer();
  VkBufferUsageFlags flag            = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
  VkBufferUsageFlags rayTracingFlags =  
      flag | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  model.vertexBuffer = m_alloc.createBuffer(cmdBuf, loader.m_vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | rayTracingFlags);
  model.indexBuffer = m_alloc.createBuffer(cmdBuf, loader.m_indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | rayTracingFlags);
  model.matColorBuffer = m_alloc.createBuffer(cmdBuf, loader.m_materials, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | flag);
  model.matIndexBuffer = m_alloc.createBuffer(cmdBuf, loader.m_matIndx, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | flag);
  auto txtOffset = static_cast<uint32_t>(m_textures.size());
  createTextureImages(cmdBuf, loader.m_textures);
  cmdBufGet.submitAndWait(cmdBuf);
  m_alloc.finalizeAndReleaseStaging();

  std::string objNb = std::to_string(m_objModel.size());
  m_debug.setObjectName(model.vertexBuffer.buffer, (std::string("vertex_" + objNb)));
  m_debug.setObjectName(model.indexBuffer.buffer, (std::string("index_" + objNb)));
  m_debug.setObjectName(model.matColorBuffer.buffer, (std::string("mat_" + objNb)));
  m_debug.setObjectName(model.matIndexBuffer.buffer, (std::string("matIdx_" + objNb)));

  ObjInstance instance;
  instance.transform = transform;
  instance.objIndex  = static_cast<uint32_t>(m_objModel.size());
  m_instances.push_back(instance);

  ObjDesc desc;
  desc.txtOffset            = txtOffset;
  desc.vertexAddress        = vk::getBufferDeviceAddress(m_device, model.vertexBuffer.buffer);
  desc.indexAddress         = vk::getBufferDeviceAddress(m_device, model.indexBuffer.buffer);
  desc.materialAddress      = vk::getBufferDeviceAddress(m_device, model.matColorBuffer.buffer);
  desc.materialIndexAddress = vk::getBufferDeviceAddress(m_device, model.matIndexBuffer.buffer);

  m_objModel.emplace_back(model);
  m_objDesc.emplace_back(desc);
}


void MazeVulkan::createUniformBuffer()
{
  m_bGlobals = m_alloc.createBuffer(sizeof(GlobalUniforms), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  m_debug.setObjectName(m_bGlobals.buffer, "Globals");
}

void MazeVulkan::createObjDescriptionBuffer()
{
  vk::CommandPool cmdGen(m_device, m_graphicsQueueIndex);

  auto cmdBuf = cmdGen.createCommandBuffer();
  m_bObjDesc  = m_alloc.createBuffer(cmdBuf, m_objDesc, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
  cmdGen.submitAndWait(cmdBuf);
  m_alloc.finalizeAndReleaseStaging();
  m_debug.setObjectName(m_bObjDesc.buffer, "ObjDescs");
}

void MazeVulkan::createTextureImages(const VkCommandBuffer& cmdBuf, const std::vector<std::string>& textures)
{
  VkSamplerCreateInfo samplerCreateInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  samplerCreateInfo.minFilter  = VK_FILTER_LINEAR;
  samplerCreateInfo.magFilter  = VK_FILTER_LINEAR;
  samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerCreateInfo.maxLod     = FLT_MAX;

  VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;

  if(textures.empty() && m_textures.empty())
  {
    vk::Texture texture;

    std::array<uint8_t, 4> color{255u, 255u, 255u, 255u};
    VkDeviceSize           bufferSize      = sizeof(color);
    auto                   imgSize         = VkExtent2D{1, 1};
    auto                   imageCreateInfo = vk::makeImage2DCreateInfo(imgSize, format);

    vk::Image           image  = m_alloc.createImage(cmdBuf, bufferSize, color.data(), imageCreateInfo);
    VkImageViewCreateInfo ivInfo = vk::makeImageViewCreateInfo(image.image, imageCreateInfo);
    texture                      = m_alloc.createTexture(image, ivInfo, samplerCreateInfo);

    vk::cmdBarrierImageLayout(cmdBuf, texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    m_textures.push_back(texture);
  }
  else
  {
    for(const auto& texture : textures)
    {
      std::stringstream o;
      int               texWidth, texHeight, texChannels;
      o << "media/textures/" << texture;
      std::string txtFile = findFile(o.str(), defaultSearchPaths, true);

      stbi_uc* stbi_pixels = stbi_load(txtFile.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

      std::array<stbi_uc, 4> color{255u, 0u, 255u, 255u};

      stbi_uc* pixels = stbi_pixels;
      if(!stbi_pixels)
      {
        texWidth = texHeight = 1;
        texChannels          = 4;
        pixels               = reinterpret_cast<stbi_uc*>(color.data());
      }

      VkDeviceSize bufferSize      = static_cast<uint64_t>(texWidth) * texHeight * sizeof(uint8_t) * 4;
      auto         imgSize         = VkExtent2D{(uint32_t)texWidth, (uint32_t)texHeight};
      auto         imageCreateInfo = vk::makeImage2DCreateInfo(imgSize, format, VK_IMAGE_USAGE_SAMPLED_BIT, true);

      {
        vk::Image image = m_alloc.createImage(cmdBuf, bufferSize, pixels, imageCreateInfo);
        vk::cmdGenerateMipmaps(cmdBuf, image.image, format, imgSize, imageCreateInfo.mipLevels);
        VkImageViewCreateInfo ivInfo  = vk::makeImageViewCreateInfo(image.image, imageCreateInfo);
        vk::Texture         texture = m_alloc.createTexture(image, ivInfo, samplerCreateInfo);

        m_textures.push_back(texture);
      }

      stbi_image_free(stbi_pixels);
    }
  }
}

void MazeVulkan::destroyResources()
{
  bvhNodes.clear();
  primitives.clear();
  
  vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
  vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
  vkDestroyDescriptorPool(m_device, m_descPool, nullptr);
  vkDestroyDescriptorSetLayout(m_device, m_descSetLayout, nullptr);

  m_alloc.destroy(m_bGlobals);
  m_alloc.destroy(m_bObjDesc);

  for(auto& m : m_objModel)
  {
    m_alloc.destroy(m.vertexBuffer);
    m_alloc.destroy(m.indexBuffer);
    m_alloc.destroy(m.matColorBuffer);
    m_alloc.destroy(m.matIndexBuffer);
  }

  for(auto& t : m_textures)
  {
    m_alloc.destroy(t);
  }

  m_alloc.destroy(m_offscreenColor);
  m_alloc.destroy(m_offscreenDepth);
  vkDestroyPipeline(m_device, m_postPipeline, nullptr);
  vkDestroyPipelineLayout(m_device, m_postPipelineLayout, nullptr);
  vkDestroyDescriptorPool(m_device, m_postDescPool, nullptr);
  vkDestroyDescriptorSetLayout(m_device, m_postDescSetLayout, nullptr);
  vkDestroyRenderPass(m_device, m_offscreenRenderPass, nullptr);
  vkDestroyFramebuffer(m_device, m_offscreenFramebuffer, nullptr);


  m_rtBuilder.destroy();
  vkDestroyPipeline(m_device, m_rtPipeline, nullptr);
  vkDestroyPipelineLayout(m_device, m_rtPipelineLayout, nullptr);
  vkDestroyDescriptorPool(m_device, m_rtDescPool, nullptr);
  vkDestroyDescriptorSetLayout(m_device, m_rtDescSetLayout, nullptr);
  m_alloc.destroy(m_rtSBTBuffer);

  m_alloc.deinit();
}

void MazeVulkan::rasterize(const VkCommandBuffer& cmdBuf)
{
  VkDeviceSize offset{0};

  m_debug.beginLabel(cmdBuf, "Rasterize");

  setViewport(cmdBuf);

  vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
  vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descSet, 0, nullptr);


  for(const MazeVulkan::ObjInstance& inst : m_instances)
  {
    auto& model            = m_objModel[inst.objIndex];
    m_pcRaster.objIndex    = inst.objIndex;  
    m_pcRaster.modelMatrix = inst.transform;

    vkCmdPushConstants(cmdBuf, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(PushConstantRaster), &m_pcRaster);
    vkCmdBindVertexBuffers(cmdBuf, 0, 1, &model.vertexBuffer.buffer, &offset);
    vkCmdBindIndexBuffer(cmdBuf, model.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmdBuf, model.nbIndices, 1, 0, 0, 0);
  }
  m_debug.endLabel(cmdBuf);
}

void MazeVulkan::onResize(int , int )
{
  createOffscreenRender();
  updatePostDescriptorSet();
  updateRtDescriptorSet();
}




void MazeVulkan::createOffscreenRender()
{
  m_alloc.destroy(m_offscreenColor);
  m_alloc.destroy(m_offscreenDepth);

  {
    auto colorCreateInfo = vk::makeImage2DCreateInfo(m_size, m_offscreenColorFormat,
                                                       VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                                                           | VK_IMAGE_USAGE_STORAGE_BIT);


    vk::Image           image  = m_alloc.createImage(colorCreateInfo);
    VkImageViewCreateInfo ivInfo = vk::makeImageViewCreateInfo(image.image, colorCreateInfo);
    VkSamplerCreateInfo   sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    m_offscreenColor                        = m_alloc.createTexture(image, ivInfo, sampler);
    m_offscreenColor.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  }

  auto depthCreateInfo = vk::makeImage2DCreateInfo(m_size, m_offscreenDepthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
  {
    vk::Image image = m_alloc.createImage(depthCreateInfo);


    VkImageViewCreateInfo depthStencilView{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    depthStencilView.viewType         = VK_IMAGE_VIEW_TYPE_2D;
    depthStencilView.format           = m_offscreenDepthFormat;
    depthStencilView.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    depthStencilView.image            = image.image;

    m_offscreenDepth = m_alloc.createTexture(image, depthStencilView);
  }

  {
    vk::CommandPool genCmdBuf(m_device, m_graphicsQueueIndex);
    auto              cmdBuf = genCmdBuf.createCommandBuffer();
    vk::cmdBarrierImageLayout(cmdBuf, m_offscreenColor.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    vk::cmdBarrierImageLayout(cmdBuf, m_offscreenDepth.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

    genCmdBuf.submitAndWait(cmdBuf);
  }

  if(!m_offscreenRenderPass)
  {
    m_offscreenRenderPass = vk::createRenderPass(m_device, {m_offscreenColorFormat}, m_offscreenDepthFormat, 1, true,
                                                   true, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL);
  }


  std::vector<VkImageView> attachments = {m_offscreenColor.descriptor.imageView, m_offscreenDepth.descriptor.imageView};

  vkDestroyFramebuffer(m_device, m_offscreenFramebuffer, nullptr);
  VkFramebufferCreateInfo info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
  info.renderPass      = m_offscreenRenderPass;
  info.attachmentCount = 2;
  info.pAttachments    = attachments.data();
  info.width           = m_size.width;
  info.height          = m_size.height;
  info.layers          = 1;
  vkCreateFramebuffer(m_device, &info, nullptr, &m_offscreenFramebuffer);
}

void MazeVulkan::createPostPipeline()
{
  VkPushConstantRange pushConstantRanges = {VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float)};

  VkPipelineLayoutCreateInfo createInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  createInfo.setLayoutCount         = 1;
  createInfo.pSetLayouts            = &m_postDescSetLayout;
  createInfo.pushConstantRangeCount = 1;
  createInfo.pPushConstantRanges    = &pushConstantRanges;
  vkCreatePipelineLayout(m_device, &createInfo, nullptr, &m_postPipelineLayout);


  vk::GraphicsPipelineGeneratorCombined pipelineGenerator(m_device, m_postPipelineLayout, m_renderPass);
  pipelineGenerator.addShader(loadFile("spv/passthrough.vert.spv", true, defaultSearchPaths, true), VK_SHADER_STAGE_VERTEX_BIT);
  pipelineGenerator.addShader(loadFile("spv/post.frag.spv", true, defaultSearchPaths, true), VK_SHADER_STAGE_FRAGMENT_BIT);
  pipelineGenerator.rasterizationState.cullMode = VK_CULL_MODE_NONE;
  m_postPipeline                                = pipelineGenerator.createPipeline();
  m_debug.setObjectName(m_postPipeline, "post");
}

void MazeVulkan::createPostDescriptor()
{
  m_postDescSetLayoutBind.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
  m_postDescSetLayout = m_postDescSetLayoutBind.createLayout(m_device);
  m_postDescPool      = m_postDescSetLayoutBind.createPool(m_device);
  m_postDescSet       = vk::allocateDescriptorSet(m_device, m_postDescPool, m_postDescSetLayout);
}


void MazeVulkan::updatePostDescriptorSet()
{
  VkWriteDescriptorSet writeDescriptorSets = m_postDescSetLayoutBind.makeWrite(m_postDescSet, 0, &m_offscreenColor.descriptor);
  vkUpdateDescriptorSets(m_device, 1, &writeDescriptorSets, 0, nullptr);
}

void MazeVulkan::drawPost(VkCommandBuffer cmdBuf)
{
  m_debug.beginLabel(cmdBuf, "Post");

  setViewport(cmdBuf);

  auto aspectRatio = static_cast<float>(m_size.width) / static_cast<float>(m_size.height);
  vkCmdPushConstants(cmdBuf, m_postPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float), &aspectRatio);
  vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_postPipeline);
  vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_postPipelineLayout, 0, 1, &m_postDescSet, 0, nullptr);
  vkCmdDraw(cmdBuf, 3, 1, 0, 0);

  m_debug.endLabel(cmdBuf);
}


void MazeVulkan::initRayTracing()
{
  VkPhysicalDeviceProperties2 prop2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
  prop2.pNext = &m_rtProperties;
  vkGetPhysicalDeviceProperties2(m_physicalDevice, &prop2);

  m_rtBuilder.setup(m_device, &m_alloc, m_graphicsQueueIndex);
}

auto MazeVulkan::objectToVkGeometryKHR(const ObjModel& model)
{
  VkDeviceAddress vertexAddress = vk::getBufferDeviceAddress(m_device, model.vertexBuffer.buffer);
  VkDeviceAddress indexAddress  = vk::getBufferDeviceAddress(m_device, model.indexBuffer.buffer);

  uint32_t maxPrimitiveCount = model.nbIndices / 3;

  VkAccelerationStructureGeometryTrianglesDataKHR triangles{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
  triangles.vertexFormat             = VK_FORMAT_R32G32B32_SFLOAT;  
  triangles.vertexData.deviceAddress = vertexAddress;
  triangles.vertexStride             = sizeof(VertexObj);
  triangles.indexType               = VK_INDEX_TYPE_UINT32;
  triangles.indexData.deviceAddress = indexAddress;
  triangles.maxVertex = model.nbVertices - 1;

  VkAccelerationStructureGeometryKHR asGeom{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
  asGeom.geometryType       = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
  asGeom.flags              = VK_GEOMETRY_OPAQUE_BIT_KHR;
  asGeom.geometry.triangles = triangles;

  VkAccelerationStructureBuildRangeInfoKHR offset;
  offset.firstVertex     = 0;
  offset.primitiveCount  = maxPrimitiveCount;
  offset.primitiveOffset = 0;
  offset.transformOffset = 0;

  vk::RaytracingBuilderKHR::BlasInput input;
  input.asGeometry.emplace_back(asGeom);
  input.asBuildOffsetInfo.emplace_back(offset);

  return input;
}

void MazeVulkan::createBottomLevelAS()
{
  std::vector<vk::RaytracingBuilderKHR::BlasInput> allBlas;
  allBlas.reserve(m_objModel.size());
  for(const auto& obj : m_objModel)
  {
    auto blas = objectToVkGeometryKHR(obj);

    allBlas.emplace_back(blas);
  }
  m_rtBuilder.buildBlas(allBlas, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
}

void MazeVulkan::createTopLevelAS()
{
  std::vector<VkAccelerationStructureInstanceKHR> tlas;
  tlas.reserve(m_instances.size());
  for(const MazeVulkan::ObjInstance& inst : m_instances)
  {
    VkAccelerationStructureInstanceKHR rayInst{};
    rayInst.transform                      = vk::toTransformMatrixKHR(inst.transform);  
    rayInst.instanceCustomIndex            = inst.objIndex;                               
    rayInst.accelerationStructureReference = m_rtBuilder.getBlasDeviceAddress(inst.objIndex);
    rayInst.flags                          = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    rayInst.mask                           = 0xFF;       
    rayInst.instanceShaderBindingTableRecordOffset = 0;  
    tlas.emplace_back(rayInst);
  }
  m_rtBuilder.buildTlas(tlas, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
}

void MazeVulkan::createRtDescriptorSet()
{
  m_rtDescSetLayoutBind.addBinding(RtxBindings::eTlas, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1,
                                   VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);  
  m_rtDescSetLayoutBind.addBinding(RtxBindings::eOutImage, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
                                   VK_SHADER_STAGE_RAYGEN_BIT_KHR);  

  m_rtDescPool      = m_rtDescSetLayoutBind.createPool(m_device);
  m_rtDescSetLayout = m_rtDescSetLayoutBind.createLayout(m_device);

  VkDescriptorSetAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  allocateInfo.descriptorPool     = m_rtDescPool;
  allocateInfo.descriptorSetCount = 1;
  allocateInfo.pSetLayouts        = &m_rtDescSetLayout;
  vkAllocateDescriptorSets(m_device, &allocateInfo, &m_rtDescSet);


  VkAccelerationStructureKHR tlas = m_rtBuilder.getAccelerationStructure();
  VkWriteDescriptorSetAccelerationStructureKHR descASInfo{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
  descASInfo.accelerationStructureCount = 1;
  descASInfo.pAccelerationStructures    = &tlas;
  VkDescriptorImageInfo imageInfo{{}, m_offscreenColor.descriptor.imageView, VK_IMAGE_LAYOUT_GENERAL};

  std::vector<VkWriteDescriptorSet> writes;
  writes.emplace_back(m_rtDescSetLayoutBind.makeWrite(m_rtDescSet, RtxBindings::eTlas, &descASInfo));
  writes.emplace_back(m_rtDescSetLayoutBind.makeWrite(m_rtDescSet, RtxBindings::eOutImage, &imageInfo));
  vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}


void MazeVulkan::updateRtDescriptorSet()
{
  std::vector<VkWriteDescriptorSet> writes;
  VkDescriptorImageInfo imageInfo{{}, m_offscreenColor.descriptor.imageView, VK_IMAGE_LAYOUT_GENERAL};
  writes.push_back(m_rtDescSetLayoutBind.makeWrite(m_rtDescSet, RtxBindings::eOutImage, &imageInfo));  

  VkAccelerationStructureKHR tlas = m_rtBuilder.getAccelerationStructure();
  VkWriteDescriptorSetAccelerationStructureKHR descASInfo{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
  descASInfo.accelerationStructureCount = 1;
  descASInfo.pAccelerationStructures    = &tlas;
  writes.push_back(m_rtDescSetLayoutBind.makeWrite(m_rtDescSet, RtxBindings::eTlas, &descASInfo));

  vkUpdateDescriptorSets(m_device, (uint32_t)writes.size(), writes.data(), 0, nullptr);
}


void MazeVulkan::createRtPipeline()
{
  enum StageIndices
  {
    eRaygen,
    eMiss,
    eMiss2,
    eClosestHit,
    eShaderGroupCount
  };

  std::array<VkPipelineShaderStageCreateInfo, eShaderGroupCount> stages{};
  VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
  stage.pName = "main";  
  stage.module = vk::createShaderModule(m_device, loadFile("spv/raytrace.rgen.spv", true, defaultSearchPaths, true));
  stage.stage     = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
  stages[eRaygen] = stage;
  stage.module = vk::createShaderModule(m_device, loadFile("spv/raytrace.rmiss.spv", true, defaultSearchPaths, true));
  stage.stage   = VK_SHADER_STAGE_MISS_BIT_KHR;
  stages[eMiss] = stage;
  stage.module =
      vk::createShaderModule(m_device, loadFile("spv/raytraceShadow.rmiss.spv", true, defaultSearchPaths, true));
  stage.stage    = VK_SHADER_STAGE_MISS_BIT_KHR;
  stages[eMiss2] = stage;
  stage.module = vk::createShaderModule(m_device, loadFile("spv/raytrace.rchit.spv", true, defaultSearchPaths, true));
  stage.stage         = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
  stages[eClosestHit] = stage;


  VkRayTracingShaderGroupCreateInfoKHR group{VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR};
  group.anyHitShader       = VK_SHADER_UNUSED_KHR;
  group.closestHitShader   = VK_SHADER_UNUSED_KHR;
  group.generalShader      = VK_SHADER_UNUSED_KHR;
  group.intersectionShader = VK_SHADER_UNUSED_KHR;

  group.type          = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
  group.generalShader = eRaygen;
  m_rtShaderGroups.push_back(group);

  group.type          = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
  group.generalShader = eMiss;
  m_rtShaderGroups.push_back(group);

  group.type          = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
  group.generalShader = eMiss2;
  m_rtShaderGroups.push_back(group);

  group.type             = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
  group.generalShader    = VK_SHADER_UNUSED_KHR;
  group.closestHitShader = eClosestHit;
  m_rtShaderGroups.push_back(group);

  VkPushConstantRange pushConstant{VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
                                   0, sizeof(PushConstantRay)};


  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
  pipelineLayoutCreateInfo.pPushConstantRanges    = &pushConstant;

  std::vector<VkDescriptorSetLayout> rtDescSetLayouts = {m_rtDescSetLayout, m_descSetLayout};
  pipelineLayoutCreateInfo.setLayoutCount             = static_cast<uint32_t>(rtDescSetLayouts.size());
  pipelineLayoutCreateInfo.pSetLayouts                = rtDescSetLayouts.data();

  vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_rtPipelineLayout);


  VkRayTracingPipelineCreateInfoKHR rayPipelineInfo{VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};
  rayPipelineInfo.stageCount = static_cast<uint32_t>(stages.size());  
  rayPipelineInfo.pStages    = stages.data();

  rayPipelineInfo.groupCount = static_cast<uint32_t>(m_rtShaderGroups.size());
  rayPipelineInfo.pGroups    = m_rtShaderGroups.data();

  rayPipelineInfo.maxPipelineRayRecursionDepth = 2;  
  rayPipelineInfo.layout                       = m_rtPipelineLayout;

  vkCreateRayTracingPipelinesKHR(m_device, {}, {}, 1, &rayPipelineInfo, nullptr, &m_rtPipeline);


  if(m_rtProperties.maxRayRecursionDepth <= 1)
  {
    throw std::runtime_error("Device fails to support ray recursion (m_rtProperties.maxRayRecursionDepth <= 1)");
  }

  for(auto& s : stages)
    vkDestroyShaderModule(m_device, s.module, nullptr);
}

void MazeVulkan::createRtShaderBindingTable()
{
  uint32_t missCount{2};
  uint32_t hitCount{1};
  auto     handleCount = 1 + missCount + hitCount;
  uint32_t handleSize  = m_rtProperties.shaderGroupHandleSize;

  uint32_t handleSizeAligned = align_up(handleSize, m_rtProperties.shaderGroupHandleAlignment);

  m_rgenRegion.stride = align_up(handleSizeAligned, m_rtProperties.shaderGroupBaseAlignment);
  m_rgenRegion.size = m_rgenRegion.stride;  
  m_missRegion.stride = handleSizeAligned;
  m_missRegion.size   = align_up(missCount * handleSizeAligned, m_rtProperties.shaderGroupBaseAlignment);
  m_hitRegion.stride  = handleSizeAligned;
  m_hitRegion.size    = align_up(hitCount * handleSizeAligned, m_rtProperties.shaderGroupBaseAlignment);

  uint32_t             dataSize = handleCount * handleSize;
  std::vector<uint8_t> handles(dataSize);
  auto result = vkGetRayTracingShaderGroupHandlesKHR(m_device, m_rtPipeline, 0, handleCount, dataSize, handles.data());
  assert(result == VK_SUCCESS);

  VkDeviceSize sbtSize = m_rgenRegion.size + m_missRegion.size + m_hitRegion.size + m_callRegion.size;
  m_rtSBTBuffer        = m_alloc.createBuffer(sbtSize,
                                              VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                                                  | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
                                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  m_debug.setObjectName(m_rtSBTBuffer.buffer, std::string("SBT"));  

  VkBufferDeviceAddressInfo info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, m_rtSBTBuffer.buffer};
  VkDeviceAddress           sbtAddress = vkGetBufferDeviceAddress(m_device, &info);
  m_rgenRegion.deviceAddress           = sbtAddress;
  m_missRegion.deviceAddress           = sbtAddress + m_rgenRegion.size;
  m_hitRegion.deviceAddress            = sbtAddress + m_rgenRegion.size + m_missRegion.size;

  auto getHandle = [&](int i) { return handles.data() + i * handleSize; };

  auto*    pSBTBuffer = reinterpret_cast<uint8_t*>(m_alloc.map(m_rtSBTBuffer));
  uint8_t* pData{nullptr};
  uint32_t handleIdx{0};
  pData = pSBTBuffer;
  memcpy(pData, getHandle(handleIdx++), handleSize);
  pData = pSBTBuffer + m_rgenRegion.size;
  for(uint32_t c = 0; c < missCount; c++)
  {
    memcpy(pData, getHandle(handleIdx++), handleSize);
    pData += m_missRegion.stride;
  }
  pData = pSBTBuffer + m_rgenRegion.size + m_missRegion.size;
  for(uint32_t c = 0; c < hitCount; c++)
  {
    memcpy(pData, getHandle(handleIdx++), handleSize);
    pData += m_hitRegion.stride;
  }

  m_alloc.unmap(m_rtSBTBuffer);
  m_alloc.finalizeAndReleaseStaging();
}

void MazeVulkan::raytrace(const VkCommandBuffer& cmdBuf, const glm::vec4& clearColor)
{
  m_debug.beginLabel(cmdBuf, "Ray trace");
  m_pcRay.clearColor     = clearColor;
  m_pcRay.lightPosition  = m_pcRaster.lightPosition;
  m_pcRay.lightIntensity = m_pcRaster.lightIntensity;
  m_pcRay.lightType      = m_pcRaster.lightType;

  std::vector<VkDescriptorSet> descSets{m_rtDescSet, m_descSet};
  vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_rtPipeline);
  vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_rtPipelineLayout, 0,
                          (uint32_t)descSets.size(), descSets.data(), 0, nullptr);
  vkCmdPushConstants(cmdBuf, m_rtPipelineLayout,
                     VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
                     0, sizeof(PushConstantRay), &m_pcRay);


  vkCmdTraceRaysKHR(cmdBuf, &m_rgenRegion, &m_missRegion, &m_hitRegion, &m_callRegion, m_size.width, m_size.height, 1);


  m_debug.endLabel(cmdBuf);
}

glm::vec3 MazeVulkan::closestPointTriangle(const glm::vec3& P, const glm::vec3& A, const glm::vec3& B, const glm::vec3& C)
{
  glm::vec3 AB = B - A, AC = C - A, AP = P - A;
  float d1 = glm::dot(AB, AP), d2 = glm::dot(AC, AP);
  if(d1 <= 0 && d2 <= 0) return A;
  glm::vec3 BP = P - B; float d3 = glm::dot(AB, BP), d4 = glm::dot(AC, BP);
  if(d3 >= 0 && d4 <= d3) return B;
  glm::vec3 CP = P - C; float d5 = glm::dot(AB, CP), d6 = glm::dot(AC, CP);
  if(d6 >= 0 && d5 <= d6) return C;
  float vc = d1*d4 - d3*d2;
  if(vc <= 0 && d1 >= 0 && d3 <= 0) {
    float v = d1 / (d1 - d3);
    return A + v * AB;
  }
  float vb = d5*d2 - d1*d6;
  if(vb <= 0 && d2 >= 0 && d6 <= 0) {
    float w = d2 / (d2 - d6);
    return A + w * AC;
  }
  float va = d3*d6 - d5*d4;
  if(va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0) {
    float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
    return B + w * (C - B);
  }
  glm::vec3 N = glm::cross(AB, AC);
  float dist = glm::dot(AP, N) / glm::length(N);
  return P - dist * glm::normalize(N);
}

void MazeVulkan::handleCollision(const glm::vec3& next, const glm::mat4& transform, glm::vec3& ballPos, glm::vec3& ballVel, 
                                float ballRadius, const std::vector<Triangle>& triangles, float restitution, float friction)
{

  if (intersectBVH(next, ballRadius, transform)) {

    for(auto &tri : triangles) {
      glm::vec3 A = glm::vec3(transform * glm::vec4(tri.a, 1));
      glm::vec3 B = glm::vec3(transform * glm::vec4(tri.b, 1));
      glm::vec3 C = glm::vec3(transform * glm::vec4(tri.c, 1));
      glm::vec3 p = closestPointTriangle(next, A, B, C);
      glm::vec3 diff = next - p;
      float d2 = glm::dot(diff, diff);
      if(d2 < ballRadius * ballRadius) {
        float d = std::sqrt(d2);
        glm::vec3 n = (d > 1e-6f ? diff/d : glm::vec3(0,1,0));
        ballPos = p + n * ballRadius;
        glm::vec3 vN = glm::dot(ballVel, n) * n;
        glm::vec3 vT = ballVel - vN;
        ballVel = -restitution * vN + (1.0f - friction) * vT;
        break;
      }
    }
  }
}

float calculateImpulse(
        float massA, float massB,
        const glm::vec3& velocityA, const glm::vec3& velocityB,
        const glm::vec3& normal,
        float restitution
) {
    glm::vec3 relativeVelocity = velocityB - velocityA;
    float velAlongNormal = glm::dot(relativeVelocity, normal);

    if (velAlongNormal > 0)
        return 0.0f;

    float invMassA = (massA > 0) ? 1.0f / massA : 0.0f;
    float invMassB = (massB > 0) ? 1.0f / massB : 0.0f;

    float impulseScalar = -(1.0f + restitution) * velAlongNormal;
    impulseScalar /= invMassA + invMassB;

    return impulseScalar;
}

void MazeVulkan::buildBVH() {

    bvhNodes.clear();
    primitives.clear();
    

    for (size_t i = 0; i < mazeTris.size(); i++) {
        Primitive prim;
        prim.triangle = mazeTris[i];
        prim.meshIndex = 0; 
        primitives.push_back(prim);
    }
    

    rootNodeIndex = createBVHNode(0, primitives.size());
    

    subdivideNode(rootNodeIndex);
}

int MazeVulkan::createBVHNode(int firstPrimitive, int primitiveCount) {
    BVHNode node;
    node.firstPrimitive = firstPrimitive;
    node.primitiveCount = primitiveCount;
    node.leftChild = -1;
    node.rightChild = -1;
    
    updateNodeBounds(bvhNodes.size());
    
    bvhNodes.push_back(node);
    return bvhNodes.size() - 1;
}

void MazeVulkan::updateNodeBounds(int nodeIndex) {
    BVHNode& node = bvhNodes[nodeIndex];
    node.bounds = AABB();
    
    for (int i = 0; i < node.primitiveCount; i++) {
        const Triangle& tri = primitives[node.firstPrimitive + i].triangle;
        node.bounds.expand(tri.a);
        node.bounds.expand(tri.b);
        node.bounds.expand(tri.c);
    }
}

void MazeVulkan::subdivideNode(int nodeIndex) {
    BVHNode& node = bvhNodes[nodeIndex];
    
    if (node.primitiveCount <= 2) {
        return;
    }
    

    std::vector<glm::vec3> centers(node.primitiveCount);
    glm::vec3 center(0.0f);
    for (int i = 0; i < node.primitiveCount; i++) {
        const Triangle& tri = primitives[node.firstPrimitive + i].triangle;
        centers[i] = (tri.a + tri.b + tri.c) / 3.0f;
        center += centers[i];
    }
    center /= float(node.primitiveCount);
    

    glm::vec3 variance(0.0f);
    for (int i = 0; i < node.primitiveCount; i++) {
        glm::vec3 diff = centers[i] - center;
        variance += diff * diff;
    }
    
    int axis = 0;
    if (variance.y > variance.x) axis = 1;
    if (variance.z > variance[axis]) axis = 2;
    
    std::sort(primitives.begin() + node.firstPrimitive,
              primitives.begin() + node.firstPrimitive + node.primitiveCount,
              [&](const Primitive& a, const Primitive& b) {
                  glm::vec3 centerA = (a.triangle.a + a.triangle.b + a.triangle.c) / 3.0f;
                  glm::vec3 centerB = (b.triangle.a + b.triangle.b + b.triangle.c) / 3.0f;
                  return centerA[axis] < centerB[axis];
              });
    

    int mid = node.primitiveCount / 2;
    node.leftChild = createBVHNode(node.firstPrimitive, mid);
    node.rightChild = createBVHNode(node.firstPrimitive + mid, node.primitiveCount - mid);
    

    subdivideNode(node.leftChild);
    subdivideNode(node.rightChild);
}

bool MazeVulkan::intersectBVH(const glm::vec3& center, float radius, const glm::mat4& transform) {

    AABB sphereBounds;
    sphereBounds.min = center - glm::vec3(radius);
    sphereBounds.max = center + glm::vec3(radius);
    

    std::vector<int> stack;
    stack.push_back(rootNodeIndex);
    
    while (!stack.empty()) {
        int nodeIndex = stack.back();
        stack.pop_back();
        
        const BVHNode& node = bvhNodes[nodeIndex];
        

        if (!sphereBounds.intersects(node.bounds)) {
            continue;
        }
        
        if (node.isLeaf()) {

            for (int i = 0; i < node.primitiveCount; i++) {
                const Triangle& tri = primitives[node.firstPrimitive + i].triangle;
                glm::vec3 A = glm::vec3(transform * glm::vec4(tri.a, 1.0f));
                glm::vec3 B = glm::vec3(transform * glm::vec4(tri.b, 1.0f));
                glm::vec3 C = glm::vec3(transform * glm::vec4(tri.c, 1.0f));
                
                glm::vec3 p = closestPointTriangle(center, A, B, C);
                float dist2 = glm::dot(center - p, center - p);
                
                if (dist2 < radius * radius) {
                    return true;
                }
            }
        } else {
            stack.push_back(node.leftChild);
            stack.push_back(node.rightChild);
        }
    }
    
    return false;
}