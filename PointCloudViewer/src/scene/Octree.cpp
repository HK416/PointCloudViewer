#include "stdafx.h"
#include "Octree.h"
#include "Frustum.h"

//
// ================ Bound3D ================
//

glm::vec3 Bound3D::getCenter() const {
    return 0.5f * (min + max);
}

glm::vec3 Bound3D::getSize() const {
    return max - min;
}

//
// ================ OctreeNode ================
//

OctreeNode::OctreeNode(uint64_t id, Bound3D bound, PointCloudFileManager* fileManager) 
    : m_fileManager(fileManager) {
    m_id = id;
    m_bound = bound;
    m_center = m_bound.getCenter();
    glm::vec3 voxelSize = m_bound.getSize() / glm::vec3(gridResolution);
    m_invVoxelSize = 1.0f / voxelSize;

    // Out-of-Core 방식으로 인해 최대치가 아닌 적은 양(약 128KB)만 미리 할당하여 재할당 오버헤드 최소화
    m_points.reserve(4096); 
}

void OctreeNode::insert(const PointCloudVertex& p) {
    if (!m_leafNode) {
        int vx = std::clamp(static_cast<int>((p.position.x - m_bound.min.x) * m_invVoxelSize.x), 0, gridResolution - 1);
        int vy = std::clamp(static_cast<int>((p.position.y - m_bound.min.y) * m_invVoxelSize.y), 0, gridResolution - 1);
        int vz = std::clamp(static_cast<int>((p.position.z - m_bound.min.z) * m_invVoxelSize.z), 0, gridResolution - 1);

        int index = vz * gridResolution * gridResolution + vy * gridResolution + vx;

        if (!m_voxelOccupancy[index]) {
            m_voxelOccupancy[index] = 1;
            m_lodPoints.emplace_back(p);
        }

        insertIntoChild(p);
        return;
    }

    m_points.emplace_back(p);
    m_totalPointCount++;

    if (m_totalPointCount >= maxCapacity) {
        // 이미 디스크로 임시 플러시된 데이터가 있다면 병합
        if (!m_chunkSpans.empty()) {
            std::vector<PointCloudVertex> allPoints;
            allPoints.reserve(m_totalPointCount);
            for (const auto& span : m_chunkSpans) {
                auto diskPoints = m_fileManager->readData(span);
                allPoints.insert(allPoints.end(), diskPoints.begin(), diskPoints.end());
            }
            allPoints.insert(allPoints.end(), m_points.begin(), m_points.end());

            m_points = std::move(allPoints);
            m_chunkSpans.clear();
        }

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

size_t OctreeNode::flushToDiskTemporary() {
    size_t flushed = 0;
    if (m_leafNode) {
        if (!m_points.empty()) {
            m_chunkSpans.push_back(m_fileManager->writeData(m_points));
            flushed += m_points.size();
            m_points.clear();
            m_points.shrink_to_fit();
        }
    } else {
        if (!m_lodPoints.empty()) {
            m_chunkSpans.push_back(m_fileManager->writeData(m_lodPoints));
            flushed += m_lodPoints.size();
            m_lodPoints.clear();
            m_lodPoints.shrink_to_fit();
        }
        for (auto& child : m_children) {
            if (child) {
                flushed += child->flushToDiskTemporary();
            }
        }
    }
    return flushed;
}

void OctreeNode::flushRemainingToDisk() {
    if (m_leafNode) {
        if (!m_chunkSpans.empty()) {
            if (!m_points.empty()) {
                m_chunkSpans.push_back(m_fileManager->writeData(m_points));
                m_points.clear();
            }
            // 최종 렌더링을 위해 파편화된 청크들을 하나의 연속된 청크로 병합 (Merge)
            std::vector<PointCloudVertex> allPoints;
            allPoints.reserve(m_totalPointCount);
            for (const auto& span : m_chunkSpans) {
                auto diskPoints = m_fileManager->readData(span);
                allPoints.insert(allPoints.end(), diskPoints.begin(), diskPoints.end());
            }
            m_chunkSpan = m_fileManager->writeData(allPoints);
            m_chunkSpans.clear();
            m_points.shrink_to_fit();
        } else if (!m_points.empty()) {
            m_chunkSpan = m_fileManager->writeData(m_points);
            m_points.clear();
            m_points.shrink_to_fit();
        }
    } else {
        if (!m_chunkSpans.empty()) {
            if (!m_lodPoints.empty()) {
                m_chunkSpans.push_back(m_fileManager->writeData(m_lodPoints));
                m_lodPoints.clear();
            }
            std::vector<PointCloudVertex> allPoints;
            for (const auto& span : m_chunkSpans) {
                auto diskPoints = m_fileManager->readData(span);
                allPoints.insert(allPoints.end(), diskPoints.begin(), diskPoints.end());
            }
            m_chunkSpan = m_fileManager->writeData(allPoints);
            m_chunkSpans.clear();
            m_lodPoints.shrink_to_fit();
        } else if (!m_lodPoints.empty()) {
            m_chunkSpan = m_fileManager->writeData(m_lodPoints);
            m_lodPoints.clear();
            m_lodPoints.shrink_to_fit();
        }
        m_voxelOccupancy.clear();
        m_voxelOccupancy.shrink_to_fit();
    }

    for (const auto& child : m_children) {
        if (child) {
            child->flushRemainingToDisk();
        }
    }
}

ChunkSpan OctreeNode::getChunkData() const {
    return m_chunkSpan;
}

void OctreeNode::getAllBounds(std::vector<Bound3D>& outBounds) const {
    outBounds.push_back(m_bound);
    for (const auto& child : m_children) {
        if (child) {
            child->getAllBounds(outBounds);
        }
    }
}

void OctreeNode::getVisibleChunks(
    const Frustum& frustum,
    glm::vec3 localCameraPos,
    std::vector<ChunkRenderInfo>& outChunks
) {
    if (!frustum.intersects(m_bound)) {
        return;
    }

    if (m_chunkSpan.pointCount > 0) {
        outChunks.push_back({m_id, m_chunkSpan, m_center});
    }

    float dist = glm::distance(localCameraPos, m_center);
    float nodeDiagonal = glm::length(m_bound.getSize());
    if (!m_leafNode && dist > nodeDiagonal * 3.0f) {
        return;
    }

    for (const auto& child : m_children) {
        if (child) {
            child->getVisibleChunks(frustum, localCameraPos, outChunks);
        }
    }
}

int OctreeNode::getOctantIndex(const glm::vec3& position) const {
    int index = 0x00;
    if (position.x >= m_center.x) {
        index |= 0x01;
    }
    if (position.y >= m_center.y) {
        index |= 0x02;
    }
    if (position.z >= m_center.z) {
        index |= 0x04;
    }

    return index;
}

void OctreeNode::createChild(int index) {
    glm::vec3 min = m_bound.min;
    glm::vec3 max = m_bound.max;

    if (index & 0x01) {
        min.x = m_center.x;
    } else {
        max.x = m_center.x;
    }

    if (index & 0x02) {
        min.y = m_center.y;
    } else {
        max.y = m_center.y;
    }

    if (index & 0x04) {
        min.z = m_center.z;
    } else {
        max.z = m_center.z;
    }

    Bound3D bound{min, max};
    uint64_t newId = m_id * 8 + index + 1;
    m_children[index] = std::make_unique<OctreeNode>(newId, bound, m_fileManager);
}

void OctreeNode::splitAndPushDown() {
    m_leafNode = false;

    m_voxelOccupancy.assign(maxCapacity, 0);
    m_lodPoints.reserve(4096);

    std::vector<PointCloudVertex> pushDownPoints;
    pushDownPoints.reserve(maxCapacity);

    for (const auto& p : m_points) {
        int vx = std::clamp(static_cast<int>((p.position.x - m_bound.min.x) * m_invVoxelSize.x), 0, gridResolution - 1);
        int vy = std::clamp(static_cast<int>((p.position.y - m_bound.min.y) * m_invVoxelSize.y), 0, gridResolution - 1);
        int vz = std::clamp(static_cast<int>((p.position.z - m_bound.min.z) * m_invVoxelSize.z), 0, gridResolution - 1);

        int index = vz * gridResolution * gridResolution + vy * gridResolution + vx;
        
        if (!m_voxelOccupancy[index]) {
            m_voxelOccupancy[index] = 1;
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

//
// ================ Octree ================
//

Octree::Octree(PointCloudFileManager* fileManager, Bound3D bound)
    : m_fileManager(fileManager) 
{
    m_root = std::make_unique<OctreeNode>(0, bound, m_fileManager);
}

void Octree::insert(const PointCloudVertex& p) {
    if (m_root) {
        m_root->insert(p);
        m_ramPointCount++;

        // Out-of-Core 임시 플러시
        if (m_ramPointCount >= m_maxRamPoints) {
            m_root->flushToDiskTemporary();
            // 플러시 이후 트리 전체의 RAM 상 포인트는 0이 됨 (underflow 방지)
            m_ramPointCount = 0;
        }
    }
}

void Octree::flushRemainingToDisk() {
    if (m_root) {
        m_root->flushRemainingToDisk();
    }
    if (m_fileManager) {
        m_fileManager->flush();
    }
}

std::vector<Bound3D> Octree::getAllBounds() const {
    std::vector<Bound3D> bounds;
    if (m_root) {
        m_root->getAllBounds(bounds);
    }
    return bounds;
}

std::vector<ChunkRenderInfo> Octree::getVisibleChunks(
    const Frustum& frustum, const glm::vec3& localCameraPos
) {
    std::vector<ChunkRenderInfo> chunks;
    if (m_root) {
        m_root->getVisibleChunks(frustum, localCameraPos, chunks);
    }
    return chunks;
}
