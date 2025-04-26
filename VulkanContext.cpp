#include "VulkanContext.h"
#include <stdexcept>
#include <iostream>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <fstream>
#include <cstring>
#include <algorithm>
// #include <vulkan/vulkan_core.h>

// 定义光线追踪相关的函数指针
PFN_vkCmdTraceRaysKHR pfn_vkCmdTraceRaysKHR;
PFN_vkDestroyAccelerationStructureKHR pfn_vkDestroyAccelerationStructureKHR;
PFN_vkCreateRayTracingPipelinesKHR pfn_vkCreateRayTracingPipelinesKHR;
PFN_vkGetRayTracingShaderGroupHandlesKHR pfn_vkGetRayTracingShaderGroupHandlesKHR;

// Extension function pointers for acceleration structures
PFN_vkGetAccelerationStructureBuildSizesKHR pfn_vkGetAccelerationStructureBuildSizesKHR;
PFN_vkCreateAccelerationStructureKHR pfn_vkCreateAccelerationStructureKHR;
PFN_vkCmdBuildAccelerationStructuresKHR pfn_vkCmdBuildAccelerationStructuresKHR;
PFN_vkGetAccelerationStructureDeviceAddressKHR pfn_vkGetAccelerationStructureDeviceAddressKHR;
PFN_vkBindAccelerationStructureMemoryKHR pfn_vkBindAccelerationStructureMemoryKHR;

VulkanContext::VulkanContext(GLFWwindow* window) : window(window) {
}

VulkanContext::~VulkanContext() {
    cleanup();
}

void VulkanContext::initWindow(int width, int height, const std::string& title) {
    std::cout << "Initializing GLFW..." << std::endl;
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }
    
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    
    window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }
    std::cout << "GLFW window created successfully" << std::endl;
}

void VulkanContext::initVulkan() {
    try {
        std::cout << "Creating Vulkan instance..." << std::endl;
        createInstance();
        std::cout << "Vulkan instance created successfully" << std::endl;

        std::cout << "Creating surface..." << std::endl;
        createSurface();
        std::cout << "Surface created successfully" << std::endl;

        std::cout << "Picking physical device..." << std::endl;
        pickPhysicalDevice();
        std::cout << "Physical device selected successfully" << std::endl;

        // 初始化 rayTracingPipelineProperties
        rayTracingPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
        rayTracingPipelineProperties.pNext = nullptr;
        // 查询光线追踪管道属性
        VkPhysicalDeviceProperties2 props2{};
        props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        props2.pNext = &rayTracingPipelineProperties;
        vkGetPhysicalDeviceProperties2(physicalDevice, &props2);

        std::cout << "Creating logical device..." << std::endl;
        createLogicalDevice();
        std::cout << "Logical device created successfully" << std::endl;

        std::cout << "Creating swap chain..." << std::endl;
        createSwapChain();
        std::cout << "Swap chain created successfully" << std::endl;

        std::cout << "Creating image views..." << std::endl;
        createImageViews();
        std::cout << "Image views created successfully" << std::endl;

        std::cout << "Creating command pool..." << std::endl;
        createCommandPool();
        std::cout << "Command pool created successfully" << std::endl;

        std::cout << "Creating sync objects..." << std::endl;
        createSyncObjects();
        std::cout << "Sync objects created successfully" << std::endl;

        std::cout << "Creating acceleration structure..." << std::endl;
        createAccelerationStructure();
        std::cout << "Acceleration structure created successfully" << std::endl;

        std::cout << "Creating storage image..." << std::endl;
        createStorageImage();
        std::cout << "Storage image created successfully" << std::endl;

        std::cout << "Creating ray tracing descriptor set layout..." << std::endl;
        createRayTracingDescriptorSetLayout();
        std::cout << "Ray tracing descriptor set layout created successfully" << std::endl;

        std::cout << "Creating ray tracing pipeline..." << std::endl;
        createRayTracingPipeline();
        std::cout << "Ray tracing pipeline created successfully" << std::endl;

        std::cout << "Creating shader binding table..." << std::endl;
        createShaderBindingTable();
        std::cout << "Shader binding table created successfully" << std::endl;

        std::cout << "Creating ray tracing descriptor sets..." << std::endl;
        createRayTracingDescriptorSets();
        std::cout << "Ray tracing descriptor sets created successfully" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error in initVulkan: " << e.what() << std::endl;
        throw;
    }
}

