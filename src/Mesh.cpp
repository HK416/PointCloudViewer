#include "stdafx.h"
#include "Mesh.h"

PointCloudMesh::PointCloudMesh(
    VkDevice device,
    VmaAllocator allocator,
    TransferManager* transferMgr,
    const std::vector<PointCloudVertex>& vertices
) {
    if (vertices.empty())
        throw std::runtime_error("The given vertex data is empty!");

    m_device = device;
    m_allocator = allocator;
    m_vertexCount = static_cast<uint32_t>(vertices.size());

    createBuffer(
        m_allocator,
        transferMgr,
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
    VmaAllocator allocator,
    TransferManager* transferMgr,
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

    // 3. Delegate Copy to TransferManager
    m_transferId = transferMgr->requestTransfer(stagingBuffer, buffer, bufferSize);
}

void PointCloudMesh::updateStatus(TransferManager* transferMgr) {
    if (!m_ready && m_transferId > 0) {
        if (transferMgr->isFinished(m_transferId)) {
            if (m_stagingBuffer) {
                vmaDestroyBuffer(m_allocator, m_stagingBuffer, m_stagingAllocation);
                m_stagingBuffer = VK_NULL_HANDLE;
                m_stagingAllocation = VK_NULL_HANDLE;
            }
            m_ready = true;
        }
    }
}

void PointCloudMesh::bind(VkCommandBuffer commandBuffer) const {
    if (m_ready) {
        VkDeviceSize offset{0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_vertexBuffer, &offset);
    }
}

void PointCloudMesh::draw(VkCommandBuffer commandBuffer) const {
    if (m_ready) {
        vkCmdDraw(commandBuffer, m_vertexCount, 1, 0, 0);
    }
}
