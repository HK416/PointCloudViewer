#include "Core/stdafx.h"
#include "Render/Buffer.h"
#include "Scene/Octree.h"
#include "Utils/FileManager.h"
#include "Render/TransferManager.h"

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
    uint64_t totalBufferSize = m_vertexCount * sizeof(PointCloudVertex);

    // 1. Create GPU Only Buffer
    {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = totalBufferSize;
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

    const uint64_t CHUNK_SIZE = 256 * 1024 * 1024;
    const uint8_t* rawData = reinterpret_cast<const uint8_t*>(vertices.data());
    uint64_t currentOffset = 0;

    // 2. Create Staging Buffers
    std::vector<BufferCopyRequest> copyRequests;
    while (currentOffset < totalBufferSize) {
        uint64_t copySize = std::min(CHUNK_SIZE, totalBufferSize - currentOffset);

        VkBuffer stagingBuffer = VK_NULL_HANDLE;
        VmaAllocation stagingAllocation = VK_NULL_HANDLE;

        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = copySize;
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
            &stagingBuffer,
            &stagingAllocation,
            &stagingAllocInfo
        );

        memcpy(stagingAllocInfo.pMappedData, rawData + currentOffset, copySize);

        m_stagingChunks.push_back({stagingBuffer, stagingAllocation});

        copyRequests.push_back({stagingBuffer, 0, m_pointCloudBuffer, currentOffset, copySize});

        currentOffset += copySize;
    }

    // 3. Delegate Copy to TransferManager
    m_transferId = transferManager->requestTransfer(copyRequests);
}

PointCloudBuffer::~PointCloudBuffer() {
    for (auto&& blob : m_stagingChunks) {
        vmaDestroyBuffer(m_allocator, blob.buffer, blob.allocation);
    }

    if (m_pointCloudBuffer) {
        vmaDestroyBuffer(m_allocator, m_pointCloudBuffer, m_pointCloudAllocation);
    }
}

void PointCloudBuffer::updateStatus(TransferManager* transferManager) {
    if (!m_ready && m_transferId > 0) {
        if (transferManager->isFinished(m_transferId)) {
            for (auto&& blob : m_stagingChunks) {
                vmaDestroyBuffer(m_allocator, blob.buffer, blob.allocation);
            }

            m_stagingChunks.clear();
            m_stagingChunks.shrink_to_fit();
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
    PointCloudFileManager* fileManager,
    size_t capacity
) {
    m_device = device;
    m_allocator = allocator;
    m_transferManager = transferManager;
    m_fileManager = fileManager;
    m_capacity = capacity;
}

void PointCloudBufferManager::evict() {
    if (m_lruList.empty())
        return;

    if (m_lruList.back().buffer) {
        m_garbageList.push_back(std::move(m_lruList.back().buffer));
    }

    m_cacheMap.erase(m_lruList.back().id);
    m_lruList.pop_back();
}

void PointCloudBufferManager::beginFrame() {
    for (auto& node : m_lruList) {
        node.usedThisFrame = false;
    }
    m_garbageList.clear();
}

void PointCloudBufferManager::updateBufferState() {
    for (auto& node : m_lruList) {
        if (node.isLoading) {
            if (node.loadFuture.valid() &&
                node.loadFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                std::vector<PointCloudVertex> vertices = node.loadFuture.get();

                if (!vertices.empty()) {
                    node.buffer = std::make_unique<PointCloudBuffer>(
                        m_device, m_allocator, m_transferManager, vertices
                    );
                }
                node.isLoading = false;
            }
        } else if (node.buffer) {
            node.buffer->updateStatus(m_transferManager);
        }
    }
}

PointCloudBuffer* PointCloudBufferManager::getOrRequestBuffer(uint64_t id, ChunkSpan span) {
    auto it = m_cacheMap.find(id);

    // Cache Hit
    if (it != m_cacheMap.end()) {
        m_lruList.splice(m_lruList.begin(), m_lruList, it->second);
        it->second->usedThisFrame = true;
        return it->second->buffer.get();
    }

    // Cache Miss
    if (m_cacheMap.size() >= m_capacity) {
        if (m_lruList.back().isLoading || 
            !m_lruList.back().buffer->isReady() ||
            m_lruList.back().usedThisFrame) {
            return nullptr;
        } else {
            evict();
        }
    }

    CacheNode newNode;
    newNode.id = id;
    newNode.isLoading = true;
    newNode.usedThisFrame = true;

    newNode.loadFuture = std::async(std::launch::async, [this, span]() {
        return m_fileManager->readData(span);
    });

    m_lruList.push_front(std::move(newNode));
    m_cacheMap[id] = m_lruList.begin();

    return nullptr;
}
