#pragma once

#include "TransferManager.h"

class Mesh {
public:
    virtual ~Mesh() {}

    virtual void updateStatus(TransferManager* transferMgr) = 0;

    virtual void bind(VkCommandBuffer) const = 0;
    virtual void draw(VkCommandBuffer) const = 0;

    virtual bool isReady() const = 0;
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
        TransferManager* transferMgr,
        const std::vector<PointCloudVertex>& vertices
    );
    virtual ~PointCloudMesh();

private:
    void createBuffer(
        VmaAllocator allocator,
        TransferManager* transferMgr,
        VkBuffer& buffer,
        VmaAllocation& allocation,
        VkBuffer& stagingBuffer,
        VmaAllocation& stagingAllocation,
        size_t bufferSize,
        const void* bufferData
    );

public:
    virtual void updateStatus(TransferManager* transferMgr) override;

    virtual void bind(VkCommandBuffer commandBuffer) const override;
    virtual void draw(VkCommandBuffer commandBuffer) const override;

    virtual bool isReady() const override { return m_ready; }

protected:
    bool m_ready = false;
    uint32_t m_vertexCount = 0;
    uint64_t m_transferId = 0;

    VkDevice m_device = VK_NULL_HANDLE;
    VmaAllocator m_allocator = VK_NULL_HANDLE;

    VkBuffer m_stagingBuffer = VK_NULL_HANDLE;
    VmaAllocation m_stagingAllocation = VK_NULL_HANDLE;

    VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
    VmaAllocation m_vertexAllocation = VK_NULL_HANDLE;
};