void VulkanContext::cleanup() {
    if (raygenShaderBindingTableBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, raygenShaderBindingTableBuffer, nullptr);
        vkFreeMemory(device, raygenShaderBindingTableMemory, nullptr);
    }
    if (missShaderBindingTableBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, missShaderBindingTableBuffer, nullptr);
        vkFreeMemory(device, missShaderBindingTableMemory, nullptr);
    }
    if (hitShaderBindingTableBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, hitShaderBindingTableBuffer, nullptr);
        vkFreeMemory(device, hitShaderBindingTableMemory, nullptr);
    }

    for (size_t i = 0; i < bottomLevelASs.size(); ++i) {
        if (bottomLevelASs[i] != VK_NULL_HANDLE)
            pfn_vkDestroyAccelerationStructureKHR(device, bottomLevelASs[i], nullptr);
        if (bottomLevelASBuffers[i] != VK_NULL_HANDLE)
            vkDestroyBuffer(device, bottomLevelASBuffers[i], nullptr);
        if (bottomLevelASMemories[i] != VK_NULL_HANDLE)
            vkFreeMemory(device, bottomLevelASMemories[i], nullptr);
    }

    if (topLevelAS != VK_NULL_HANDLE)
        pfn_vkDestroyAccelerationStructureKHR(device, topLevelAS, nullptr);
    if (topLevelASBuffer != VK_NULL_HANDLE)
        vkDestroyBuffer(device, topLevelASBuffer, nullptr);
    if (topLevelASMemory != VK_NULL_HANDLE)
        vkFreeMemory(device, topLevelASMemory, nullptr);

    if (device != VK_NULL_HANDLE) {
        vkDestroyDevice(device, nullptr);
    }
    if (surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance, surface, nullptr);
    }
    if (instance != VK_NULL_HANDLE) {
        vkDestroyInstance(instance, nullptr);
    }
}


void VulkanContext::createInstance() {
    VkApplicationInfo appInfo{};  
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Ray Tracing Demo";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    bool layersSupported = true;
    for (const char* layerName : validationLayers) {
        bool layerFound = false;
        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }
        if (!layerFound) {
            layersSupported = false;
            break;
        }
    }

    if (!layersSupported) {
        throw std::runtime_error("Validation layers requested, but not available!");
    }

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    
    if (!glfwExtensions) {
        throw std::runtime_error("Failed to get required GLFW extensions!");
    }

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

    VkInstanceCreateInfo createInfo{};  
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();

    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance! Error code: " + std::to_string(result));
    }
}

void VulkanContext::createSurface() {
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface!");
    }
}

void VulkanContext::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) throw std::runtime_error("No Vulkan-supported GPU found!");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (const auto& device : devices) {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        
        try {
            uint32_t queueFamilyIndex = findQueueFamily(device);
            
            uint32_t extensionCount;
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
            std::vector<VkExtensionProperties> availableExtensions(extensionCount);
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
            
            bool swapChainSupported = false;
            bool rayTracingSupported = false;
            for (const auto& extension : availableExtensions) {
                if (strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                    swapChainSupported = true;
                }
                if (strcmp(extension.extensionName, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) == 0) {
                    rayTracingSupported = true;
                }
            }
            
            if (!swapChainSupported || !rayTracingSupported) {
                continue;
            }

            if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                physicalDevice = device;
                std::cout << "Selected discrete GPU: " << deviceProperties.deviceName << std::endl;
                return;
            }
        } catch (const std::exception&) {
            continue;
        }
    }

    throw std::runtime_error("No suitable GPU found!");
}

