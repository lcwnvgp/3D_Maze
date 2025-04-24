#pragma once

#include "Types.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <float.h>  // 添加这个头文件，用于FLT_MAX

// 碰撞检测输出结构
struct CollisionInfo {
    glm::vec3 contactPoint;   // 碰撞接触点
    glm::vec3 normal;         // 碰撞法向量（指向物体外部）
    float penetrationDepth;   // 穿透深度
    class Model* collidedObject;  // 碰撞的对象指针
    int triangleIndex;        // 碰撞的三角形索引
};

// BoundingBox结构
struct BoundingBox {
    glm::vec3 min;
    glm::vec3 max;
    
    // 检查与球体的碰撞
    bool intersectSphere(const glm::vec3& center, float radius) const;
    
    // 从模型数据计算AABB
    static BoundingBox fromModel(const std::vector<Vertex>& vertices, const glm::mat4& transform);
};

class Model {
public:
    Model(VkDevice device, VkPhysicalDevice physicalDevice, 
          VkCommandPool commandPool, VkQueue graphicsQueue,
          const std::string& modelPath);
    ~Model();

    void draw(VkCommandBuffer commandBuffer);
    void updateUniformBuffer(const glm::mat4& view, const glm::mat4& projection, const glm::mat4& model);

    const std::vector<Vertex>& GetVertices() const { return vertices; }
    const std::vector<uint32_t>& GetIndices() const { return indices; }

    // 碰撞检测相关方法
    void initCollisionData();
    void updateBoundingBox(const glm::mat4& transform);
    bool checkSphereCollision(const glm::vec3& center, float radius, 
                           const glm::mat4& modelMatrix, 
                           std::vector<CollisionInfo>& collisions);

private:
    void loadModel(const std::string& modelPath);
    void createVertexBuffer(VkPhysicalDevice physicalDevice);
    void createBuffer(VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                     VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    // 碰撞检测辅助方法
    void buildBVH();
    bool sphereTriangleCollision(const glm::vec3& center, float radius,
                              int triIndex, const glm::mat4& transform,
                              CollisionInfo* info = nullptr) const;
    bool checkSphereCollisionBVH(const glm::vec3& center, float radius,
                             const glm::mat4& modelMatrix, int nodeIdx,
                             std::vector<CollisionInfo>& collisions);
    static bool pointInTriangle(const glm::vec3& p, const glm::vec3& a, 
                             const glm::vec3& b, const glm::vec3& c);
    static void checkEdgeDistance(const glm::vec3& point, const glm::vec3& a, 
                               const glm::vec3& b, float& minDist, glm::vec3& closest);

    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkCommandPool commandPool;
    VkQueue graphicsQueue;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    VkBuffer uniformBuffer;
    VkDeviceMemory uniformBufferMemory;
    void* uniformBufferMapped;

    // 碰撞检测相关成员
    BoundingBox boundingBox;
    
    // BVH结构
    struct BVHNode {
        BoundingBox bounds;
        int firstTriIndex;
        int triCount;
        int leftChild;
        int rightChild;
        
        bool isLeaf() const { return leftChild == -1; }
    };
    
    std::vector<BVHNode> bvhNodes;
    std::vector<int> triangleIndices; // 三角形索引列表，用于BVH
};
