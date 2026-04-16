#pragma once
#include "Buffer.h"
#include "FileManager.h"

struct Bound3D {
    glm::vec3 min;
    glm::vec3 max;

    glm::vec3 getCenter() const;
    glm::vec3 getSize() const;
};

class OctreeNode {
public:
    OctreeNode() = delete;
    OctreeNode(const OctreeNode&) = delete;
    OctreeNode(Bound3D bound, PointCloudFileManager* fileManager);

    void insert(const PointCloudVertex& p);
    void flushRemainingToDisk();
    ChunkSpan getChunkData() const;

private:
    int getOctantIndex(const glm::vec3& position) const;
    void createChild(int index);
    void flushLODToDisk(const std::vector<PointCloudVertex>& lodPoints);
    void insertIntoChild(const PointCloudVertex& p);
    void splitAndPushDown();

private:
    bool m_leafNode = true;
    ChunkSpan m_chunkSpan;
    Bound3D m_bound;
    std::unique_ptr<OctreeNode> m_children[8];
    std::vector<PointCloudVertex> m_points;
    PointCloudFileManager* m_fileManager;

public:
    static const int MAX_CAPACITY = 65536;
    static const int GRID_RES = 32;
};

class Octree {
public:
    Octree() = delete;
    Octree(const Octree&) = delete;
    Octree(PointCloudFileManager* fileManager, Bound3D bound);

public:
    void insert(const PointCloudVertex& p);
    void flushRemainingToDisk();

private:
    PointCloudFileManager* m_fileManager;
    std::unique_ptr<OctreeNode> m_root;
};
