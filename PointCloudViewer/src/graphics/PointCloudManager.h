#pragma once

struct StreamRequest;
struct RenderNode;
class RenderContext;
class TransferManager;
class PointCloudFileManager;

struct alignas(16) PointCloudVertex {
    glm::vec3 position{0.0f};
    uint32_t _padding0{0};
    glm::vec3 color{1.0f};
    uint32_t _padding1{0};
};

struct PointCloudNode {
    uint64_t id{UINT64_MAX};
    uint32_t vertexCount{0};
    size_t slotIndex{0};

    enum class State {
        None,
        LoadingFromDisk,
        PendingUpload,
        UploadingToGPU,
        Ready
    };
    State state = State::None;

    uint64_t transferId = 0;
    std::future<std::vector<PointCloudVertex>> loadFuture;
    std::vector<PointCloudVertex> tempVertices;
};

class FixedSlotAllocator {
public:
    FixedSlotAllocator() = delete;
    FixedSlotAllocator(const FixedSlotAllocator&) = delete;
    FixedSlotAllocator& operator=(const FixedSlotAllocator&) = delete;

    FixedSlotAllocator(size_t maxSlots);

    size_t getAvailableCount() const { return m_freeSlots.size(); }
    std::optional<size_t> allocate();
    void free(size_t slotIndex);

private:
    std::vector<size_t> m_freeSlots;
};

class GlobalPointCloudManager {
public:
    GlobalPointCloudManager() = delete;
    GlobalPointCloudManager(const GlobalPointCloudManager&) = delete;
    GlobalPointCloudManager& operator=(const GlobalPointCloudManager&) = delete;

    GlobalPointCloudManager(
        RenderContext* context,
        TransferManager* transferManager,
        PointCloudFileManager* fileManager,
        size_t maxNodesCapacity
    );
    ~GlobalPointCloudManager();

    void updateStreamingState();

    void requestNodes(const std::vector<StreamRequest>& visibleRequest);

    void bindGlobalBuffer(VkCommandBuffer cmd) const;
    void drawNodes(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout, const std::vector<RenderNode>& nodes) const;

    bool getRenderNode(uint64_t id, RenderNode& outNode) const;
    size_t getCapacity() const { return m_capacity; }

private:
    void evictLRU();

private:
    RenderContext* m_context = nullptr;
    TransferManager* m_transferManager = nullptr;
    PointCloudFileManager* m_fileManager = nullptr;

    size_t m_capacity = 1024;
    VkBuffer m_globalBuffer = VK_NULL_HANDLE;
    VmaAllocation m_globalAllocation = VK_NULL_HANDLE;

    std::unique_ptr<FixedSlotAllocator> m_slotAllocator;

    std::list<uint64_t> m_lruList;
    std::unordered_map<uint64_t, std::list<uint64_t>::iterator> m_lruMap;
    std::unordered_map<uint64_t, PointCloudNode> m_nodeRegistry;

public:
    // OctreeNode::maxCapacity (64 * 64 * 64)와 정확히 일치해야 함
    static const uint32_t maxVerticesPerNode = 262144;
    static const uint32_t vertexStride = 32;
    static const uint32_t bytesPerNode = maxVerticesPerNode * vertexStride;
};
