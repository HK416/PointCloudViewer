#pragma once
#include "Buffer.h"
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
    OctreeNode(uint64_t id, Bound3D bound, PointCloudFileManager* fileManager);

    void insert(const PointCloudVertex& p);
    void flushRemainingToDisk();
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
    bool m_leafNode = true;
    uint64_t m_id = 0;
    ChunkSpan m_chunkSpan;
    Bound3D m_bound;
    std::unique_ptr<OctreeNode> m_children[8];
    std::vector<PointCloudVertex> m_points;
    PointCloudFileManager* m_fileManager;

    std::vector<bool> m_voxelOccupancy;
    std::vector<PointCloudVertex> m_lodPoints;

public:
    static const int GRID_RES = 64;
    static const int MAX_CAPACITY = GRID_RES * GRID_RES * GRID_RES;
};

class Octree {
public:
    Octree() = delete;
    Octree(const Octree&) = delete;
    Octree(PointCloudFileManager* fileManager, Bound3D bound);

public:
    void insert(const PointCloudVertex& p);
    void flushRemainingToDisk();

    void getAllBounds(std::vector<Bound3D>& outBounds) const;
    void getVisibleChunks(
        const Frustum& frustum,
        glm::vec3 localCameraPos,
        std::vector<ChunkRenderInfo>& outChunks
    );

private:
    PointCloudFileManager* m_fileManager;
    std::unique_ptr<OctreeNode> m_root;
};
