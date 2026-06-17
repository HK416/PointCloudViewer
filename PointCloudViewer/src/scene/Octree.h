#pragma once
#include "PointCloudManager.h"
#include "FileManager.h"

struct Frustum;

struct Bound3D {
    glm::vec3 min;
    glm::vec3 max;

    glm::vec3 getCenter() const;
    glm::vec3 getSize() const;
};

struct ChunkRenderInfo {
    uint64_t id;
    ChunkSpan span;
    glm::vec3 center;
};

class OctreeNode {
public:
    OctreeNode() = delete;
    OctreeNode(const OctreeNode&) = delete;
    OctreeNode& operator=(const OctreeNode&) = delete;

    OctreeNode(uint64_t id, Bound3D bound, PointCloudFileManager* fileManager);
    ~OctreeNode() = default;

    void insert(const PointCloudVertex& p);
    void flushRemainingToDisk();
    size_t flushToDiskTemporary();
    ChunkSpan getChunkData() const;

    void getAllBounds(std::vector<Bound3D>& outBounds) const;
    void getVisibleChunks(
        const Frustum& frustum,
        glm::vec3 localCameraPos,
        std::vector<ChunkRenderInfo>& outChunks
    );

private:
    int getOctantIndex(const glm::vec3& position) const;
    void createChild(int index);

    void insertIntoChild(const PointCloudVertex& p);
    void splitAndPushDown();

private:
    PointCloudFileManager* m_fileManager;
    Bound3D m_bound;

    bool m_leafNode = true;
    uint64_t m_id = 0;
    ChunkSpan m_chunkSpan;
    std::vector<ChunkSpan> m_chunkSpans;
    uint32_t m_totalPointCount = 0;
    std::unique_ptr<OctreeNode> m_children[8];
    std::vector<PointCloudVertex> m_points;

    std::vector<uint8_t> m_voxelOccupancy;
    std::vector<PointCloudVertex> m_lodPoints;

    glm::vec3 m_center{0.0f};
    glm::vec3 m_invVoxelSize{0.0f};

public:
    static const int gridResolution = 64;
    static const int maxCapacity = gridResolution * gridResolution * gridResolution;
};

class Octree {
public:
    Octree() = delete;
    Octree(const Octree&) = delete;
    Octree& operator=(const Octree&) = delete;

    Octree(PointCloudFileManager* fileManager, Bound3D bound);
    ~Octree() = default;

public:
    void insert(const PointCloudVertex& p);
    void flushRemainingToDisk();

    std::vector<Bound3D> getAllBounds() const;
    std::vector<ChunkRenderInfo> getVisibleChunks(const Frustum& frustum, const glm::vec3& localCameraPos);

private:
    PointCloudFileManager* m_fileManager;
    std::unique_ptr<OctreeNode> m_root;
    
    size_t m_ramPointCount = 0;
    size_t m_maxRamPoints = 5000000; // 약 160MB 메모리 제한
};