void VulkanContext::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};  
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{};
    bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    bufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures{};
    rayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
    rayTracingPipelineFeatures.pNext = &bufferDeviceAddressFeatures;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{};
    accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    accelerationStructureFeatures.accelerationStructure = VK_TRUE;
    accelerationStructureFeatures.pNext = &rayTracingPipelineFeatures;

    VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures{};
    descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    descriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;
    descriptorIndexingFeatures.pNext = &accelerationStructureFeatures;

    VkDeviceCreateInfo createInfo{};  
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();
    createInfo.pNext = &descriptorIndexingFeatures;
    
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device!");
    }

    // 加载必要的函数指针
    pfn_vkCmdTraceRaysKHR = (PFN_vkCmdTraceRaysKHR)vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR");
    pfn_vkCreateRayTracingPipelinesKHR = (PFN_vkCreateRayTracingPipelinesKHR)vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR");
    pfn_vkGetRayTracingShaderGroupHandlesKHR = (PFN_vkGetRayTracingShaderGroupHandlesKHR)vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR");
    pfn_vkGetAccelerationStructureBuildSizesKHR = (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR");
    pfn_vkCreateAccelerationStructureKHR = (PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR");
    pfn_vkCmdBuildAccelerationStructuresKHR = (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR");
    pfn_vkGetAccelerationStructureDeviceAddressKHR = (PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR");

    // 检查必要的函数指针是否成功加载
    if (!pfn_vkCmdTraceRaysKHR) {
        throw std::runtime_error("Failed to load vkCmdTraceRaysKHR");
    }
    if (!pfn_vkCreateRayTracingPipelinesKHR) {
        throw std::runtime_error("Failed to load vkCreateRayTracingPipelinesKHR");
    }
    if (!pfn_vkGetRayTracingShaderGroupHandlesKHR) {
        throw std::runtime_error("Failed to load vkGetRayTracingShaderGroupHandlesKHR");
    }
    if (!pfn_vkGetAccelerationStructureBuildSizesKHR) {
        throw std::runtime_error("Failed to load vkGetAccelerationStructureBuildSizesKHR");
    }
    if (!pfn_vkCreateAccelerationStructureKHR) {
        throw std::runtime_error("Failed to load vkCreateAccelerationStructureKHR");
    }
    if (!pfn_vkCmdBuildAccelerationStructuresKHR) {
        throw std::runtime_error("Failed to load vkCmdBuildAccelerationStructuresKHR");
    }
    if (!pfn_vkGetAccelerationStructureDeviceAddressKHR) {
        throw std::runtime_error("Failed to load vkGetAccelerationStructureDeviceAddressKHR");
    }

    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}

void VulkanContext::createSwapChain() {
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);

    uint32_t width = WINDOW_WIDTH;
    uint32_t height = WINDOW_HEIGHT;

    if (width < capabilities.minImageExtent.width) width = capabilities.minImageExtent.width;
    if (width > capabilities.maxImageExtent.width) width = capabilities.maxImageExtent.width;
    if (height < capabilities.minImageExtent.height) height = capabilities.minImageExtent.height;
    if (height > capabilities.maxImageExtent.height) height = capabilities.maxImageExtent.height;

    VkExtent2D actualExtent = {width, height};

    swapChainImageFormat = VK_FORMAT_B8G8R8A8_SRGB;
    swapChainExtent = actualExtent;

    VkSwapchainCreateInfoKHR createInfo{};  
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = 2;
    createInfo.imageFormat = swapChainImageFormat;
    createInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    createInfo.imageExtent = swapChainExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    createInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swap chain!");
    }

    uint32_t imageCount;
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
}

void VulkanContext::createImageViews() {
    swapChainImageViews.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainImages.size(); i++) {
        VkImageViewCreateInfo createInfo{};  
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapChainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapChainImageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image views!");
        }
    }
}

void VulkanContext::createCommandPool() {
    uint32_t queueFamilyIndex = findQueueFamily(physicalDevice);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndex;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool!");
    }
}

void VulkanContext::createSyncObjects() {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create synchronization objects!");
        }
    }
}

