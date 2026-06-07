#pragma once

struct BufferCopyRequest {
    VkBuffer src;
    VkDeviceSize srcOffset;
    VkBuffer dst;
    VkDeviceSize dstOffset;
    VkDeviceSize size;
};

class TransferManager {
private:
    struct InFlightTransfer {
        uint64_t timelineValue;
        VkCommandBuffer commandBuffer;
    };

public:
    TransferManager(VkDevice device, uint32_t queueFamilyIndex, VkQueue queue);
    ~TransferManager();

    void garbageCollect();

    // ?„ì†¡???”ì²­?˜ê³  ?„ë£Œ ?ë³„ê°?Timeline Value)??ë°˜í™˜
    uint64_t requestTransfer(const std::vector<BufferCopyRequest>& requests);
    
    // ?¹ì • ?„ì†¡???„ë£Œ?˜ì—ˆ?”ì? ?•ì¸
    bool isFinished(uint64_t value);


private:
    VkDevice m_device;
    VkQueue m_queue;
    VkCommandPool m_commandPool;
    VkSemaphore m_timelineSemaphore;
    uint64_t m_currentValue = 0;

    std::queue<InFlightTransfer> m_inFlightTransfers;
};
