#include "stdafx.h"
#include "Mesh.h"

PointCloudMesh::PointCloudMesh(
    VkDevice device,
    VmaAllocator allocator,
    VkCommandBuffer commandBuffer,
    const std::vector<PointCloudVertex>& vertices
) {
    if (vertices.empty())
        throw std::runtime_error("The given vertex data is empty!");

    m_allocator = allocator;
    m_vertexCount = vertices.size();

    createBuffer(
        device,
        allocator,
        commandBuffer,
        m_vertexBuffer,
        m_vertexAllocation,
        m_stagingBuffer,
        m_stagingAllocation,
        m_vertexCount * sizeof(PointCloudVertex),
        vertices.data()
    );
}

PointCloudMesh::~PointCloudMesh() {
    if (m_stagingBuffer) {
        vmaDestroyBuffer(m_allocator, m_stagingBuffer, m_stagingAllocation);
    }

    if (m_vertexBuffer) {
        vmaDestroyBuffer(m_allocator, m_vertexBuffer, m_vertexAllocation);
    }
}

void PointCloudMesh::createBuffer(
    VkDevice device,
    VmaAllocator allocator,
    VkCommandBuffer commandBuffer,
    VkBuffer& buffer,
    VmaAllocation& allocation,
    VkBuffer& stagingBuffer,
    VmaAllocation& stagingAllocation,
    size_t bufferSize,
    const void* bufferData
) {
    // 1. Staging Buffer
    {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                          VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo stagingAllocInfo;
        vmaCreateBuffer(
            allocator,
            &bufferInfo,
            &allocInfo,
            &stagingBuffer,
            &stagingAllocation,
            &stagingAllocInfo
        );

        memcpy(stagingAllocInfo.pMappedData, bufferData, bufferSize);
    }

    // 2. GPU Only Buffer
    {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                           VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);
    }

    // 3. Copy Staging to GPU Only
    {
        VkBufferCopy copyRegion = {};
        copyRegion.size = bufferSize;
        vkCmdCopyBuffer(commandBuffer, stagingBuffer, buffer, 1, &copyRegion);
    }
}

void PointCloudMesh::releaseStagingBuffers() {
    if (m_stagingBuffer) {
        vmaDestroyBuffer(m_allocator, m_stagingBuffer, m_stagingAllocation);
        m_stagingBuffer = VK_NULL_HANDLE;
        m_stagingAllocation = VK_NULL_HANDLE;
    }
}

void PointCloudMesh::bind(VkCommandBuffer commandBuffer) const {
    VkDeviceSize offset{0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_vertexBuffer, &offset);
}

void PointCloudMesh::draw(VkCommandBuffer commandBuffer) const {
    vkCmdDraw(commandBuffer, m_vertexCount, 1, 0, 0);
}