void VulkanContext::createAccelerationStructure() {
    // 清理旧的BLAS
    std::cout << "number of models: " << models.size() << std::endl;
    for (size_t i = 0; i < bottomLevelASs.size(); ++i) {
        if (bottomLevelASs[i] != VK_NULL_HANDLE) {
            pfn_vkDestroyAccelerationStructureKHR(device, bottomLevelASs[i], nullptr);
            bottomLevelASs[i] = VK_NULL_HANDLE;
        }
        if (bottomLevelASBuffers[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, bottomLevelASBuffers[i], nullptr);
            bottomLevelASBuffers[i] = VK_NULL_HANDLE;
        }
        if (bottomLevelASMemories[i] != VK_NULL_HANDLE) {
            vkFreeMemory(device, bottomLevelASMemories[i], nullptr);
            bottomLevelASMemories[i] = VK_NULL_HANDLE;
        }
    }
    bottomLevelASs.clear();
    bottomLevelASBuffers.clear();
    bottomLevelASMemories.clear();

    // 1) 为每个模型准备 Geometry 描述
    std::vector<VkAccelerationStructureBuildGeometryInfoKHR> buildInfos;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRanges;
    buildInfos.reserve(models.size());
    buildRanges.reserve(models.size());

    for (Model* m : models) {
        VkDeviceAddress vertexAddress = getBufferDeviceAddress(m->getVertexBuffer());
        VkDeviceAddress indexAddress  = getBufferDeviceAddress(m->getIndexBuffer());

        VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
        triangles.sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        triangles.vertexData.deviceAddress = vertexAddress;
        triangles.vertexStride = sizeof(Vertex);
        triangles.indexType    = VK_INDEX_TYPE_UINT32;
        triangles.indexData.deviceAddress  = indexAddress;
        triangles.maxVertex    = static_cast<uint32_t>(m->GetVertices().size() - 1);

        VkAccelerationStructureGeometryKHR asGeometry{};
        asGeometry.sType          = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        asGeometry.geometryType   = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        asGeometry.flags          = VK_GEOMETRY_OPAQUE_BIT_KHR;
        asGeometry.geometry.triangles = triangles;

        VkAccelerationStructureBuildRangeInfoKHR buildRange{};
        buildRange.primitiveCount  = static_cast<uint32_t>(m->GetIndices().size() / 3);
        buildRange.primitiveOffset = 0;
        buildRange.firstVertex     = 0;
        buildRange.transformOffset = 0;

        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
        buildInfo.sType                    = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildInfo.type                     = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        buildInfo.flags                    = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR
                                          | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
        buildInfo.geometryCount            = 1;
        buildInfo.pGeometries              = &asGeometry;
        buildInfo.ppGeometries             = nullptr; 
        buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
        buildInfo.dstAccelerationStructure = VK_NULL_HANDLE;
        buildInfo.scratchData.deviceAddress = 0;

        buildInfos.push_back(buildInfo);
        buildRanges.push_back(buildRange);
    }

    // 2) 查询每个 BLAS 所需内存大小
    VkDeviceSize maxScratchSize = 0;
    std::vector<VkAccelerationStructureBuildSizesInfoKHR> sizeInfos(models.size());
    for (size_t i = 0; i < models.size(); ++i) {
        sizeInfos[i].sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        pfn_vkGetAccelerationStructureBuildSizesKHR(
            device,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildInfos[i],
            &buildRanges[i].primitiveCount,
            &sizeInfos[i]
        );
        maxScratchSize = std::max<VkDeviceSize>(maxScratchSize, sizeInfos[i].buildScratchSize);
    }

    // 3) 创建 scratch buffer
    VkBuffer scratchBuffer;
    VkDeviceMemory scratchMemory;
    createBuffer(
        maxScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        scratchBuffer, scratchMemory
    );
    VkDeviceAddress scratchAddress = getBufferDeviceAddress(scratchBuffer);

    // 4) 创建 BLAS buffers 和 AS
    for (size_t i = 0; i < models.size(); ++i) {
        VkBuffer blasBuffer;
        VkDeviceMemory blasMemory;
        createBuffer(
            sizeInfos[i].accelerationStructureSize,
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            blasBuffer, blasMemory
        );

        VkAccelerationStructureCreateInfoKHR createInfo{};
        createInfo.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        createInfo.buffer = blasBuffer;
        createInfo.size   = sizeInfos[i].accelerationStructureSize;
        createInfo.type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

        VkAccelerationStructureKHR blas;
        pfn_vkCreateAccelerationStructureKHR(device, &createInfo, nullptr, &blas);

        bottomLevelASs.push_back(blas);
        bottomLevelASBuffers.push_back(blasBuffer);
        bottomLevelASMemories.push_back(blasMemory);

        buildInfos[i].dstAccelerationStructure = blas;
        buildInfos[i].scratchData.deviceAddress = scratchAddress;
    }

    // 5) 构建 BLAS
    VkCommandBuffer cmd = beginSingleTimeCommands();
    for (size_t i = 0; i < models.size(); ++i) {
        const VkAccelerationStructureBuildRangeInfoKHR* pRanges[] = { &buildRanges[i] };
        pfn_vkCmdBuildAccelerationStructuresKHR(
            cmd,
            1,
            &buildInfos[i],
            pRanges
        );
    }
    endSingleTimeCommands(cmd);

    // 6) 构建 TLAS
    // 准备 instances 数组
    std::vector<VkAccelerationStructureInstanceKHR> instances(models.size());
    for (size_t i = 0; i < models.size(); ++i) {
        VkAccelerationStructureInstanceKHR instance{};
        VkTransformMatrixKHR transform = { { {1.0f,0,0,0}, {0,1.0f,0,0}, {0,0,1.0f,0} } };
        instance.transform = transform;
        instance.instanceCustomIndex = static_cast<uint32_t>(i);
        instance.mask             = 0xFF;
        instance.instanceShaderBindingTableRecordOffset = 0;
        instance.flags            = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instance.accelerationStructureReference = getAccelerationStructureDeviceAddress(bottomLevelASs[i]);

        instances[i] = instance;
    }

    // 创建 instance buffer
    VkBuffer       instanceBuf;
    VkDeviceMemory instanceMem;
    createBuffer(
        sizeof(VkAccelerationStructureInstanceKHR) * instances.size(),
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        instanceBuf, instanceMem
    );

    // 填充 instance 数据
    void* data;
    vkMapMemory(device, instanceMem, 0, VK_WHOLE_SIZE, 0, &data);
    memcpy(data, instances.data(), instances.size() * sizeof(instances[0]));
    vkUnmapMemory(device, instanceMem);

    // TLAS build info
    VkDeviceAddress instBufferAddr = getBufferDeviceAddress(instanceBuf);
    VkAccelerationStructureGeometryInstancesDataKHR instancesData{};
    instancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instancesData.arrayOfPointers = VK_FALSE;
    instancesData.data.deviceAddress = instBufferAddr;

    VkAccelerationStructureGeometryKHR topAsGeom{};
    topAsGeom.sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    topAsGeom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    topAsGeom.geometry.instances = instancesData;

    VkAccelerationStructureBuildRangeInfoKHR topRange{};
    topRange.primitiveCount = static_cast<uint32_t>(instances.size());
    topRange.primitiveOffset = 0;
    topRange.firstVertex     = 0;
    topRange.transformOffset = 0;

    VkAccelerationStructureBuildGeometryInfoKHR topBuildInfo{};
    topBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    topBuildInfo.type  = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    topBuildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    topBuildInfo.geometryCount = 1;
    topBuildInfo.pGeometries   = &topAsGeom;

    // 查询 TLAS 大小
    VkAccelerationStructureBuildSizesInfoKHR topSizeInfo{};
    topSizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    pfn_vkGetAccelerationStructureBuildSizesKHR(
        device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &topBuildInfo,
        &topRange.primitiveCount,
        &topSizeInfo
    );

    // 创建 TLAS Buffer & AS
    createBuffer(
        topSizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        topLevelASBuffer, topLevelASMemory
    );

    VkAccelerationStructureCreateInfoKHR topCreateInfo{};
    topCreateInfo.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    topCreateInfo.buffer = topLevelASBuffer;
    topCreateInfo.size   = topSizeInfo.accelerationStructureSize;
    topCreateInfo.type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    pfn_vkCreateAccelerationStructureKHR(device, &topCreateInfo, nullptr, &topLevelAS);

    // TLAS build
    topBuildInfo.dstAccelerationStructure = topLevelAS;
    topBuildInfo.scratchData.deviceAddress = scratchAddress;

    VkCommandBuffer cmd2 = beginSingleTimeCommands();
    const VkAccelerationStructureBuildRangeInfoKHR* pTopRanges[] = { &topRange };
    pfn_vkCmdBuildAccelerationStructuresKHR(
        cmd2,
        1,
        &topBuildInfo,
        pTopRanges
    );
    endSingleTimeCommands(cmd2);

    // 7) 清理临时资源
    vkDestroyBuffer(device, scratchBuffer, nullptr);
    vkFreeMemory(device, scratchMemory, nullptr);
    vkDestroyBuffer(device, instanceBuf, nullptr);
    vkFreeMemory(device, instanceMem, nullptr);
}


