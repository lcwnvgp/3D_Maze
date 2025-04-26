#include "VulkanContext.h"
#include "Model.h"
#include "Camera.h"
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

static bool followMode = false;
static glm::vec3 spherePos = glm::vec3(0.0f);

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
        followMode = !followMode;
    }

    Camera* camera = static_cast<Camera*>(glfwGetWindowUserPointer(window));

    if (camera && !followMode) {
        camera->handleInput(key, action);
    }
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    Camera* camera = static_cast<Camera*>(glfwGetWindowUserPointer(window));
    if (camera) {
        camera->setMouseButton(button, action == GLFW_PRESS);
    }
}

void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    Camera* camera = static_cast<Camera*>(glfwGetWindowUserPointer(window));
    if (camera) {
        camera->handleMouseMovement(xpos, ypos);
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    Camera* camera = static_cast<Camera*>(glfwGetWindowUserPointer(window));
    if (camera) {
        camera->handleMouseScroll(yoffset);
    }
}

int main() {
    try {
        VulkanContext context;
        context.initWindow(1600, 1200, "3D Maze Game");
        context.initVulkan();

        Camera camera;
        glfwSetWindowUserPointer(context.getWindow(), &camera);
        glfwSetKeyCallback(context.getWindow(), key_callback);
        glfwSetMouseButtonCallback(context.getWindow(), mouse_button_callback);
        glfwSetCursorPosCallback(context.getWindow(), cursor_pos_callback);
        glfwSetScrollCallback(context.getWindow(), scroll_callback);
        glfwSetInputMode(context.getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);

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

            glm::vec3 sphereWorldPos(10.0f, 0.0f, 5.0f);
            glm::mat4 modelMatrix_sphere = glm::translate(glm::mat4(1.0f), sphereWorldPos);

            spherePos = sphereWorldPos;

            UniformBufferObject uboFrame{};

            int width, height;
            glfwGetFramebufferSize(context.getWindow(), &width, &height);
            if (width > 0 && height > 0) {
                camera.setPerspective(45.0f, (float)width / (float)height, 0.1f, 1000.0f);
            }

            if (followMode) {
                glm::vec3 camOffset(-5,5,0);
                glm::vec3 camPos = spherePos + camOffset;
                uboFrame.view = glm::lookAt(camPos, spherePos, {0,1,0});
            } else {
                uboFrame.view = camera.getViewMatrix();
            }

            uboFrame.proj = camera.getProjectionMatrix();

            context.updateUniformBuffer(uboFrame);

            glm::mat4 modelMatrix_maze = glm::mat4(1.0f);
            modelMatrix_maze = glm::scale(modelMatrix_maze, glm::vec3(2.0f, 2.0f, 2.0f));
            context.pushModelMatrix(commandBuffer, modelMatrix_maze);
            
            mazeModel.draw(commandBuffer);

            context.pushModelMatrix(commandBuffer, modelMatrix_sphere);
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