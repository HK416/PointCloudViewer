#include "stdafx.h"
#include "PointCloudManager.h"
#include "TransferManager.h"
#include "FileManager.h"
#include "Renderer.h"

FixedSlotAllocator::FixedSlotAllocator(size_t maxSlots) {
    m_freeSlots.reserve(maxSlots);
    for (size_t i = maxSlots; i > 0; --i) {
        m_freeSlots.push_back(i - 1);
    }
}

std::optional<size_t> FixedSlotAllocator::allocate() {
    if (m_freeSlots.empty()) {
        return std::nullopt;
    }

    size_t slot = m_freeSlots.back();
    m_freeSlots.pop_back();
    return slot;
}

void FixedSlotAllocator::free(size_t slotIndex) {
    m_freeSlots.push_back(slotIndex);
}

GlobalPointCloudManager::GlobalPointCloudManager(
    RenderContext* context,
    TransferManager* transferManager,
    PointCloudFileManager* fileManager,
    size_t maxNodesCapacity
)
    : m_context(context), m_transferManager(transferManager),
      m_fileManager(fileManager), m_capacity(maxNodesCapacity) {
    m_slotAllocator = std::make_unique<FixedSlotAllocator>(maxNodesCapacity);

    VkBufferCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.size = maxNodesCapacity * bytesPerNode;
    createInfo.usage =
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    VkResult res = vmaCreateBuffer(
        context->getAllocator(),
        &createInfo,
        &allocInfo,
        &m_globalBuffer,
        &m_globalAllocation,
        nullptr
    );
    if (res != VK_SUCCESS) {
        spdlog::critical("[GlobalPointCloudManager] Failed to create Global Point Cloud Buffer! (CODE:{:#08x})", (int)res);
        throw std::runtime_error(std::format("Failed to create Global Point Cloud Buffer! (CODE:{:#08x})", (int)res));
    }
    spdlog::info("[GlobalPointCloudManager] Initialized with capacity: {} nodes ({} MB VRAM)", maxNodesCapacity, (maxNodesCapacity * bytesPerNode) / (1024 * 1024));
}

GlobalPointCloudManager::~GlobalPointCloudManager() {
    if (m_context && m_globalBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(m_context->getAllocator(), m_globalBuffer, m_globalAllocation);
    }
}

void GlobalPointCloudManager::updateStreamingState() {
    bool uploadQueuedThisFrame = false;
    std::vector<uint64_t> queuedIds;

    for (auto& [id, node] : m_nodeRegistry) {
        if (node.state == PointCloudNode::State::UploadingToGPU) {
            if (m_transferManager->isFinished(node.transferId)) {
                node.state = PointCloudNode::State::Ready;
            }
        }

        if (node.state == PointCloudNode::State::LoadingFromDisk) {
            if (node.loadFuture.valid() && node.loadFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                node.tempVertices = node.loadFuture.get();
                node.vertexCount = static_cast<uint32_t>(node.tempVertices.size());
                node.state = PointCloudNode::State::PendingUpload;
            }
        }

        if (node.state == PointCloudNode::State::PendingUpload) {
            if (node.tempVertices.empty()) {
                spdlog::warn("[GlobalPointCloudManager] Node {} has empty vertices, marking as Ready.", id);
                node.state = PointCloudNode::State::Ready;
                continue;
            }

            VkDeviceSize dstOffset = node.slotIndex * bytesPerNode;
            VkDeviceSize dataSize = node.tempVertices.size() * sizeof(PointCloudVertex);

            bool success = m_transferManager->queueUpload(
                m_globalBuffer, dstOffset, node.tempVertices.data(), dataSize
            );

            if (success) {
                node.tempVertices.clear();
                node.tempVertices.shrink_to_fit();
                node.state = PointCloudNode::State::UploadingToGPU;
                queuedIds.push_back(id);
                uploadQueuedThisFrame = true;
            }
        }
    }

    if (uploadQueuedThisFrame) {
        uint64_t waitValue = m_transferManager->flush();
        for (uint64_t id : queuedIds) {
            m_nodeRegistry[id].transferId = waitValue;
        }
    }
}

void GlobalPointCloudManager::requestNodes(const std::vector<StreamRequest>& visibleRequest) {
    int activeLoads = 0;
    for (const auto& [id, node] : m_nodeRegistry) {
        if (node.state == PointCloudNode::State::LoadingFromDisk) {
            activeLoads++;
        }
    }

    for (const auto& req : visibleRequest) {
        uint64_t id = req.id;

        // 캐시 적중
        if (m_nodeRegistry.find(id) != m_nodeRegistry.end()) {
            // LRU 캐시 갱신
            m_lruList.erase(m_lruMap[id]);
            m_lruList.push_front(id);
            m_lruMap[id] = m_lruList.begin();
            continue;
        }

        // 스레드 폭주 및 락(Lock) 경합 방지: 동시 읽기 작업 스로틀링
        if (activeLoads >= 16) {
            continue;
        }

        // 캐시 미스
        if (m_nodeRegistry.size() >= m_capacity) {
            evictLRU();
        }

        auto slotOpt = m_slotAllocator->allocate();
        if (!slotOpt.has_value()) {
            spdlog::warn("[GlobalPointCloudManager] VRAM Slots full! Cannot load node {}", id);
            continue;
        }

        PointCloudNode newNode = {};
        newNode.id = id;
        newNode.slotIndex = slotOpt.value();
        newNode.state = PointCloudNode::State::LoadingFromDisk;
        newNode.loadFuture = m_fileManager->readDataAsync(req.span);

        m_nodeRegistry[id] = std::move(newNode);

        m_lruList.push_front(id);
        m_lruMap[id] = m_lruList.begin();

        activeLoads++;
    }
}

void GlobalPointCloudManager::bindGlobalBuffer(VkCommandBuffer cmd) const {
    VkDeviceSize offsets[]{0};
    vkCmdBindVertexBuffers(cmd, 0, 1, &m_globalBuffer, offsets);
}

bool GlobalPointCloudManager::getRenderNode(uint64_t id, RenderNode& outNode) const {
    auto it = m_nodeRegistry.find(id);
    if (it != m_nodeRegistry.end() && it->second.state == PointCloudNode::State::Ready) {
        outNode.id = id;
        outNode.vertexCount = it->second.vertexCount;
        outNode.slotIndex = it->second.slotIndex;
        return true;
    }
    return false;
}

void GlobalPointCloudManager::drawNodes(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout, const std::vector<RenderNode>& nodes) const {
    for (const auto& renderNode : nodes) {
        auto it = m_nodeRegistry.find(renderNode.id);
        if (it != m_nodeRegistry.end() && it->second.state == PointCloudNode::State::Ready) {
            vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &renderNode.transform);
            uint32_t firstVertex = static_cast<uint32_t>(it->second.slotIndex * maxVerticesPerNode);
            vkCmdDraw(cmd, it->second.vertexCount, 1, firstVertex, 0);
        }
    }
}

void GlobalPointCloudManager::evictLRU() {
    uint64_t targetId = m_lruList.back();
    auto& node = m_nodeRegistry[targetId];

    if (node.state == PointCloudNode::State::LoadingFromDisk && node.loadFuture.valid()) {
        node.loadFuture.wait();
    }

    m_slotAllocator->free(node.slotIndex);

    m_nodeRegistry.erase(targetId);
    m_lruMap.erase(targetId);
    m_lruList.pop_back();
}

