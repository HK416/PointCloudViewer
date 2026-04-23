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

    // 전송을 요청하고 완료 식별값(Timeline Value)을 반환
    uint64_t requestTransfer(const std::vector<BufferCopyRequest>& requests);
    
    // 특정 전송이 완료되었는지 확인
    bool isFinished(uint64_t value);


private:
    VkDevice m_device;
    VkQueue m_queue;
    VkCommandPool m_commandPool;
    VkSemaphore m_timelineSemaphore;
    uint64_t m_currentValue = 0;

    std::queue<InFlightTransfer> m_inFlightTransfers;
};
