#pragma once

class Mesh {
public:
    virtual ~Mesh() {}

    virtual void releaseStagingBuffers() = 0;

    virtual void bind(VkCommandBuffer) const = 0;
    virtual void draw(VkCommandBuffer) const = 0;
};

struct PointCloudVertex {
    glm::vec3 position;
    glm::vec3 color;
    float intensity;
};

class PointCloudMesh : public Mesh {
public:
    PointCloudMesh() = delete;
    PointCloudMesh(const PointCloudMesh&) = delete;
    PointCloudMesh(
        VkDevice device,
        VmaAllocator allocator,
        VkCommandBuffer commandBuffer,
        const std::vector<PointCloudVertex>& vertices
    );
    virtual ~PointCloudMesh();

private:
    void createBuffer(
        VkDevice device,
        VmaAllocator allocator,
        VkCommandBuffer commandBuffer,
        VkBuffer& buffer,
        VmaAllocation& allocation,
        VkBuffer& stagingBuffer,
        VmaAllocation& stagingAllocation,
        size_t bufferSize,
        const void* bufferData
    );

public:
    virtual void releaseStagingBuffers() override;

    virtual void bind(VkCommandBuffer commandBuffer) const override;
    virtual void draw(VkCommandBuffer commandBuffer) const override;

protected:
    uint32_t m_vertexCount = 0;
    VmaAllocator m_allocator = VK_NULL_HANDLE;

    VkBuffer m_stagingBuffer = VK_NULL_HANDLE;
    VmaAllocation m_stagingAllocation = VK_NULL_HANDLE;

    VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
    VmaAllocation m_vertexAllocation = VK_NULL_HANDLE;
};
