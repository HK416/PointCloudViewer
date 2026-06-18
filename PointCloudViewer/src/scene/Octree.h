#pragma once
#include "PointCloudVertex.h"
#include "FileManager.h"

struct Frustum;

//
// ================ Bound3D ================
//

/// @brief 3차원 공간에서 축 정렬 바운딩 박스(AABB)를 나타내는 구조체입니다.
struct Bound3D {
    glm::vec3 min;
    glm::vec3 max;

    glm::vec3 getCenter() const;
    glm::vec3 getSize() const;
};

//
// ================ ChunkRenderInfo ================
//

/// @brief 렌더링할 청크 데이터의 위치, 스팬(Span), ID 등 정보를 담는 구조체입니다.
struct ChunkRenderInfo {
    uint64_t id;
    ChunkSpan span;
    glm::vec3 center;
};

//
// ================ OctreeNode ================
//

/// @brief 옥트리 구조의 단일 노드를 나타내며, 포인트를 저장하고 자식 노드로 분할하는 역할을 하는 클래스입니다.
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
    static constexpr int gridResolution = 64;
    static constexpr int maxCapacity = gridResolution * gridResolution * gridResolution;
};

//
// ================ Octree ================
//

/// @brief 포인트 데이터를 공간적으로 분할 및 관리하기 위한 옥트리 자료구조를 나타내는 클래스입니다.
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

