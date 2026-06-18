#pragma once
#include "Octree.h"
#include "PointCloudVertex.h"

struct StreamRequest;
struct RenderNode;
class RenderContext;
class TransferManager;
class PointCloudFileManager;


//
// ================ PointCloudNode ================
//

/// @brief 옥트리의 각 노드에 대응하며 렌더링에 필요한 포인트 데이터를 관리하는 구조체입니다.
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

//
// ================ FixedSlotAllocator ================
//

/// @brief 고정된 개수의 슬롯을 관리하고 할당/해제하는 할당기 클래스입니다.
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

//
// ================ PointCloudDataManager ================
//

/// @brief 스트리밍되는 포인트 클라우드 노드 데이터를 관리하고 GPU 메모리 풀(버퍼)을 제어하는 클래스입니다.
class PointCloudDataManager {
public:
    PointCloudDataManager() = delete;
    PointCloudDataManager(const PointCloudDataManager&) = delete;
    PointCloudDataManager& operator=(const PointCloudDataManager&) = delete;

    PointCloudDataManager(
        RenderContext* context,
        TransferManager* transferManager,
        PointCloudFileManager* fileManager,
        size_t maxNodesCapacity
    );
    ~PointCloudDataManager();

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
    static constexpr uint32_t maxVerticesPerNode = OctreeNode::maxCapacity;
    static constexpr uint32_t vertexStride = sizeof(PointCloudVertex);
    static constexpr uint32_t bytesPerNode = maxVerticesPerNode * vertexStride;
};
