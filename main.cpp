/*
 * Copyright (c) 2014-2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2014-2021 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */


// ImGui - standalone example application for Glfw + Vulkan, using programmable
// pipeline If you are new to ImGui, see examples/README.txt and documentation
// at the top of imgui.cpp.

#include <array>
#include <string>
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"
#include "imgui/imgui_helper.h"

#include "hello_vulkan.h"
#include "imgui/imgui_camera_widget.h"
#include "nvh/cameramanipulator.hpp"
#include "nvh/fileoperations.hpp"
#include "nvpsystem.hpp"
#include "nvvk/commands_vk.hpp"
#include "nvvk/context_vk.hpp"


//////////////////////////////////////////////////////////////////////////
#define UNUSED(x) (void)(x)
//////////////////////////////////////////////////////////////////////////

// Default search path for shaders
std::vector<std::string> defaultSearchPaths;


// GLFW Callback functions
static void onErrorCallback(int error, const char* description)
{
  fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// Extra UI
void renderUI(HelloVulkan& helloVk)
{
  ImGuiH::CameraWidget();
  if(ImGui::CollapsingHeader("Light"))
  {
    ImGui::RadioButton("Point", &helloVk.m_pcRaster.lightType, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Infinite", &helloVk.m_pcRaster.lightType, 1);

    ImGui::SliderFloat3("Position", &helloVk.m_pcRaster.lightPosition.x, -20.f, 20.f);
    ImGui::SliderFloat("Intensity", &helloVk.m_pcRaster.lightIntensity, 0.f, 150.f);
  }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
static int const SAMPLE_WIDTH  = 1280;
static int const SAMPLE_HEIGHT = 720;


//--------------------------------------------------------------------------------------------------
// Application Entry
//
int main(int argc, char** argv)
{
  UNUSED(argc);

  // Setup GLFW window
  glfwSetErrorCallback(onErrorCallback);
  if(!glfwInit())
  {
    return 1;
  }
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow* window = glfwCreateWindow(SAMPLE_WIDTH, SAMPLE_HEIGHT, "3DMazeGame", nullptr, nullptr);


  // Setup camera
  CameraManip.setWindowSize(SAMPLE_WIDTH, SAMPLE_HEIGHT);
  CameraManip.setLookat(glm::vec3(-60, 48, 0), glm::vec3(0, 1, 0), glm::vec3(0, 1, 0));

  // Setup Vulkan
  if(!glfwVulkanSupported())
  {
    printf("GLFW: Vulkan Not Supported\n");
    return 1;
  }

  // setup some basic things for the sample, logging file for example
  NVPSystem system("3DMazeGame");

  // Search path for shaders and other media
  defaultSearchPaths = {
      NVPSystem::exePath() + PROJECT_RELDIRECTORY,
      NVPSystem::exePath() + PROJECT_RELDIRECTORY "..",
      std::string(PROJECT_NAME),
  };

  // Vulkan required extensions
  assert(glfwVulkanSupported() == 1);
  uint32_t count{0};
  auto     reqExtensions = glfwGetRequiredInstanceExtensions(&count);

  // Requesting Vulkan extensions and layers
  nvvk::ContextCreateInfo contextInfo;
  contextInfo.setVersion(1, 2);                       // Using Vulkan 1.2
  for(uint32_t ext_id = 0; ext_id < count; ext_id++)  // Adding required extensions (surface, win32, linux, ..)
    contextInfo.addInstanceExtension(reqExtensions[ext_id]);
  // contextInfo.addInstanceLayer("VK_LAYER_LUNARG_monitor", true);              // FPS in titlebar
  contextInfo.addInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, true);  // Allow debug names
  contextInfo.addDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);            // Enabling ability to present rendering

  // #VKRay: Activate the ray tracing extension
  VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeature{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
  contextInfo.addDeviceExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, false, &accelFeature);  // To build acceleration structures
  VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeature{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
  contextInfo.addDeviceExtension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, false, &rtPipelineFeature);  // To use vkCmdTraceRaysKHR
  contextInfo.addDeviceExtension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);  // Required by ray tracing pipeline

  // Creating Vulkan base application
  nvvk::Context vkctx{};
  vkctx.initInstance(contextInfo);
  // Find all compatible devices
  auto compatibleDevices = vkctx.getCompatibleDevices(contextInfo);
  assert(!compatibleDevices.empty());
  // Use a compatible device
  vkctx.initDevice(compatibleDevices[0], contextInfo);

  // Create example
  HelloVulkan helloVk;

  // Window need to be opened to get the surface on which to draw
  const VkSurfaceKHR surface = helloVk.getVkSurface(vkctx.m_instance, window);
  vkctx.setGCTQueueWithPresent(surface);

  helloVk.setup(vkctx.m_instance, vkctx.m_device, vkctx.m_physicalDevice, vkctx.m_queueGCT.familyIndex);
  helloVk.createSwapchain(surface, SAMPLE_WIDTH, SAMPLE_HEIGHT);
  helloVk.createDepthBuffer();
  helloVk.createRenderPass();
  helloVk.createFrameBuffers();

  // Setup Imgui
  helloVk.initGUI(0);  // Using sub-pass 0

  // Creation of the example
  helloVk.loadModel(nvh::findFile("media/scenes/maze.obj", defaultSearchPaths, true));
  helloVk.loadModel(nvh::findFile("media/scenes/mysphere.obj", defaultSearchPaths, true));
  // helloVk.initMazeTris();
  
  helloVk.m_instances.resize(2);
  helloVk.m_instances[0].transform = glm::mat4(1.0f);
  glm::vec3 ballStart = {-25.2f, 20.f, -4.72f};
  helloVk.m_instances[1].transform = glm::translate(glm::mat4(1.0f), ballStart)
                                 * glm::scale(glm::mat4(1.0f), glm::vec3(1.0f)); 

  helloVk.createOffscreenRender();
  helloVk.createDescriptorSetLayout();
  helloVk.createGraphicsPipeline();
  helloVk.createUniformBuffer();
  helloVk.createObjDescriptionBuffer();
  helloVk.updateDescriptorSet();

  // #VKRay
  helloVk.initRayTracing();
  helloVk.createBottomLevelAS();
  helloVk.createTopLevelAS();
  helloVk.createRtDescriptorSet();
  helloVk.createRtPipeline();
  helloVk.createRtShaderBindingTable();

  helloVk.createPostDescriptor();
  helloVk.createPostPipeline();
  helloVk.updatePostDescriptorSet();


  glm::vec4 clearColor   = glm::vec4(1, 1, 1, 1.00f);
  bool      useRaytracer = true;


  helloVk.setupGlfwCallbacks(window);
  ImGui_ImplGlfw_InitForVulkan(window, true);

  glm::vec3 ballPos = ballStart;
  glm::vec3 ballVel = {0.0f,0.0f,0.0f};
  float     ballRadius   = 1.744f;
  const glm::vec3 GRAV = {0.0f,-9.81f,0.0f};
  const float μ = 0.2f;        
  const float e = 0.3f;        

  float g_tiltX = 0.0f, g_tiltZ = 0.0f;
  const float g_speed = glm::radians(30.0f); 
  double lastTime = glfwGetTime();

  // Main loop
  while(!glfwWindowShouldClose(window))
  {
    glfwPollEvents();
    if(helloVk.isMinimized())
      continue;

    double now = glfwGetTime();
    float  dt  = float(now - lastTime);
    lastTime   = now;

    if(glfwGetKey(window, GLFW_KEY_W)==GLFW_PRESS) g_tiltX += g_speed*dt;
    if(glfwGetKey(window, GLFW_KEY_S)==GLFW_PRESS) g_tiltX -= g_speed*dt;
    if(glfwGetKey(window, GLFW_KEY_D)==GLFW_PRESS) g_tiltZ += g_speed*dt;
    if(glfwGetKey(window, GLFW_KEY_A)==GLFW_PRESS) g_tiltZ -= g_speed*dt;

    glm::vec3 mazePos = glm::vec3(helloVk.m_instances[0].transform[3]);

    glm::mat4 T1       = glm::translate(glm::mat4(1.0f), -mazePos);
    glm::mat4 RtiltX   = glm::rotate(glm::mat4(1.0f), g_tiltX, glm::vec3(1, 0, 0));
    glm::mat4 RtiltZ   = glm::rotate(glm::mat4(1.0f), g_tiltZ, glm::vec3(0, 0, 1));
    glm::mat4 T2       = glm::translate(glm::mat4(1.0f), mazePos);

    glm::mat4 R = T2 * RtiltZ * RtiltX * T1;
    helloVk.m_instances[0].transform = R;
    glm::vec3 platformUp = glm::vec3(R * glm::vec4(0, 1, 0, 0));
    glm::vec3 effectiveGravity = GRAV - platformUp * glm::dot(GRAV, platformUp);
    int   subSteps = 30;
    float subDt    = dt / float(subSteps);
    for(int step = 0; step < subSteps; ++step) {
      ballVel += GRAV * subDt;
      
      glm::vec3 next = ballPos + ballVel * subDt;

      bool collided = false;
      glm::vec3 contactN(0.0f);
      float penetration = 0.0f;
      for(auto& tri : helloVk.mazeTris) {
        glm::vec3 A = glm::vec3(R * glm::vec4(tri.a, 1.0f));
        glm::vec3 B = glm::vec3(R * glm::vec4(tri.b, 1.0f));
        glm::vec3 C = glm::vec3(R * glm::vec4(tri.c, 1.0f));
        glm::vec3 p = helloVk.closestPointTriangle(next, A, B, C);
        glm::vec3 diff = next - p;
        float dist2 = glm::dot(diff, diff);
        if(dist2 < ballRadius * ballRadius) {
          float dist = std::sqrt(dist2);
          contactN    = (dist > 1e-6f ? diff / dist : glm::vec3(0,1,0));
          penetration = ballRadius - dist;
          collided    = true;
          break;
        }
      }

      if(collided) {
        // penetration
        next += contactN * penetration;

        // normal
        glm::vec3 n = contactN;
        // gravity
        glm::vec3 gN = glm::dot(GRAV, n) * n;
        glm::vec3 gT = GRAV - gN;
        // tangential
        glm::vec3 vT = ballVel - glm::dot(ballVel, n) * n;
        // tangential acceleration
        glm::vec3 aT = gT - μ * vT;
        // tangential velocity
        vT += aT * subDt;
        // normal velocity
        glm::vec3 vN = glm::dot(ballVel, n) * n;
        // normal velocity new
        glm::vec3 vN_new = -e * vN;
        // tangential velocity new
        glm::vec3 vT_new = vT;
        ballVel = vN_new + vT_new;
      }

      ballPos = next;
    }

    helloVk.m_instances[1].transform = glm::translate(glm::mat4(1.0f), ballPos) * glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));

    helloVk.createObjDescriptionBuffer();
    helloVk.updateDescriptorSet();

    if(useRaytracer)
    {
      helloVk.createTopLevelAS();
      helloVk.updateRtDescriptorSet();
    }

    // Start the Dear ImGui frame
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();


    // Show UI window.
    // if(helloVk.showGui())
    // {
    //   ImGuiH::Panel::Begin();
    //   ImGui::ColorEdit3("Clear color", reinterpret_cast<float*>(&clearColor));
    //   ImGui::Checkbox("Ray Tracer mode", &useRaytracer);  // Switch between raster and ray tracing

    //   // renderUI(helloVk);
    //   ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    //   ImGuiH::Control::Info("", "", "(F10) Toggle Pane", ImGuiH::Control::Flags::Disabled);
    //   ImGuiH::Panel::End();
    // }

    // Start rendering the scene
    helloVk.prepareFrame();

    // Start command buffer of this frame
    auto                   curFrame = helloVk.getCurFrame();
    const VkCommandBuffer& cmdBuf   = helloVk.getCommandBuffers()[curFrame];

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmdBuf, &beginInfo);

    // Updating camera buffer
    helloVk.updateUniformBuffer(cmdBuf);

    // Clearing screen
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color        = {{clearColor[0], clearColor[1], clearColor[2], clearColor[3]}};
    clearValues[1].depthStencil = {1.0f, 0};

    // Offscreen render pass
    {
      VkRenderPassBeginInfo offscreenRenderPassBeginInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
      offscreenRenderPassBeginInfo.clearValueCount = 2;
      offscreenRenderPassBeginInfo.pClearValues    = clearValues.data();
      offscreenRenderPassBeginInfo.renderPass      = helloVk.m_offscreenRenderPass;
      offscreenRenderPassBeginInfo.framebuffer     = helloVk.m_offscreenFramebuffer;
      offscreenRenderPassBeginInfo.renderArea      = {{0, 0}, helloVk.getSize()};

      // Rendering Scene
      if(useRaytracer)
      {
        helloVk.raytrace(cmdBuf, clearColor);
      }
      else
      {
        vkCmdBeginRenderPass(cmdBuf, &offscreenRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        helloVk.rasterize(cmdBuf);
        vkCmdEndRenderPass(cmdBuf);
      }
    }

    // 2nd rendering pass: tone mapper, UI
    {
      VkRenderPassBeginInfo postRenderPassBeginInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
      postRenderPassBeginInfo.clearValueCount = 2;
      postRenderPassBeginInfo.pClearValues    = clearValues.data();
      postRenderPassBeginInfo.renderPass      = helloVk.getRenderPass();
      postRenderPassBeginInfo.framebuffer     = helloVk.getFramebuffers()[curFrame];
      postRenderPassBeginInfo.renderArea      = {{0, 0}, helloVk.getSize()};

      // Rendering tonemapper
      vkCmdBeginRenderPass(cmdBuf, &postRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
      helloVk.drawPost(cmdBuf);
      // Rendering UI
      ImGui::Render();
      ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuf);
      vkCmdEndRenderPass(cmdBuf);
    }

    // Submit for display
    vkEndCommandBuffer(cmdBuf);
    helloVk.submitFrame();
  }

  // Cleanup
  vkDeviceWaitIdle(helloVk.getDevice());

  helloVk.destroyResources();
  helloVk.destroy();
  vkctx.deinit();

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
