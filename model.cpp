#include "model.h"
#include "tiny_obj_loader.h"
#include <unordered_map>
#include <iostream>
#include <array>
#include <stdexcept>

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& v) const {
            size_t h1 = std::hash<float>()(v.position.x) ^ std::hash<float>()(v.position.y) << 1 ^ std::hash<float>()(v.position.z) << 2;
            return h1;
        }
    };
}

bool operator==(const Vertex& a, const Vertex& b) {
    return a.position == b.position;
}

// BoundingBox方法实现
bool BoundingBox::intersectSphere(const glm::vec3& center, float radius) const {
    glm::vec3 closest = glm::max(min, glm::min(center, max));
    glm::vec3 diff = closest - center;
    float distSquared = glm::dot(diff, diff);
    return distSquared <= radius * radius;
}

BoundingBox BoundingBox::fromModel(const std::vector<Vertex>& vertices, const glm::mat4& transform) {
    BoundingBox box{glm::vec3(FLT_MAX), glm::vec3(-FLT_MAX)};
    for (const auto& vertex : vertices) {
        glm::vec3 pos = glm::vec3(transform * glm::vec4(vertex.position, 1.0f));
        box.min = glm::min(box.min, pos);
        box.max = glm::max(box.max, pos);
    }
    return box;
}

Model::Model(VkDevice device, VkPhysicalDevice physicalDevice, 
             VkCommandPool commandPool, VkQueue graphicsQueue,
             const std::string& modelPath) 
    : device(device), commandPool(commandPool), graphicsQueue(graphicsQueue) {
    loadModel(modelPath);
    createVertexBuffer(physicalDevice);
    initCollisionData(); // 初始化碰撞检测数据
}

Model::~Model() {
    vkDestroyBuffer(device, vertexBuffer, nullptr);
    vkFreeMemory(device, vertexBufferMemory, nullptr);
}

void Model::loadModel(const std::string& modelPath) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, modelPath.c_str())) {
        throw std::runtime_error(warn + err);
    }

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            vertex.position = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            vertex.normal = {
                attrib.normals[3 * index.normal_index + 0],
                attrib.normals[3 * index.normal_index + 1],
                attrib.normals[3 * index.normal_index + 2]
            };

            vertex.texCoord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
            };

            vertices.push_back(vertex);
        }
    }
}

