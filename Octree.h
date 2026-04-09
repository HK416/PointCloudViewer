#pragma once
#include "Mesh.h"

struct Bound3D {
    glm::vec3 min;
    glm::vec3 max;

    glm::vec3 getCenter() const;
    bool contains(const glm::vec3& point) const;
};

enum class Dimension : size_t {
    PxPyPz, // 0b000
    PxPyNz, // 0b001
    PxNyPz, // 0b010
    PxNyNz, // 0b011
    NxPyPz, // 0b100
    NxPyNz, // 0b101
    NxNyPz, // 0b110
    NxNyNz, // 0b111
    MaxEnum,
};

struct _Node {
    size_t level;
    Bound3D bound;
    std::vector<_Node> children;
    std::vector<PointCloudVertex> data;
};

class Octree {
public:
    Octree() = delete;
    Octree(const Octree&) = delete;
    Octree(Bound3D bound);
    Octree(Bound3D bound, size_t maxLevel);
    Octree(Bound3D bound, size_t maxLevel, size_t capacity);

private:
    void setupTree(_Node& n, Bound3D bound, size_t maxLevel, size_t level, size_t capacity);

private:
    _Node m_root;
};