void VulkanContext::createStorageImage() {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = WINDOW_WIDTH;
    imageInfo.extent.height = WINDOW_HEIGHT;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &storageImage) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create storage image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, storageImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkMemoryAllocateFlagsInfo allocFlags{};
    allocFlags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    allocFlags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    allocInfo.pNext = &allocFlags;

    if (vkAllocateMemory(device, &allocInfo, nullptr, &storageImageMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate storage image memory!");
    }

    vkBindImageMemory(device, storageImage, storageImageMemory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = storageImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &storageImageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create storage image view!");
    }
}

void VulkanContext::createRayTracingDescriptorSetLayout() {
    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        // 0: Top-level acceleration structure
        {
            0,
            VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            1,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            nullptr
        },
        // 1: Camera data
        {
            1,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            1,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            nullptr
        },
        // 2: Storage image
        {
            2,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            1,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            nullptr
        },
        // 3: Scene data
        {
            3,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            1,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
            nullptr
        },
        // 4: Light data
        {
            4,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            1,
            VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
            nullptr
        }
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &rayTracingDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ray tracing descriptor set layout!");
    }
}

void VulkanContext::createRayTracingPipeline() {
    // 加载着色器
    std::vector<char> raygenShaderCode = readFile("shaders/raygen.rgen.spv");
    std::vector<char> missShaderCode = readFile("shaders/miss.rmiss.spv");
    std::vector<char> closestHitShaderCode = readFile("shaders/closesthit.rchit.spv");

    VkShaderModule raygenShaderModule = createShaderModule(raygenShaderCode);
    VkShaderModule missShaderModule = createShaderModule(missShaderCode);
    VkShaderModule closestHitShaderModule = createShaderModule(closestHitShaderCode);

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages = {
        // Ray generation
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            0,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            raygenShaderModule,
            "main",
            nullptr
        },
        // Miss
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            0,
            VK_SHADER_STAGE_MISS_BIT_KHR,
            missShaderModule,
            "main",
            nullptr
        },
        // Closest hit
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            0,
            VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
            closestHitShaderModule,
            "main",
            nullptr
        }
    };

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups = {
        // Ray generation group
        {
            VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            nullptr,
            VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            0,
            VK_SHADER_UNUSED_KHR,
            VK_SHADER_UNUSED_KHR,
            VK_SHADER_UNUSED_KHR
        },
        // Miss group
        {
            VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            nullptr,
            VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            1,
            VK_SHADER_UNUSED_KHR,
            VK_SHADER_UNUSED_KHR,
            VK_SHADER_UNUSED_KHR
        },
        // Closest hit group
        {
            VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            nullptr,
            VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
            VK_SHADER_UNUSED_KHR,
            2,
            VK_SHADER_UNUSED_KHR,
            VK_SHADER_UNUSED_KHR
        }
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &rayTracingDescriptorSetLayout;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &rayTracingPipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ray tracing pipeline layout!");
    }

    VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.groupCount = static_cast<uint32_t>(shaderGroups.size());
    pipelineInfo.pGroups = shaderGroups.data();
    pipelineInfo.maxPipelineRayRecursionDepth = 1;
    pipelineInfo.layout = rayTracingPipelineLayout;

    if (pfn_vkCreateRayTracingPipelinesKHR(device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &rayTracingPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ray tracing pipeline!");
    }

    vkDestroyShaderModule(device, raygenShaderModule, nullptr);
    vkDestroyShaderModule(device, missShaderModule, nullptr);
    vkDestroyShaderModule(device, closestHitShaderModule, nullptr);
}

