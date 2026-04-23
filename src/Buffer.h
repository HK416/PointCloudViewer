#pragma once

struct ChunkSpan;
class TransferManager;
class PointCloudFileManager;

struct PointCloudVertex {
    glm::vec3 position;
    glm::vec3 color;
    float intensity;
};

class PointCloudBuffer {
private:
    struct StagingChunk {
        VkBuffer buffer;
        VmaAllocation allocation;
    };

public:
    PointCloudBuffer() = delete;
    PointCloudBuffer(const PointCloudBuffer&) = delete;
    PointCloudBuffer(
        VkDevice device,
        VmaAllocator allocator,
        TransferManager* transferManager,
        const std::vector<PointCloudVertex>& vertices
    );
    ~PointCloudBuffer();

public:
    void updateStatus(TransferManager* transferManager);

    void bind(VkCommandBuffer commandBuffer) const;
    void draw(VkCommandBuffer commandBuffer) const;

    bool isReady() const;

private:
    bool m_ready = false;
    uint32_t m_vertexCount = 0;
    uint64_t m_transferId = 0;
    uint64_t m_offset = 0;

    VkDevice m_device = VK_NULL_HANDLE;
    VmaAllocator m_allocator = VK_NULL_HANDLE;

    std::vector<StagingChunk> m_stagingChunks;

    VkBuffer m_pointCloudBuffer = VK_NULL_HANDLE;
    VmaAllocation m_pointCloudAllocation = VK_NULL_HANDLE;
};

class PointCloudBufferManager {
private:
    struct CacheNode {
        uint64_t id;
        std::unique_ptr<PointCloudBuffer> buffer;
        
        bool isLoading = false;
        std::future<std::vector<PointCloudVertex>> loadFuture;

        bool usedThisFrame = false;
    };

public:
    PointCloudBufferManager() = delete;
    PointCloudBufferManager(const PointCloudBufferManager&) = delete;
    PointCloudBufferManager(
        VkDevice device,
        VmaAllocator allocator,
        TransferManager* transferManager,
        PointCloudFileManager* fileManager,
        size_t capacity = 2048
    );

private:
    void evict();

public:
    void beginFrame();
    void updateBufferState();
    PointCloudBuffer* getOrRequestBuffer(uint64_t id, ChunkSpan span);

private:
    VkDevice m_device;
    VmaAllocator m_allocator;

    size_t m_capacity;
    std::list<CacheNode> m_lruList;
    std::unordered_map<uint64_t, std::list<CacheNode>::iterator> m_cacheMap;
    TransferManager* m_transferManager;
    PointCloudFileManager* m_fileManager;

    std::vector<std::unique_ptr<PointCloudBuffer>> m_garbageList;
};
