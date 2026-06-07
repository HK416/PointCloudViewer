#pragma once

struct Bound3D;

struct Plane {
    glm::vec3 normal;
    float distance;

    void normalize();
};

struct Frustum {
    Plane planes[6];

    Frustum(const glm::mat4& vp);

    bool intersects(const Bound3D& bound) const;
};