void VulkanContext::createShaderBindingTable() {
    const uint32_t handleSize = rayTracingPipelineProperties.shaderGroupHandleSize;
    const uint32_t handleSizeAligned = (handleSize + rayTracingPipelineProperties.shaderGroupHandleAlignment - 1) & ~(rayTracingPipelineProperties.shaderGroupHandleAlignment - 1);
    const uint32_t groupCount = 3; // raygen, miss, closesthit
    const uint32_t sbtSize = groupCount * handleSizeAligned;

    std::vector<uint8_t> shaderHandleStorage(sbtSize);
    if (pfn_vkGetRayTracingShaderGroupHandlesKHR(device, rayTracingPipeline, 0, groupCount, sbtSize, shaderHandleStorage.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to get ray tracing shader group handles!");
    }

    // Create raygen shader binding table
    createBuffer(sbtSize, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                raygenShaderBindingTableBuffer, raygenShaderBindingTableMemory);

    void* data;
    vkMapMemory(device, raygenShaderBindingTableMemory, 0, sbtSize, 0, &data);
    memcpy(data, shaderHandleStorage.data(), handleSize);
    vkUnmapMemory(device, raygenShaderBindingTableMemory);

    // Create miss shader binding table
    createBuffer(sbtSize, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                missShaderBindingTableBuffer, missShaderBindingTableMemory);

    vkMapMemory(device, missShaderBindingTableMemory, 0, sbtSize, 0, &data);
    memcpy(data, shaderHandleStorage.data() + handleSizeAligned, handleSize);
    vkUnmapMemory(device, missShaderBindingTableMemory);

    // Create hit shader binding table
    createBuffer(sbtSize, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                hitShaderBindingTableBuffer, hitShaderBindingTableMemory);

    vkMapMemory(device, hitShaderBindingTableMemory, 0, sbtSize, 0, &data);
    memcpy(data, shaderHandleStorage.data() + 2 * handleSizeAligned, handleSize);
    vkUnmapMemory(device, hitShaderBindingTableMemory);
}

void VulkanContext::createRayTracingDescriptorSets() {
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 }
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &rayTracingDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ray tracing descriptor pool!");
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = rayTracingDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &rayTracingDescriptorSetLayout;

    rayTracingDescriptorSets.resize(1);
    if (vkAllocateDescriptorSets(device, &allocInfo, rayTracingDescriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate ray tracing descriptor sets!");
    }

    // 更新描述符集
    VkWriteDescriptorSetAccelerationStructureKHR accelStructInfo{};
    accelStructInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    accelStructInfo.accelerationStructureCount = 1;
    accelStructInfo.pAccelerationStructures = &topLevelAS;

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageView = storageImageView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    std::vector<VkWriteDescriptorSet> descriptorWrites = {
        // 0: Top-level acceleration structure
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            &accelStructInfo,
            rayTracingDescriptorSets[0],
            0,
            0,
            1,
            VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            nullptr,
            nullptr,
            nullptr
        },
        // 2: Storage image
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            rayTracingDescriptorSets[0],
            2,
            0,
            1,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            &imageInfo,
            nullptr,
            nullptr
        }
    };

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void VulkanContext::beginFrame() {
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &inFlightFences[currentFrame]);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, 
                                          imageAvailableSemaphores[currentFrame], 
                                          VK_NULL_HANDLE, &imageIndex);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    vkResetCommandBuffer(commandBuffers[currentFrame], 0);
    this->imageIndex = imageIndex;

    // 更新光线追踪uniform buffer
    updateRayTracingUniformBuffer(currentFrame);
}

