#include "stdafx.h"
#include "Octree.h"

const size_t DEF_LEVEL = 4;
const size_t DEF_CAPACITY = 4096;

glm::vec3 Bound3D::getCenter() const {
    return 0.5f * (min + max);
}

bool Bound3D::contains(const glm::vec3& point) const {
    return min.x <= point.x && point.x <= max.x && min.y <= point.y &&
           point.y <= max.y && min.z <= point.z && point.z <= max.z;
}

Octree::Octree(Bound3D bound) {
    setupTree(m_root, bound, DEF_LEVEL, 0, DEF_CAPACITY);
}

Octree::Octree(Bound3D bound, size_t maxLevel) {
    setupTree(m_root, bound, maxLevel, 0, DEF_CAPACITY);
}

Octree::Octree(Bound3D bound, size_t maxLevel, size_t capacity) {
    setupTree(m_root, bound, maxLevel, 0, capacity);
}

void Octree::setupTree(_Node& n, Bound3D bound, size_t maxLevel, size_t level, size_t capacity) {
    n.bound = bound;
    n.level = level;
    
    if (level < maxLevel) {
        n.children.resize(static_cast<size_t>(Dimension::MaxEnum));

        glm::vec3 subMin, subMax;
        glm::vec3 center = bound.getCenter();
        for (size_t i = 0; i < static_cast<size_t>(Dimension::MaxEnum); ++i) {

            // Bitwise mapping: 0 = Positive (Center to Max), 1 = Negative (Min
            // to Center) X-axis: bit 2
            if (i & 4) { // Nx
                subMin.x = bound.min.x;
                subMax.x = center.x;
            } else { // Px
                subMin.x = center.x;
                subMax.x = bound.max.x;
            }

            // Y-axis: bit 1
            if (i & 2) { // Ny
                subMin.y = bound.min.y;
                subMax.y = center.y;
            } else { // Py
                subMin.y = center.y;
                subMax.y = bound.max.y;
            }

            // Z-axis: bit 0
            if (i & 1) { // Nz
                subMin.z = bound.min.z;
                subMax.z = center.z;
            } else { // Pz
                subMin.z = center.z;
                subMax.z = bound.max.z;
            }

            Bound3D subBound{subMin, subMax};
            setupTree(n.children[i], subBound, maxLevel, level + 1, capacity);
        }
    } else {
        n.children.reserve(capacity);
    }
}
