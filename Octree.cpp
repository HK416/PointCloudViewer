#include "stdafx.h"
#include "Octree.h"

glm::vec3 Bound3D::getCenter() const {
    return 0.5f * (min + max);
}

glm::vec3 Bound3D::getSize() const {
    return max - min;
}

OctreeNode::OctreeNode(Bound3D bound, PointCloudFileManager& fm) : m_fileManager(fm) {
    m_bound = bound;
    m_points.reserve(MAX_CAPACITY);
}

void OctreeNode::insert(const PointCloudVertex& p) {
    if (!m_leafNode) {
        insertIntoChild(p);
        return;
    }

    m_points.emplace_back(p);

    if (m_points.size() >= MAX_CAPACITY) {
        splitAndPushDown();
    }
}

void OctreeNode::flushRemainingToDisk() {
    if (m_leafNode && !m_points.empty()) {
        m_chunkSpan = m_fileManager.writeData(m_points);
        m_points.clear();
        m_points.shrink_to_fit();
    }

    for (auto& child : m_children) {
        if (child)
            child->flushRemainingToDisk();
    }
}

ChunkSpan OctreeNode::getChunkData() const {
    return m_chunkSpan;
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
    m_children[index] = std::make_unique<OctreeNode>(bound, m_fileManager);
}

void OctreeNode::flushLODToDisk(const std::vector<PointCloudVertex>& lodPoints) {
    if (lodPoints.empty())
        return;

    m_chunkSpan = m_fileManager.writeData(lodPoints);
}

void OctreeNode::insertIntoChild(const PointCloudVertex& p) {
    int octantIndex = getOctantIndex(p.position);
    if (!m_children[octantIndex]) {
        createChild(octantIndex);
    }
    m_children[octantIndex]->insert(p);
}

void OctreeNode::splitAndPushDown() {
    m_leafNode = false;

    std::vector<bool> voxelOccupancy;
    voxelOccupancy.resize(GRID_RES * GRID_RES * GRID_RES, false);
    glm::vec3 voxelSize = m_bound.getSize() / glm::vec3(GRID_RES);

    std::vector<PointCloudVertex> lodPoints;
    std::vector<PointCloudVertex> pushDownPoints;
    
    lodPoints.reserve(GRID_RES * GRID_RES * GRID_RES);
    pushDownPoints.reserve(MAX_CAPACITY);

    for (const auto& p : m_points) {
        int vx = std::clamp(static_cast<int>((p.position.x - m_bound.min.x) / voxelSize.x), 0, GRID_RES - 1);
        int vy = std::clamp(static_cast<int>((p.position.y - m_bound.min.y) / voxelSize.y), 0, GRID_RES - 1);
        int vz = std::clamp(static_cast<int>((p.position.z - m_bound.min.z) / voxelSize.z), 0, GRID_RES - 1);

        int index = vz * GRID_RES * GRID_RES + vy * GRID_RES + vx;

        if (!voxelOccupancy[index]) {
            voxelOccupancy[index] = true;
            lodPoints.emplace_back(p);
        } else {
            pushDownPoints.emplace_back(p);
        }
    }

    m_points.clear();
    m_points.shrink_to_fit();

    flushLODToDisk(lodPoints);

    for (const auto& p : pushDownPoints) {
        insertIntoChild(p);
    }
}

PointCloudFileManager::PointCloudFileManager(const std::string& filename) {
    m_dataFile.open(filename, std::ios::binary | std::ios::app);
    if (!m_dataFile.is_open()) {
        throw std::runtime_error("Cannot open data file!");
    }
    
    m_dataFile.seekp(0, std::ios::end);
    m_currentWriteOffset = m_dataFile.tellp();
}

PointCloudFileManager::~PointCloudFileManager() {
    if (m_dataFile.is_open())
        m_dataFile.close();
}

ChunkSpan PointCloudFileManager::writeData(const std::vector<PointCloudVertex>& points) {
    if (!m_dataFile.is_open() || points.empty())
        return {0, 0};

    size_t sizeInBytes = points.size() * sizeof(PointCloudVertex);
    ChunkSpan span{m_currentWriteOffset, points.size()};

    m_dataFile.write(reinterpret_cast<const char*>(points.data()), sizeInBytes);
    m_dataFile.flush();

    m_currentWriteOffset += sizeInBytes;
    return span;
}

Octree::Octree(Bound3D bound) : m_fileManager(std::format("temp.bin")) {
    m_root = std::make_unique<OctreeNode>(bound, m_fileManager);
}

void Octree::insert(const PointCloudVertex& p) {
    if (m_root)
        m_root->insert(p);
}

void Octree::flushRemainingToDisk() {
    if (m_root)
        m_root->flushRemainingToDisk();
}