void VulkanContext::endFrame() {
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    VkResult result = vkQueuePresentKHR(graphicsQueue, &presentInfo);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swap chain image!");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanContext::updateRayTracingUniformBuffer(uint32_t currentImage) {
    // 更新相机数据
    struct CameraData {
        glm::mat4 viewInverse;
        glm::mat4 projInverse;
    } cameraData;

    cameraData.viewInverse = glm::inverse(viewMatrix);
    cameraData.projInverse = glm::inverse(projectionMatrix);

    void* data;
    vkMapMemory(device, rayTracingUniformBuffersMemory[currentImage], 0, sizeof(CameraData), 0, &data);
    memcpy(data, &cameraData, sizeof(CameraData));
    vkUnmapMemory(device, rayTracingUniformBuffersMemory[currentImage]);

    // 更新场景数据
    struct SceneData {
        int numSpheres;
        struct Sphere {
            glm::vec3 center;
            float radius;
            glm::vec3 color;
        } spheres[10];
    } sceneData;

    sceneData.numSpheres = static_cast<int>(models.size());
    for (size_t i = 0; i < models.size(); ++i) {
        sceneData.spheres[i].center = models[i]->getPosition();
        sceneData.spheres[i].radius = models[i]->getScale().x;
        sceneData.spheres[i].color = glm::vec3(1.0f);
    }

    vkMapMemory(device, rayTracingUniformBuffersMemory[currentImage], sizeof(CameraData), sizeof(SceneData), 0, &data);
    memcpy(data, &sceneData, sizeof(SceneData));
    vkUnmapMemory(device, rayTracingUniformBuffersMemory[currentImage]);

    // 更新光源数据
    struct LightData {
        glm::vec3 position;
        glm::vec3 color;
        float intensity;
    } lightData;

    lightData.position = lightPosition;
    lightData.color = lightColor;
    lightData.intensity = 1.0f;

    vkMapMemory(device, rayTracingUniformBuffersMemory[currentImage], sizeof(CameraData) + sizeof(SceneData), sizeof(LightData), 0, &data);
    memcpy(data, &lightData, sizeof(LightData));
    vkUnmapMemory(device, rayTracingUniformBuffersMemory[currentImage]);
}

