#include "stdafx.h"
#include "Octree.h"
#include "Frustum.h"

glm::vec3 Bound3D::getCenter() const {
    return 0.5f * (min + max);
}

glm::vec3 Bound3D::getSize() const {
    return max - min;
}

OctreeNode::OctreeNode(uint64_t id, Bound3D bound, PointCloudFileManager* fileManager) : m_fileManager(fileManager) {
    m_id = id;
    m_bound = bound;
    m_points.reserve(MAX_CAPACITY);
}

void OctreeNode::insert(const PointCloudVertex& p) {
    if (!m_leafNode) {
        glm::vec3 voxelSize = m_bound.getSize() / glm::vec3(GRID_RES);
        int vx = std::clamp(static_cast<int>((p.position.x - m_bound.min.x) / voxelSize.x), 0, GRID_RES - 1);
        int vy = std::clamp(static_cast<int>((p.position.y - m_bound.min.y) / voxelSize.y), 0, GRID_RES - 1);
        int vz = std::clamp(static_cast<int>((p.position.z - m_bound.min.z) / voxelSize.z), 0, GRID_RES - 1);

        int index = vz * GRID_RES * GRID_RES + vy * GRID_RES + vx;

        if (!m_voxelOccupancy[index] && m_lodPoints.size() < MAX_CAPACITY) {
            m_voxelOccupancy[index] = true;
            m_lodPoints.emplace_back(p);
        }

        insertIntoChild(p);
        return;
    }

    m_points.emplace_back(p);

    if (m_points.size() >= MAX_CAPACITY) {
        splitAndPushDown();
    }
}

void OctreeNode::insertIntoChild(const PointCloudVertex& p) {
    int octantIndex = getOctantIndex(p.position);
    if (!m_children[octantIndex]) {
        createChild(octantIndex);
    }
    m_children[octantIndex]->insert(p);
}

void OctreeNode::flushRemainingToDisk() {
    if (m_leafNode) {
        if (!m_points.empty()) {
            m_chunkSpan = m_fileManager->writeData(m_points);
            m_points.clear();
            m_points.shrink_to_fit();
        }
    } else {
        if (!m_lodPoints.empty()) {
            m_chunkSpan = m_fileManager->writeData(m_lodPoints);
            m_lodPoints.clear();
            m_lodPoints.shrink_to_fit();
        }
        m_voxelOccupancy.clear();
        m_voxelOccupancy.shrink_to_fit();
    }

    for (auto& child : m_children) {
        if (child)
            child->flushRemainingToDisk();
    }
}

ChunkSpan OctreeNode::getChunkData() const {
    return m_chunkSpan;
}

void OctreeNode::getVisibleChunks(
    const Frustum& frustum,
    glm::vec3 localCameraPos,
    std::vector<ChunkRenderInfo>& outChunks
) {
    if (!frustum.intersects(m_bound))
        return;

    if (m_chunkSpan.pointCount > 0)
        outChunks.push_back({m_id, m_chunkSpan, m_bound.getCenter()});

    float dist = glm::distance(localCameraPos, m_bound.getCenter());
    float nodeDiagonal = glm::length(m_bound.getSize());
    if (!m_leafNode && dist > nodeDiagonal * 3.0f) {
        return;
    }

    for (const auto& child : m_children)
        if (child)
            child->getVisibleChunks(frustum, localCameraPos, outChunks);
}

Bound3D OctreeNode::getBounds() const {
    return m_bound;
}

int OctreeNode::getOctantIndex(const glm::vec3& position) const {
    glm::vec3 center = m_bound.getCenter();

    int index = 0x00;
    if (position.x >= center.x)
        index |= 0x01;
    if (position.y >= center.y)
        index |= 0x02;
    if (position.z >= center.z)
        index |= 0x04;

    return index;
}

void OctreeNode::createChild(int index) {
    glm::vec3 center = m_bound.getCenter();
    glm::vec3 min = m_bound.min;
    glm::vec3 max = m_bound.max;

    if (index & 0x01) {
        min.x = center.x;
    } else {
        max.x = center.x;
    }

    if (index & 0x02) {
        min.y = center.y;
    } else {
        max.y = center.y;
    }

    if (index & 0x04) {
        min.z = center.z;
    } else {
        max.z = center.z;
    }

    Bound3D bound{min, max};
    uint64_t newId = m_id * 8 + (index + 1);
    m_children[index] = std::make_unique<OctreeNode>(newId, bound, m_fileManager);
}

void OctreeNode::splitAndPushDown() {
    m_leafNode = false;

    m_voxelOccupancy.assign(MAX_CAPACITY, false);
    m_lodPoints.reserve(MAX_CAPACITY);

    glm::vec3 voxelSize = m_bound.getSize() / glm::vec3(GRID_RES);
    std::vector<PointCloudVertex> pushDownPoints;
    pushDownPoints.reserve(MAX_CAPACITY);

    for (const auto& p : m_points) {
        int vx = std::clamp(static_cast<int>((p.position.x - m_bound.min.x) / voxelSize.x), 0, GRID_RES - 1);
        int vy = std::clamp(static_cast<int>((p.position.y - m_bound.min.y) / voxelSize.y), 0, GRID_RES - 1);
        int vz = std::clamp(static_cast<int>((p.position.z - m_bound.min.z) / voxelSize.z), 0, GRID_RES - 1);

        int index = vz * GRID_RES * GRID_RES + vy * GRID_RES + vx;
        
        if (!m_voxelOccupancy[index]) {
            m_voxelOccupancy[index] = true;
            m_lodPoints.emplace_back(p);
        } else {
            pushDownPoints.emplace_back(p);
        }
    }

    m_points.clear();
    m_points.shrink_to_fit();

    for (const auto& p : pushDownPoints) {
        insertIntoChild(p);
    }
}

Octree::Octree(PointCloudFileManager* fileManager, Bound3D bound)
    : m_fileManager(fileManager) {
    m_root = std::make_unique<OctreeNode>(0, bound, m_fileManager);
}

void Octree::insert(const PointCloudVertex& p) {
    if (m_root)
        m_root->insert(p);
}

void Octree::flushRemainingToDisk() {
    if (m_root)
        m_root->flushRemainingToDisk();
}

void Octree::getVisibleChunks(
    const Frustum& frustum, 
    glm::vec3 localCameraPos,
    std::vector<ChunkRenderInfo>& outChunks
) {
    if (m_root)
        m_root->getVisibleChunks(frustum, localCameraPos, outChunks);
}

Bound3D Octree::getTotalBounds() const {
    return m_root->getBounds();
}