void Model::createVertexBuffer(VkPhysicalDevice physicalDevice) {
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    createBuffer(physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

    copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void Model::createBuffer(VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                        VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
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
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

uint32_t Model::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && 
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

void Model::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
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

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void Model::draw(VkCommandBuffer commandBuffer) {
    VkBuffer vertexBuffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdDraw(commandBuffer, static_cast<uint32_t>(vertices.size()), 1, 0, 0);
}

// 以下是碰撞检测相关方法实现

void Model::initCollisionData() {
    // 1. 计算BoundingBox
    boundingBox = BoundingBox::fromModel(vertices, glm::mat4(1.0f));
    
    // 2. 构建BVH
    buildBVH();
}

void Model::updateBoundingBox(const glm::mat4& transform) {
    boundingBox = BoundingBox::fromModel(vertices, transform);
}

void Model::buildBVH() {
    // 根据三角形数量初始化
    int numTriangles = vertices.size() / 3;
    triangleIndices.resize(numTriangles);
    for (int i = 0; i < numTriangles; i++) {
        triangleIndices[i] = i;
    }
    
    // 初始化BVH节点数组，预分配空间
    bvhNodes.clear();
    // 最坏情况下，BVH树可能需要2*numTriangles-1个节点
    bvhNodes.reserve(2 * numTriangles);
    
    // 如果没有三角形，创建一个空的根节点并返回
    if (numTriangles == 0) {
        bvhNodes.push_back({boundingBox, 0, 0, -1, -1});
        return;
    }
    
    // 计算所有三角形的AABB
    std::vector<BoundingBox> triangleBounds(numTriangles);
    for (int i = 0; i < numTriangles; i++) {
        int idx = triangleIndices[i] * 3;
        glm::vec3 v0 = vertices[idx].position;
        glm::vec3 v1 = vertices[idx + 1].position;
        glm::vec3 v2 = vertices[idx + 2].position;
        
        triangleBounds[i].min = glm::min(glm::min(v0, v1), v2);
        triangleBounds[i].max = glm::max(glm::max(v0, v1), v2);
    }
    
    // 创建根节点
    int rootNodeIdx = 0;
    bvhNodes.push_back({boundingBox, 0, numTriangles, -1, -1});
    
    // 递归构建BVH树
    buildBVHRecursive(0, 0, numTriangles, triangleBounds);
}

// 递归构建BVH树的辅助方法
void Model::buildBVHRecursive(int nodeIdx, int start, int count, const std::vector<BoundingBox>& triangleBounds) {
    // 如果只有少量三角形，就创建叶子节点
    if (count <= 4) {
        // 已经是叶子节点，不需要再分割
        bvhNodes[nodeIdx].firstTriIndex = start;
        bvhNodes[nodeIdx].triCount = count;
        return;
    }
    
    // 计算当前节点所有三角形的总AABB
    BoundingBox nodeBounds;
    nodeBounds.min = glm::vec3(FLT_MAX);
    nodeBounds.max = glm::vec3(-FLT_MAX);
    
    for (int i = 0; i < count; i++) {
        int triIdx = triangleIndices[start + i];
        nodeBounds.min = glm::min(nodeBounds.min, triangleBounds[triIdx].min);
        nodeBounds.max = glm::max(nodeBounds.max, triangleBounds[triIdx].max);
    }
    
    // 找到AABB最长的轴
    glm::vec3 extent = nodeBounds.max - nodeBounds.min;
    int axis = 0;
    if (extent.y > extent.x) axis = 1;
    if (extent.z > extent[axis]) axis = 2;
    
    // 如果最长轴上的尺寸太小，创建叶子节点
    if (extent[axis] < 0.0001f) {
        bvhNodes[nodeIdx].firstTriIndex = start;
        bvhNodes[nodeIdx].triCount = count;
        return;
    }
    
    // 按选定轴的中点进行分割
    float splitPos = nodeBounds.min[axis] + extent[axis] * 0.5f;
    
    // 对三角形进行分区（类似快速排序的分区操作）
    int mid = start;
    for (int i = start; i < start + count; i++) {
        int triIdx = triangleIndices[i];
        // 计算三角形中心点
        float centroid = (triangleBounds[triIdx].min[axis] + triangleBounds[triIdx].max[axis]) * 0.5f;
        
        if (centroid < splitPos) {
            // 交换三角形索引
            std::swap(triangleIndices[i], triangleIndices[mid]);
            mid++;
        }
    }
    
    // 如果分区失败（所有三角形都在一侧），使用中点分割
    if (mid == start || mid == start + count) {
        mid = start + count / 2;
    }
    
    // 创建子节点
    int leftChildIdx = bvhNodes.size();
    bvhNodes.push_back({nodeBounds, 0, 0, -1, -1}); // 左子节点占位
    
    int rightChildIdx = bvhNodes.size();
    bvhNodes.push_back({nodeBounds, 0, 0, -1, -1}); // 右子节点占位
    
    // 更新当前节点信息
    bvhNodes[nodeIdx].leftChild = leftChildIdx;
    bvhNodes[nodeIdx].rightChild = rightChildIdx;
    bvhNodes[nodeIdx].firstTriIndex = -1; // 非叶子节点
    bvhNodes[nodeIdx].triCount = 0;
    
    // 递归构建子树
    int leftCount = mid - start;
    buildBVHRecursive(leftChildIdx, start, leftCount, triangleBounds);
    buildBVHRecursive(rightChildIdx, mid, count - leftCount, triangleBounds);
}

bool Model::sphereTriangleCollision(const glm::vec3& center, float radius,
                              int triIndex, const glm::mat4& transform,
                              CollisionInfo* info) const {
    // 获取三角形顶点
    int baseIdx = triIndex * 3;
    glm::vec3 v0 = glm::vec3(transform * glm::vec4(vertices[baseIdx].position, 1.0f));
    glm::vec3 v1 = glm::vec3(transform * glm::vec4(vertices[baseIdx+1].position, 1.0f));
    glm::vec3 v2 = glm::vec3(transform * glm::vec4(vertices[baseIdx+2].position, 1.0f));
    
    // 计算三角形法向量
    glm::vec3 normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
    
    // 计算球心到三角形平面的距离
    float dist = glm::dot(center - v0, normal);
    
    // 如果距离大于半径，无碰撞
    if (std::abs(dist) > radius)
        return false;
    
    // 计算球心在平面上的投影点
    glm::vec3 projection = center - dist * normal;
    
    // 检查投影点是否在三角形内
    bool inside = pointInTriangle(projection, v0, v1, v2);
    
    if (inside) {
        // 投影点在三角形内，检查是否有碰撞
        if (dist < 0) normal = -normal; // 确保法向量指向外部
        
        if (info) {
            info->contactPoint = projection;
            info->normal = normal;
            info->penetrationDepth = radius - std::abs(dist);
            info->triangleIndex = triIndex;
        }
        
        return std::abs(dist) <= radius;
    }
    
    // 投影点不在三角形内，检查与三角形边缘的距离
    float edgeDist = FLT_MAX;
    glm::vec3 closestPoint;
    
    // 检查与每条边的距离
    checkEdgeDistance(projection, v0, v1, edgeDist, closestPoint);
    checkEdgeDistance(projection, v1, v2, edgeDist, closestPoint);
    checkEdgeDistance(projection, v2, v0, edgeDist, closestPoint);
    
    // 如果边缘距离小于半径，有碰撞
    if (edgeDist <= radius) {
        if (info) {
            info->contactPoint = closestPoint;
            info->normal = glm::normalize(center - closestPoint);
            info->penetrationDepth = radius - edgeDist;
            info->triangleIndex = triIndex;
        }
        return true;
    }
    
    return false;
}

bool Model::pointInTriangle(const glm::vec3& p, const glm::vec3& a, 
                         const glm::vec3& b, const glm::vec3& c) {
    // 使用重心坐标判断
    glm::vec3 v0 = b - a;
    glm::vec3 v1 = c - a;
    glm::vec3 v2 = p - a;
    
    float d00 = glm::dot(v0, v0);
    float d01 = glm::dot(v0, v1);
    float d11 = glm::dot(v1, v1);
    float d20 = glm::dot(v2, v0);
    float d21 = glm::dot(v2, v1);
    
    float denom = d00 * d11 - d01 * d01;
    float v = (d11 * d20 - d01 * d21) / denom;
    float w = (d00 * d21 - d01 * d20) / denom;
    float u = 1.0f - v - w;
    
    return v >= 0.0f && w >= 0.0f && u >= 0.0f;
}

void Model::checkEdgeDistance(const glm::vec3& point, const glm::vec3& a, 
                           const glm::vec3& b, float& minDist, glm::vec3& closest) {
    glm::vec3 ab = b - a;
    float t = glm::dot(point - a, ab) / glm::dot(ab, ab);
    t = glm::clamp(t, 0.0f, 1.0f);
    
    glm::vec3 pointOnEdge = a + t * ab;
    float dist = glm::distance(point, pointOnEdge);
    
    if (dist < minDist) {
        minDist = dist;
        closest = pointOnEdge;
    }
}

bool Model::checkSphereCollision(const glm::vec3& center, float radius, 
                             const glm::mat4& modelMatrix, 
                             std::vector<CollisionInfo>& collisions) {
    // 1. 更新AABB
    updateBoundingBox(modelMatrix);
    
    // 2. AABB快速剔除
    if (!boundingBox.intersectSphere(center, radius)) {
        return false;
    }
    
    // 3. 使用BVH进行细粒度碰撞检测
    return checkSphereCollisionBVH(center, radius, modelMatrix, 0, collisions);
}

bool Model::checkSphereCollisionBVH(const glm::vec3& center, float radius,
                                const glm::mat4& modelMatrix, int nodeIdx,
                                std::vector<CollisionInfo>& collisions) {
    const BVHNode& node = bvhNodes[nodeIdx];
    
    // 如果节点的AABB与球体不相交，快速返回
    if (!node.bounds.intersectSphere(center, radius)) {
        return false;
    }
    
    bool hasCollision = false;
    
    if (node.isLeaf()) {
        // 叶子节点：检测每个三角形
        for (int i = 0; i < node.triCount; i++) {
            int triIdx = triangleIndices[node.firstTriIndex + i];
            CollisionInfo info;
            info.collidedObject = this;
            
            if (sphereTriangleCollision(center, radius, triIdx, modelMatrix, &info)) {
                collisions.push_back(info);
                hasCollision = true;
            }
        }
    } else {
        // 内部节点：递归检测子节点
        bool leftCollision = checkSphereCollisionBVH(center, radius, modelMatrix, 
                                                node.leftChild, collisions);
        bool rightCollision = checkSphereCollisionBVH(center, radius, modelMatrix, 
                                                 node.rightChild, collisions);
        
        hasCollision = leftCollision || rightCollision;
    }
    
    return hasCollision;
}