VkDeviceAddress VulkanContext::getBufferDeviceAddress(VkBuffer buffer) {
    VkBufferDeviceAddressInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    info.buffer = buffer;
    return vkGetBufferDeviceAddress(device, &info);
}

QueueFamilyIndices VulkanContext::findQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }
    }

    return indices;
}

uint32_t VulkanContext::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
}

void VulkanContext::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, 
                               VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    VkMemoryAllocateFlagsInfo allocFlags{};
    allocFlags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    allocFlags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    allocInfo.pNext = &allocFlags;

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

std::vector<char> VulkanContext::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

VkShaderModule VulkanContext::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module!");
    }

    return shaderModule;
}

void VulkanContext::drawFrame() {
    VkCommandBuffer commandBuffer = commandBuffers[currentFrame];
    
    VkStridedDeviceAddressRegionKHR raygenShaderBindingTable{};
    raygenShaderBindingTable.deviceAddress = getBufferDeviceAddress(raygenShaderBindingTableBuffer);
    raygenShaderBindingTable.stride = rayTracingPipelineProperties.shaderGroupHandleSize;
    raygenShaderBindingTable.size = raygenShaderBindingTable.stride;

    VkStridedDeviceAddressRegionKHR missShaderBindingTable{};
    missShaderBindingTable.deviceAddress = getBufferDeviceAddress(missShaderBindingTableBuffer);
    missShaderBindingTable.stride = rayTracingPipelineProperties.shaderGroupHandleSize;
    missShaderBindingTable.size = missShaderBindingTable.stride;

    VkStridedDeviceAddressRegionKHR hitShaderBindingTable{};
    hitShaderBindingTable.deviceAddress = getBufferDeviceAddress(hitShaderBindingTableBuffer);
    hitShaderBindingTable.stride = rayTracingPipelineProperties.shaderGroupHandleSize;
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
}

uint32_t VulkanContext::findQueueFamily(VkPhysicalDevice device) {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable queue family!");
}

// Helper for one-time command buffers
VkCommandBuffer VulkanContext::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}

void VulkanContext::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

VkBuffer VulkanContext::createScratchBuffer(VkDeviceSize size) {
    VkBuffer scratchBuffer;
    VkDeviceMemory scratchMemory;
    createBuffer(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchMemory);
    return scratchBuffer;
}

VkDeviceAddress VulkanContext::getAccelerationStructureDeviceAddress(VkAccelerationStructureKHR as) {
    VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    addressInfo.accelerationStructure = as;
    return pfn_vkGetAccelerationStructureDeviceAddressKHR(device, &addressInfo);
}

