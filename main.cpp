#include "VulkanContext.h"
#include "Model.h"
#include "Camera.h"
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <GLFW/glfw3.h>

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto camera = static_cast<Camera*>(glfwGetWindowUserPointer(window));
    if (camera) {
        camera->handleInput(key, action);
    }
}

int main() {
    try {
        VulkanContext context;
        context.initWindow(800, 600, "3D Maze Game");
        context.initVulkan();

        Camera camera;
        glfwSetWindowUserPointer(context.getWindow(), &camera);
        glfwSetKeyCallback(context.getWindow(), keyCallback);

        std::string mazePath = R"(D:\vscode\final\models\maze.obj)";
        std::string spherePath = R"(D:\vscode\final\models\sphere.obj)";
        
        Model mazeModel(context.getDevice(), context.getPhysicalDevice(), 
                       context.getCommandPool(), context.getGraphicsQueue(),
                       mazePath);
        
        Model sphereModel(context.getDevice(), context.getPhysicalDevice(), 
                         context.getCommandPool(), context.getGraphicsQueue(),
                         spherePath);

        auto lastTime = std::chrono::high_resolution_clock::now();

        while (!glfwWindowShouldClose(context.getWindow())) {
            auto currentTime = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
            lastTime = currentTime;

            camera.update(deltaTime);
            glfwPollEvents();

            context.beginFrame();
            VkCommandBuffer commandBuffer = context.beginRenderPass();
            
            UniformBufferObject ubo{};
            ubo.model = glm::mat4(1.0f);
            ubo.view = camera.getViewMatrix();
            ubo.proj = camera.getProjectionMatrix();
            context.updateUniformBuffer(ubo);

            mazeModel.draw(commandBuffer);
            sphereModel.draw(commandBuffer);
            context.endRenderPass();
            context.endFrame();
        }

        vkDeviceWaitIdle(context.getDevice());
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
