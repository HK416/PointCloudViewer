#pragma once

class RenderContext;

class TransferManager {
    struct InFlightTransfer {
        uint64_t timelineValue;
        VkCommandBuffer commandBuffer;
        std::vector<size_t> usedStagingSlots;
    };

public:
    TransferManager() = delete;
    TransferManager(const TransferManager&) = delete;
    TransferManager& operator=(const TransferManager&) = delete;

    TransferManager(RenderContext* context, size_t maxStagingSlots = 128);
    ~TransferManager();

    void garbageCollect();

    bool queueUpload(
        VkBuffer dstBuffer,
        VkDeviceSize dstOffset,
        const void* data,
        size_t size
    );

    uint64_t flush();

    bool isFinished(uint64_t value);

private:
    std::optional<size_t> allocateStagingSlot();

private:
    RenderContext* m_context = nullptr;

    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    VkSemaphore m_timelineSemaphore = VK_NULL_HANDLE;
    uint64_t m_currentValue = 0;

    size_t m_slotSize;
    VkBuffer m_stagingBuffer = VK_NULL_HANDLE;
    VmaAllocation m_stagingAllocation = VK_NULL_HANDLE;
    void* m_mappedData = nullptr;

    std::vector<size_t> m_freeStagingSlots;

    VkCommandBuffer m_currentCmdBuffer = VK_NULL_HANDLE;
    std::vector<size_t> m_currentPendingSlots;

    std::queue<InFlightTransfer> m_inFlightTransfers;
    std::vector<VkCommandBuffer> m_cmdBufferPool;
};
