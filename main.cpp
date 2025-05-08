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
#include "physicsUtil.h"


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
  helloVk.loadModel(nvh::findFile("media/scenes/newmaze.obj", defaultSearchPaths, true));
  helloVk.loadModel(nvh::findFile("media/scenes/mysphere.obj", defaultSearchPaths, true));
  helloVk.loadModel(nvh::findFile("media/scenes/shield.obj", defaultSearchPaths, true));
  helloVk.loadModel(nvh::findFile("media/scenes/spring.obj", defaultSearchPaths, true));
  helloVk.loadModel(nvh::findFile("media/scenes/fan.obj", defaultSearchPaths, true));
  
  helloVk.m_instances[0].transform = glm::mat4(1.0f);

  glm::vec3 ballStart = {-25.2f, 20.f, -4.72f};
  // glm::vec3 ballStart = {-23.022f, 4.599f, 16.931f};
  helloVk.m_instances[1].transform = glm::translate(glm::mat4(1.0f), ballStart)
                                 * glm::scale(glm::mat4(1.0f), glm::vec3(1.0f)); 
  
  glm::vec3 shieldStart = {-0.786f, 2.745f, -4.381f};
  glm::mat4 baseShieldLocal = helloVk.m_instances[2].transform;

  glm::vec3 springStart = {-26.6f, 3.202f, 17.315f};
  // helloVk.m_instances[3].transform = glm::translate(glm::mat4(1.0f), springStart)
  //                                * glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
  glm::mat4 baseSpringLocal = helloVk.m_instances[3].transform;

  glm::vec3 fanStart = {9.991f, 4.434f, -13.264f};

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

  bool        followBall;  
  bool        spaceWasPressed; 
  const float followDist = 4.0f; 
  const float heightOff  = 2.0f; 

  glm::vec3 ballPos = ballStart;
  glm::quat ballRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
  glm::vec3 ballVel = {0.0f,0.0f,0.0f};
  glm::vec3 rotateAxis = {1.0f,0.0f,0.0f};
  glm::vec3 rotateVel = {0.0f,0.0f,0.0f};
  float     ballRadius   = 1.744f;
  float ballMass = 1;
  const glm::vec3 GRAV = {0.0f,-9.81f,0.0f};
  const float μ = 0.05f;        
  const float e = 0.15f;        

  float g_tiltX = 0.0f, g_tiltZ = 0.0f;
  const float g_speed = glm::radians(5.0f); 
  double lastTime = glfwGetTime();

  bool   shieldCollected     = false;
  float  shieldPickupRadius  = 4.0f;

  bool   springArmed       = false;     
  bool   springCompressing = false;     
  float  springCompress    = 0.0f;      
  const float maxCompress  = 5.0f;      
  const float compressRate = 1.0f;      
  const float bounceStrength = 20.0f;
  float springPickupRadius = 10.0f;
  glm::vec3 springLocalPos = glm::vec3(baseSpringLocal[3]);

  float fanAngle     = 0.0f;
  const float fanSpeed = glm::radians(180.0f);
  glm::vec3 fanForceOffset = glm::vec3(0.0f, -4.0f, 2.0f); 
  glm::vec3 fanForceCenter = fanStart + fanForceOffset; 
  float fanRadius   = 3.0f;
  float fanStrength = 70.0f;

  // Main loop
  while(!glfwWindowShouldClose(window))
  {
    glfwPollEvents();
    if(helloVk.isMinimized())
      continue;

    double now = glfwGetTime();
    float  dt  = float(now - lastTime);
    lastTime   = now;

    // fan rotation
    fanAngle += fanSpeed * dt * 2.0f;
    glm::mat4 Fan_T1 = glm::translate(glm::mat4(1.0f),  fanStart);
    glm::mat4 Fan_R  = glm::rotate   (glm::mat4(1.0f), fanAngle, glm::vec3(0, 0, 1));
    glm::mat4 Fan_T0 = glm::translate(glm::mat4(1.0f), -fanStart);
    helloVk.m_instances[4].transform = Fan_T1 * Fan_R * Fan_T0;

    // maze tilt
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
    
    glm::mat4 shieldGlobal = R * baseShieldLocal;
    glm::mat4 springGlobal = R * baseSpringLocal;
    glm::mat4 fanGlobal = R * helloVk.m_instances[4].transform;
    
    helloVk.m_instances[0].transform = R;
    // shield
    glm::vec3 shieldWorld0 = glm::vec3(R * glm::vec4(shieldStart, 1.0f));
    if (!shieldCollected && glm::length(ballPos - shieldWorld0) < shieldPickupRadius) {
      shieldCollected = true;
    }
    if (!shieldCollected) {
      helloVk.m_instances[2].transform = shieldGlobal;
    } else {
      fanStrength = 20.0f;
      // glm::vec3 mazeNegZ = -glm::normalize(glm::vec3(R * glm::vec4(0,0,1,0)));
      glm::vec3 shieldPos = ballPos + glm::vec3(0.8f,-2.5f,2.5f);
      helloVk.m_instances[2].transform = glm::translate(glm::mat4(1.0f), shieldPos);
    }
    // spring
    glm::vec3 springWorld0 = springStart;
    if (glm::distance(ballPos, springWorld0) < springPickupRadius) {
      springArmed = true;
    }
    static bool wasQPressed = false;
    bool qDown = (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS);

    if (springArmed) {
      if (qDown) {
        springCompressing = true;
        springCompress += compressRate * dt;
        springCompress = glm::min(springCompress, maxCompress);
      }
      else if (!qDown && wasQPressed && springCompressing) {
        ballVel += glm::vec3(1.0f, 0.0f, 0.0f) * bounceStrength * springCompress;

        springCompressing = false;
        springCompress    = 0.0f;
        springArmed       = false;
      }
    }

    wasQPressed = qDown;

    if (springCompressing) {
      float d = glm::clamp(springCompress, 0.0f, 1.0f);

      glm::mat4 localOffset = glm::translate(glm::mat4(1.0f), glm::vec3(-d, 0, 0));

      helloVk.m_instances[3].transform = springGlobal * localOffset;
    } 
    else {
      helloVk.m_instances[3].transform = springGlobal;
    }

    // fan
    helloVk.m_instances[4].transform = fanGlobal;

    glm::vec3 forceCenterWorld = glm::vec3(R * glm::vec4(fanForceCenter, 1.0f));
    glm::vec3 fanDir = glm::normalize(glm::vec3(R * glm::vec4(0,0,1,0)));
    
    glm::vec3 platformUp = glm::vec3(R * glm::vec4(0, 1, 0, 0));
    glm::vec3 effectiveGravity = GRAV - platformUp * glm::dot(GRAV, platformUp);
    int   subSteps = 200;
    float subDt    = dt / float(subSteps);

    for(int step = 0; step < subSteps; ++step) {
      ballVel += GRAV * subDt;
      
      if(!shieldCollected)
      {
        glm::vec3 diff = ballPos - forceCenterWorld;
        diff.y = 0;
        if(glm::length(diff) < fanRadius) {
          ballVel += fanDir * (fanStrength * subDt);
        }
      }

      glm::vec3 next = ballPos + ballVel * subDt;

      // maze collision
      bool collided = false;
      glm::vec3 contactN(0.0f);
      float penetration = 0.0f;
      
      // 使用BVH进行碰撞检测
      if(helloVk.mazeBVH.intersect(next, ballRadius, contactN, penetration)) {
        collided = true;
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
        if (std::abs(glm::dot(ballVel, n)) < 1e-3) {
            // tangential acceleration
            glm::vec3 aT = gT - μ * vT;
            // tangential velocity
            vT += aT * subDt;
            glm::vec3 vN = glm::dot(ballVel, n) * n;
            ballVel = vN + vT;
            rotateAxis = glm::normalize(glm::cross(vT, normals[i]));
            rotateVel = vT;
        } else {
            float impulse = calculateImpulse(-1, ballMass, glm::vec3(0.), ballVel, n);
            ballVel += impulse * n / ballMass;
        }
      }

      // spring collision
      if(glm::length(next - springStart) < 10.0f) {
        if(helloVk.springBVH.intersect(next, ballRadius, contactN, penetration)) {
          next += contactN * penetration;
          glm::vec3 n = contactN;
          glm::vec3 vN = glm::dot(ballVel, n) * n;
          glm::vec3 vT = ballVel - vN;
          ballVel = -e * vN + (1.0f-μ) * vT;
        }
      }
      
      // shield collision
      if(glm::length(next - shieldStart) < 3.0f) {
        if(helloVk.shieldBVH.intersect(next, ballRadius, contactN, penetration)) {
          next += contactN * penetration;
          glm::vec3 n = contactN;
          glm::vec3 vN = glm::dot(ballVel, n) * n;
          glm::vec3 vT = ballVel - vN;
          ballVel = -e * vN + (1.0f-μ) * vT;
        }
      }
      
      // fan collision
      if(glm::length(next - fanStart) < 3.0f) {
        if(helloVk.fanBVH.intersect(next, ballRadius, contactN, penetration)) {
          next += contactN * penetration;
          glm::vec3 n = contactN;
          glm::vec3 vN = glm::dot(ballVel, n) * n;
          glm::vec3 vT = ballVel - vN;
          ballVel = -e * vN + (1.0f-μ) * vT;
        }
      }
      float theta = glm::length(rotateVel * subDt) / ballRadius;
      glm::quat rotationQuat = glm::angleAxis(theta, rotateAxis);
      ballRotation = rotationQuat * ballRotation;
      ballPos = next;
    }
    helloVk.m_instances[1].transform =
            glm::translate(glm::mat4(1.0f), ballPos) *
            glm::mat4_cast(ballRotation);

    // bool spacePressed = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);
    // if (spacePressed && !spaceWasPressed) {
    //   followBall = !followBall;
    // }
    // spaceWasPressed = spacePressed;
    
    // if (followBall) {
    //   glm::vec3 dir = glm::vec3(ballVel.x, 0.0f, ballVel.z);
    //   if (glm::length(dir) < 1e-3f) {
    //     dir = glm::vec3(0.0f, 0.0f, -1.0f);
    //   } else {
    //     dir = glm::normalize(dir);
    //   }

    //   // glm::vec3 eye = ballPos - dir * followDist
    //   //               + glm::vec3(0.0f, heightOff, 0.0f);

    //   // glm::vec3 center = ballPos;

    //   glm::vec3 eye = ballPos + glm::vec3(-20.0f, 15.0f, 0.0f);
    //   glm::vec3 center = ballPos + glm::vec3(0.0f, 5.0f, 0.0f);
    //   CameraManip.setLookat(eye, center, glm::vec3(0.0f,1.0f,0.0f));
    // }
    // else{
    //   CameraManip.setLookat(glm::vec3(-60, 48, 0), glm::vec3(0, 1, 0), glm::vec3(0, 1, 0));
    // }

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
