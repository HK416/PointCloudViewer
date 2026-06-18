#include "stdafx.h"
#include "TransferManager.h"
#include "Renderer.h"
#include "PointCloudManager.h"

TransferManager::TransferManager(RenderContext* context, size_t maxStagingSlots) 
    : m_context(context), m_slotSize(PointCloudDataManager::bytesPerNode) {
    { 
        VkCommandPoolCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        createInfo.queueFamilyIndex = m_context->getGraphicsQueueFamilyIndex();
        
        VkResult res = vkCreateCommandPool(m_context->getDevice(), &createInfo, nullptr, &m_commandPool);
        if (res != VK_SUCCESS) {
            spdlog::critical("[TransferManager] Failed to create command pool! (CODE:{:#08x})", (int)res);
            throw std::runtime_error(std::format("Failed to create command pool! (CODE:{:#08x})", (int)res));
        }
    }

    {
        VkSemaphoreTypeCreateInfo typeInfo = {};
        typeInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        typeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        typeInfo.initialValue = 0;

        VkSemaphoreCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        createInfo.pNext = &typeInfo;

        VkResult res = vkCreateSemaphore(m_context->getDevice(), &createInfo, nullptr, &m_timelineSemaphore);
        if (res != VK_SUCCESS) {
            spdlog::critical("[TransferManager] Failed to create timeline semaphore! (CODE:{:#08x})", (int)res);
            throw std::runtime_error(std::format("Failed to create timeline semaphore! (CODE:{:#08x})", (int)res));
        }
    }

    {
        VkBufferCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        createInfo.size = m_slotSize * maxStagingSlots;
        createInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        allocInfo.flags =
            VMA_ALLOCATION_CREATE_MAPPED_BIT |
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        VmaAllocationInfo allocationInfo = {};
        VkResult res = vmaCreateBuffer(
            m_context->getAllocator(),
            &createInfo,
            &allocInfo,
            &m_stagingBuffer,
            &m_stagingAllocation,
            &allocationInfo
        );
        if (res != VK_SUCCESS) {
            spdlog::critical("[TransferManager] Failed to create staging buffer! (CODE:{:#08x})", (int)res);
            throw std::runtime_error(std::format("Failed to create staging buffer! (CODE:{:#08x})", (int)res));
        }
        m_mappedData = allocationInfo.pMappedData;
    }

    m_freeStagingSlots.reserve(maxStagingSlots);
    for (size_t i = maxStagingSlots; i > 0; --i) {
        m_freeStagingSlots.push_back(i - 1);
    }
}

TransferManager::~TransferManager() {
    if (m_context) {
        if (m_stagingBuffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(m_context->getAllocator(), m_stagingBuffer, m_stagingAllocation);
        }

        if (m_timelineSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_context->getDevice(), m_timelineSemaphore, nullptr);
        }

        if (m_commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_context->getDevice(), m_commandPool, nullptr);
        }
    }
}

void TransferManager::garbageCollect() {
    uint64_t completedValue = UINT64_MAX;
    VkResult res = vkGetSemaphoreCounterValue(m_context->getDevice(), m_timelineSemaphore, &completedValue);
    if (res != VK_SUCCESS) {
        spdlog::error("[TransferManager] Failed to get semaphore counter value! (CODE:{:#08x})", (int)res);
        return;
    }

    while (!m_inFlightTransfers.empty()) {
        auto& transfer = m_inFlightTransfers.front();
        if (transfer.timelineValue <= completedValue) {
            vkResetCommandBuffer(transfer.commandBuffer, NULL);
            m_cmdBufferPool.push_back(transfer.commandBuffer);

            for (size_t slot : transfer.usedStagingSlots) {
                m_freeStagingSlots.push_back(slot);
            }

            m_inFlightTransfers.pop();
        } else {
            break;
        }
    }
}

bool TransferManager::queueUpload(VkBuffer dstBuffer, VkDeviceSize dstOffset, const void* data, size_t size) {
    garbageCollect();

    auto slotOpt = allocateStagingSlot();
    if (!slotOpt.has_value()) {
        return false;
    }

    size_t slotIndex = slotOpt.value();
    size_t stagingOffset = slotIndex * m_slotSize;

    memcpy(static_cast<char*>(m_mappedData) + stagingOffset, data, size);

    if (m_currentCmdBuffer == VK_NULL_HANDLE) {
        if (m_cmdBufferPool.empty()) {
            VkCommandBufferAllocateInfo allocInfo = {};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandPool = m_commandPool;
            allocInfo.commandBufferCount = 1;

            VkResult res = vkAllocateCommandBuffers(m_context->getDevice(), &allocInfo, &m_currentCmdBuffer);
            if (res != VK_SUCCESS) {
                spdlog::error("[TransferManager] Failed to allocate command buffer! (CODE:{:#08x})", (int)res);
                return false;
            }
        } else {
            m_currentCmdBuffer = m_cmdBufferPool.back();
            m_cmdBufferPool.pop_back();
        }

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VkResult res = vkBeginCommandBuffer(m_currentCmdBuffer, &beginInfo);
        if (res != VK_SUCCESS) {
            spdlog::error("[TransferManager] Failed to begin command buffer! (CODE:{:#08x})", (int)res);
            return false;
        }
    }

    VkBufferCopy copyRegion = {stagingOffset, dstOffset, size};
    vkCmdCopyBuffer(m_currentCmdBuffer, m_stagingBuffer, dstBuffer, 1, &copyRegion);

    m_currentPendingSlots.push_back(slotIndex);
    return true;
}

uint64_t TransferManager::flush() {
    if (m_currentCmdBuffer == VK_NULL_HANDLE) {
        return m_currentValue;
    }

    VkResult res = vkEndCommandBuffer(m_currentCmdBuffer);
    if (res != VK_SUCCESS) {
        spdlog::error("[TransferManager] Failed to end command buffer! (CODE:{:#08x})", (int)res);
        return m_currentValue;
    }

    m_currentValue += 1;
    uint64_t waitValue = m_currentValue;

    VkTimelineSemaphoreSubmitInfo timelineInfo = {};
    timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    timelineInfo.signalSemaphoreValueCount = 1;
    timelineInfo.pSignalSemaphoreValues = &waitValue;

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = &timelineInfo;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_currentCmdBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &m_timelineSemaphore;

    res = vkQueueSubmit(m_context->getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    if (res != VK_SUCCESS) {
        spdlog::error("[TransferManager] Failed to submit transfer queue! (CODE:{:#08x})", (int)res);
        return m_currentValue;
    }

    m_inFlightTransfers.push({waitValue, m_currentCmdBuffer, std::move(m_currentPendingSlots)});

    m_currentCmdBuffer = VK_NULL_HANDLE;
    m_currentPendingSlots.clear();

    return waitValue;
}

bool TransferManager::isFinished(uint64_t value) {
    garbageCollect();
    uint64_t counter = UINT64_MAX;
    VkResult res = vkGetSemaphoreCounterValue(m_context->getDevice(), m_timelineSemaphore, &counter);
    if (res != VK_SUCCESS) {
        spdlog::error("[TransferManager] isFinished check failed! (CODE:{:#08x})", (int)res);
        return false;
    }

    return counter >= value;
}

std::optional<size_t> TransferManager::allocateStagingSlot() {
    if (m_freeStagingSlots.empty()) {
        return std::nullopt;
    }

    size_t slot = m_freeStagingSlots.back();
    m_freeStagingSlots.pop_back();
    return slot;
}
