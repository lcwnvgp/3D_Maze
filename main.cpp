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

#include "maze_vulkan.h"
#include "imgui/imgui_camera_widget.h"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_win32.h>
#include <vulkan/vulkan.hpp>

#define UNUSED(x) (void)(x)

std::vector<std::string> defaultSearchPaths;


static void onErrorCallback(int error, const char* description)
{
  fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

void renderUI(MazeVulkan& mazeVk)
{
  ImGuiH::CameraWidget();
  if(ImGui::CollapsingHeader("Light"))
  {
    ImGui::RadioButton("Point", &mazeVk.m_pcRaster.lightType, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Infinite", &mazeVk.m_pcRaster.lightType, 1);

    ImGui::SliderFloat3("Position", &mazeVk.m_pcRaster.lightPosition.x, -20.f, 20.f);
    ImGui::SliderFloat("Intensity", &mazeVk.m_pcRaster.lightIntensity, 0.f, 150.f);
  }
}

static int const SAMPLE_WIDTH  = 1280;
static int const SAMPLE_HEIGHT = 720;


int main(int argc, char** argv)
{
  UNUSED(argc);

  glfwSetErrorCallback(onErrorCallback);
  if(!glfwInit())
  {
    return 1;
  }
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow* window = glfwCreateWindow(SAMPLE_WIDTH, SAMPLE_HEIGHT, "3DMazeGame", nullptr, nullptr);

  CameraManip.setWindowSize(SAMPLE_WIDTH, SAMPLE_HEIGHT);
  CameraManip.setLookat(glm::vec3(-60, 48, 0), glm::vec3(0, 1, 0), glm::vec3(0, 1, 0));

  if(!glfwVulkanSupported())
  {
    printf("GLFW: Vulkan Not Supported\n");
    return 1;
  }

  assert(glfwVulkanSupported() == 1);
  uint32_t count{0};
  auto     reqExtensions = glfwGetRequiredInstanceExtensions(&count);

  vk::ContextCreateInfo contextInfo;
  contextInfo.setVersion(1, 2);                       
  for(uint32_t ext_id = 0; ext_id < count; ext_id++)  
    contextInfo.addInstanceExtension(reqExtensions[ext_id]);
  contextInfo.addInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, true);  
  contextInfo.addDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);            

  VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeature{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
  contextInfo.addDeviceExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, false, &accelFeature);  
  VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeature{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
  contextInfo.addDeviceExtension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, false, &rtPipelineFeature);  
  contextInfo.addDeviceExtension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);  

  vk::Context vkctx{};
  vkctx.initInstance(contextInfo);
  auto compatibleDevices = vkctx.getCompatibleDevices(contextInfo);
  assert(!compatibleDevices.empty());
  vkctx.initDevice(compatibleDevices[0], contextInfo);

  MazeVulkan mazeVk;

  const VkSurfaceKHR surface = mazeVk.getVkSurface(vkctx.m_instance, window);
  vkctx.setGCTQueueWithPresent(surface);

  mazeVk.setup(vkctx.m_instance, vkctx.m_device, vkctx.m_physicalDevice, vkctx.m_queueGCT.familyIndex);
  mazeVk.createSwapchain(surface, SAMPLE_WIDTH, SAMPLE_HEIGHT);
  mazeVk.createDepthBuffer();
  mazeVk.createRenderPass();
  mazeVk.createFrameBuffers();

  mazeVk.initGUI(0);  

  mazeVk.loadModel(findFile("media/scenes/newmaze.obj", defaultSearchPaths, true));
  mazeVk.loadModel(findFile("media/scenes/mysphere.obj", defaultSearchPaths, true));
  mazeVk.loadModel(findFile("media/scenes/shield.obj", defaultSearchPaths, true));
  mazeVk.loadModel(findFile("media/scenes/spring.obj", defaultSearchPaths, true));
  mazeVk.loadModel(findFile("media/scenes/fan.obj", defaultSearchPaths, true));
  
  mazeVk.m_instances[0].transform = glm::mat4(1.0f);

  glm::vec3 ballStart = {-25.2f, 20.f, -4.72f};
  mazeVk.m_instances[1].transform = glm::translate(glm::mat4(1.0f), ballStart)
                                 * glm::scale(glm::mat4(1.0f), glm::vec3(1.0f)); 
  
  glm::vec3 shieldStart = {-0.786f, 2.745f, -4.381f};
  glm::mat4 baseShieldLocal = mazeVk.m_instances[2].transform;

  glm::vec3 springStart = {-26.6f, 3.202f, 17.315f};
  glm::mat4 baseSpringLocal = mazeVk.m_instances[3].transform;

  glm::vec3 fanStart = {9.991f, 4.434f, -13.264f};

  mazeVk.createOffscreenRender();
  mazeVk.createDescriptorSetLayout();
  mazeVk.createGraphicsPipeline();
  mazeVk.createUniformBuffer();
  mazeVk.createObjDescriptionBuffer();
  mazeVk.updateDescriptorSet();

  mazeVk.initRayTracing();
  mazeVk.createBottomLevelAS();
  mazeVk.createTopLevelAS();
  mazeVk.createRtDescriptorSet();
  mazeVk.createRtPipeline();
  mazeVk.createRtShaderBindingTable();

  mazeVk.createPostDescriptor();
  mazeVk.createPostPipeline();
  mazeVk.updatePostDescriptorSet();

  glm::vec4 clearColor   = glm::vec4(1, 1, 1, 1.00f);
  bool      useRaytracer = true;

  mazeVk.setupGlfwCallbacks(window);
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

  while(!glfwWindowShouldClose(window))
  {
    glfwPollEvents();
    if(mazeVk.isMinimized())
      continue;

    double now = glfwGetTime();
    float  dt  = float(now - lastTime);
    lastTime   = now;

    fanAngle += fanSpeed * dt * 2.0f;
    glm::mat4 Fan_T1 = glm::translate(glm::mat4(1.0f),  fanStart);
    glm::mat4 Fan_R  = glm::rotate   (glm::mat4(1.0f), fanAngle, glm::vec3(0, 0, 1));
    glm::mat4 Fan_T0 = glm::translate(glm::mat4(1.0f), -fanStart);
    mazeVk.m_instances[4].transform = Fan_T1 * Fan_R * Fan_T0;

    if(glfwGetKey(window, GLFW_KEY_W)==GLFW_PRESS) g_tiltX += g_speed*dt;
    if(glfwGetKey(window, GLFW_KEY_S)==GLFW_PRESS) g_tiltX -= g_speed*dt;
    if(glfwGetKey(window, GLFW_KEY_D)==GLFW_PRESS) g_tiltZ += g_speed*dt;
    if(glfwGetKey(window, GLFW_KEY_A)==GLFW_PRESS) g_tiltZ -= g_speed*dt;

    glm::vec3 mazePos = glm::vec3(mazeVk.m_instances[0].transform[3]);

    glm::mat4 T1       = glm::translate(glm::mat4(1.0f), -mazePos);
    glm::mat4 RtiltX   = glm::rotate(glm::mat4(1.0f), g_tiltX, glm::vec3(1, 0, 0));
    glm::mat4 RtiltZ   = glm::rotate(glm::mat4(1.0f), g_tiltZ, glm::vec3(0, 0, 1));
    glm::mat4 T2       = glm::translate(glm::mat4(1.0f), mazePos);

    glm::mat4 R = T2 * RtiltZ * RtiltX * T1;
    
    glm::mat4 shieldGlobal = R * baseShieldLocal;
    glm::mat4 springGlobal = R * baseSpringLocal;
    glm::mat4 fanGlobal = R * mazeVk.m_instances[4].transform;
    
    mazeVk.m_instances[0].transform = R;
    glm::vec3 shieldWorld0 = glm::vec3(R * glm::vec4(shieldStart, 1.0f));
    if (!shieldCollected && glm::length(ballPos - shieldWorld0) < shieldPickupRadius) {
      shieldCollected = true;
    }
    if (!shieldCollected) {
      mazeVk.m_instances[2].transform = shieldGlobal;
    } else {
      fanStrength = 20.0f;
      glm::vec3 shieldPos = ballPos + glm::vec3(0.8f,-2.5f,2.5f);
      mazeVk.m_instances[2].transform = glm::translate(glm::mat4(1.0f), shieldPos);
    }
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

      mazeVk.m_instances[3].transform = springGlobal * localOffset;
    } 
    else {
      mazeVk.m_instances[3].transform = springGlobal;
    }

    mazeVk.m_instances[4].transform = fanGlobal;

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

      if (mazeVk.intersectBVH(next, ballRadius, R)) {
        bool collided = false;
        glm::vec3 contactN(0.0f);
        float penetration = 0.0f;
        for(auto& tri : mazeVk.mazeTris) {
          glm::vec3 A = glm::vec3(R * glm::vec4(tri.a, 1.0f));
          glm::vec3 B = glm::vec3(R * glm::vec4(tri.b, 1.0f));
          glm::vec3 C = glm::vec3(R * glm::vec4(tri.c, 1.0f));
          glm::vec3 p = mazeVk.closestPointTriangle(next, A, B, C);
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
          next += contactN * penetration;

          glm::vec3 n = contactN;
          glm::vec3 gN = glm::dot(GRAV, n) * n;
          glm::vec3 gT = GRAV - gN;
          glm::vec3 vT = ballVel - glm::dot(ballVel, n) * n;
          if (std::abs(glm::dot(ballVel, n)) < 1e-3) {
            glm::vec3 aT = gT - μ * vT;
            vT += aT * subDt;
            glm::vec3 vN = glm::dot(ballVel, n) * n;
            ballVel = vN + vT;
            rotateAxis = glm::normalize(glm::cross(vT, n));
            rotateVel = vT;
          } else {
            float impulse = calculateImpulse(-1, ballMass, glm::vec3(0.), ballVel, n);
            ballVel += impulse * n / ballMass;
          }
        }
      }

      if(glm::length(next - springStart) < 3.0f) {
        mazeVk.handleCollision(next, R, ballPos, ballVel, ballRadius, mazeVk.springTris, 0.15f, 0.05f);
      }
      
      if(glm::length(next - shieldStart) < 3.0f) {
        mazeVk.handleCollision(next, R, ballPos, ballVel, ballRadius, mazeVk.shieldTris, 0.15f, 0.05f);
      }
      
      if(glm::length(next - fanStart) < 3.0f) {
        mazeVk.handleCollision(next, R, ballPos, ballVel, ballRadius, mazeVk.fanTris, 0.15f, 0.05f);
      }

      float theta = glm::length(rotateVel * subDt) / ballRadius;
      glm::quat rotationQuat = glm::angleAxis(theta, rotateAxis);
      ballRotation = rotationQuat * ballRotation;
      ballPos = next;
    }
    mazeVk.m_instances[1].transform =
            glm::translate(glm::mat4(1.0f), ballPos) *
            glm::mat4_cast(ballRotation);

    mazeVk.createObjDescriptionBuffer();
    mazeVk.updateDescriptorSet();

    if(useRaytracer)
    {
      mazeVk.createTopLevelAS();
      mazeVk.updateRtDescriptorSet();
    }

    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    mazeVk.prepareFrame();

    auto                   curFrame = mazeVk.getCurFrame();
    const VkCommandBuffer& cmdBuf   = mazeVk.getCommandBuffers()[curFrame];

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmdBuf, &beginInfo);

    mazeVk.updateUniformBuffer(cmdBuf);

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color        = {{clearColor[0], clearColor[1], clearColor[2], clearColor[3]}};
    clearValues[1].depthStencil = {1.0f, 0};

    {
      VkRenderPassBeginInfo offscreenRenderPassBeginInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
      offscreenRenderPassBeginInfo.clearValueCount = 2;
      offscreenRenderPassBeginInfo.pClearValues    = clearValues.data();
      offscreenRenderPassBeginInfo.renderPass      = mazeVk.m_offscreenRenderPass;
      offscreenRenderPassBeginInfo.framebuffer     = mazeVk.m_offscreenFramebuffer;
      offscreenRenderPassBeginInfo.renderArea      = {{0, 0}, mazeVk.getSize()};

      if(useRaytracer)
      {
        mazeVk.raytrace(cmdBuf, clearColor);
      }
      else
      {
        vkCmdBeginRenderPass(cmdBuf, &offscreenRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        mazeVk.rasterize(cmdBuf);
        vkCmdEndRenderPass(cmdBuf);
      }
    }

    {
      VkRenderPassBeginInfo postRenderPassBeginInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
      postRenderPassBeginInfo.clearValueCount = 2;
      postRenderPassBeginInfo.pClearValues    = clearValues.data();
      postRenderPassBeginInfo.renderPass      = mazeVk.getRenderPass();
      postRenderPassBeginInfo.framebuffer     = mazeVk.getFramebuffers()[curFrame];
      postRenderPassBeginInfo.renderArea      = {{0, 0}, mazeVk.getSize()};

      vkCmdBeginRenderPass(cmdBuf, &postRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
      mazeVk.drawPost(cmdBuf);
      ImGui::Render();
      ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuf);
      vkCmdEndRenderPass(cmdBuf);
    }

    vkEndCommandBuffer(cmdBuf);
    mazeVk.submitFrame();
  }

  vkDeviceWaitIdle(mazeVk.getDevice());

  mazeVk.destroyResources();
  mazeVk.destroy();
  vkctx.deinit();

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
