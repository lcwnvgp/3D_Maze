#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <GLFW/glfw3.h>
#include "VulkanContext.h"
#include "model.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

extern PFN_vkCmdTraceRaysKHR pfn_vkCmdTraceRaysKHR;

class Application {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow* window;
    VulkanContext context;
    std::vector<Model*> models;

    void initWindow() {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW!");
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Ray Tracing Demo", nullptr, nullptr);
        if (!window) {
            glfwTerminate();
            throw std::runtime_error("Failed to create window!");
        }

        context.initWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Ray Tracing Demo");
    }

    void initVulkan() {
        context.initVulkan();

        // 创建测试模型
        std::string mazePath = R"(D:\vscode\final\models\maze.obj)";
        std::string spherePath = R"(D:\vscode\final\models\sphere.obj)";
        
        Model *mazeModel = new Model(context.getDevice(), context.getPhysicalDevice(), 
                       context.getCommandPool(), context.getGraphicsQueue(),
                       mazePath);
        
        Model *sphereModel = new Model(context.getDevice(), context.getPhysicalDevice(), 
                         context.getCommandPool(), context.getGraphicsQueue(),
                         spherePath);
        models.push_back(sphereModel);

        // 设置模型
        context.setModels(models);

        // 设置相机
        glm::mat4 view = glm::lookAt(
            glm::vec3(0.0f, 0.0f, 0.0f),  // 相机位置
            glm::vec3(0.0f, 0.0f, -1.0f), // 观察点
            glm::vec3(0.0f, 1.0f, 0.0f)   // 上向量
        );
        context.setViewMatrix(view);

        glm::mat4 proj = glm::perspective(
            glm::radians(45.0f),
            static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT),
            0.1f,
            100.0f
        );
        context.setProjectionMatrix(proj);

        // 设置光源
        context.setLightPosition(glm::vec3(0.0f, 5.0f, 0.0f));
        context.setLightColor(glm::vec3(1.0f, 1.0f, 1.0f));
        context.setAmbientStrength(0.1f);
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            drawFrame();
        }

        vkDeviceWaitIdle(context.getDevice());
    }

    void drawFrame() {
        context.beginFrame();

        // 执行光线追踪
        VkCommandBuffer commandBuffer = context.getCommandBuffer();
        
        // 绑定光线追踪管线
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, context.getRayTracingPipeline());
        
        // 绑定描述符集
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                              context.getRayTracingPipelineLayout(), 0, 1,
                              &context.getRayTracingDescriptorSets()[0], 0, nullptr);

        // 执行光线追踪
        VkStridedDeviceAddressRegionKHR raygenShaderBindingTable{};
        raygenShaderBindingTable.deviceAddress = context.getBufferDeviceAddress(context.getRaygenShaderBindingTable());
        raygenShaderBindingTable.stride = context.getRayTracingPipelineProperties().shaderGroupHandleSize;
        raygenShaderBindingTable.size = raygenShaderBindingTable.stride;

        VkStridedDeviceAddressRegionKHR missShaderBindingTable{};
        missShaderBindingTable.deviceAddress = context.getBufferDeviceAddress(context.getMissShaderBindingTable());
        missShaderBindingTable.stride = context.getRayTracingPipelineProperties().shaderGroupHandleSize;
        missShaderBindingTable.size = missShaderBindingTable.stride;

        VkStridedDeviceAddressRegionKHR hitShaderBindingTable{};
        hitShaderBindingTable.deviceAddress = context.getBufferDeviceAddress(context.getHitShaderBindingTable());
        hitShaderBindingTable.stride = context.getRayTracingPipelineProperties().shaderGroupHandleSize;
        hitShaderBindingTable.size = hitShaderBindingTable.stride;

        VkStridedDeviceAddressRegionKHR callableShaderBindingTable{};

        pfn_vkCmdTraceRaysKHR(commandBuffer,
                             &raygenShaderBindingTable,
                             &missShaderBindingTable,
                             &hitShaderBindingTable,
                             &callableShaderBindingTable,
                             WINDOW_WIDTH,
                             WINDOW_HEIGHT,
                             1);

        context.endFrame();
    }

    void cleanup() {
        for (auto model : models) {
            delete model;
        }
        glfwDestroyWindow(window);
        glfwTerminate();
    }
};

int main() {
    Application app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}