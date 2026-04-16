#include "stdafx.h"
#include "Buffer.h"
#include "Octree.h"
#include "TransferManager.h"

PointCloudBuffer::PointCloudBuffer(
    VkDevice device,
    VmaAllocator allocator,
    TransferManager* transferManager,
    const std::vector<PointCloudVertex>& vertices
) {
    if (vertices.empty())
        throw std::runtime_error("The given vertex data is empty!");

    m_device = device;
    m_allocator = allocator;
    m_vertexCount = static_cast<uint32_t>(vertices.size());
    uint64_t bufferSize = m_vertexCount * sizeof(PointCloudVertex);

    // 1. Create Staging Buffer 
    { 
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags =
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo stagingAllocInfo;
        vmaCreateBuffer(
            m_allocator,
            &bufferInfo,
            &allocInfo,
            &m_stagingBuffer,
            &m_stagingAllocation,
            &stagingAllocInfo
        );

        memcpy(
            stagingAllocInfo.pMappedData,
            reinterpret_cast<const void*>(vertices.data()),
            bufferSize
        );
    }

    // 2. Create GPU Only Buffer
    {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                           VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        vmaCreateBuffer(
            allocator,
            &bufferInfo,
            &allocInfo,
            &m_pointCloudBuffer,
            &m_pointCloudAllocation,
            nullptr
        );
    }

    // 3. Delegate Copy to TransferManager
    m_transferId = transferManager->requestTransfer(m_stagingBuffer, m_pointCloudBuffer, bufferSize);
}

PointCloudBuffer::~PointCloudBuffer() {
    if (m_stagingBuffer) {
        vmaDestroyBuffer(m_allocator, m_stagingBuffer, m_stagingAllocation);
    }

    if (m_pointCloudBuffer) {
        vmaDestroyBuffer(m_allocator, m_pointCloudBuffer, m_pointCloudAllocation);
    }
}

void PointCloudBuffer::updateStatus(TransferManager* transferManager) {
    if (!m_ready && m_transferId > 0) {
        if (transferManager->isFinished(m_transferId)) {
            if (m_stagingBuffer) {
                vmaDestroyBuffer(m_allocator, m_stagingBuffer, m_stagingAllocation);
                m_stagingBuffer = VK_NULL_HANDLE;
                m_stagingAllocation = VK_NULL_HANDLE;
            }
            m_ready = true;
        }
    }
}

void PointCloudBuffer::bind(VkCommandBuffer commandBuffer) const {
    if (m_ready) {
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_pointCloudBuffer, &m_offset);
    }
}

void PointCloudBuffer::draw(VkCommandBuffer commandBuffer) const {
    if (m_ready) {
        vkCmdDraw(commandBuffer, m_vertexCount, 1, 0, 0);
    }
}

bool PointCloudBuffer::isReady() const {
    return m_ready;
}

PointCloudBufferManager::PointCloudBufferManager(
    VkDevice device,
    VmaAllocator allocator,
    TransferManager* transferManager,
    size_t capacity
) {
    m_device = device;
    m_allocator = allocator;
    m_transferManager = transferManager;
    m_capacity = capacity;
}

void PointCloudBufferManager::evict() {
    if (m_lruList.empty())
        return;

    const auto& last = m_lruList.back();
    m_cacheMap.erase(last.id);
    m_lruList.pop_back();
}

PointCloudBuffer* PointCloudBufferManager::getOrRequestBuffer(uint64_t id, ChunkSpan span) {
    auto it = m_cacheMap.find(id);

    if (it != m_cacheMap.end()) {
        m_lruList.splice(m_lruList.begin(), m_lruList, it->second);
        return &(it->second->buffer);
    }

    if (m_cacheMap.size() >= m_capacity) {
        evict();
    }



    return nullptr;
}
