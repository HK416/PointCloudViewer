#include "stdafx.h"
#include "Frustum.h"
#include "Octree.h"

void Plane::normalize() {
    float length = glm::length(normal);
    normal /= length;
    distance /= length;
}

Frustum::Frustum(const glm::mat4& vp) {
    // 좌측 평면
    planes[0].normal.x = vp[0][3] + vp[0][0];
    planes[0].normal.y = vp[1][3] + vp[1][0];
    planes[0].normal.z = vp[2][3] + vp[2][0];
    planes[0].distance = vp[3][3] + vp[3][0];
    // 우측 평면
    planes[1].normal.x = vp[0][3] - vp[0][0];
    planes[1].normal.y = vp[1][3] - vp[1][0];
    planes[1].normal.z = vp[2][3] - vp[2][0];
    planes[1].distance = vp[3][3] - vp[3][0];
    // 하단 평면
    planes[2].normal.x = vp[0][3] + vp[0][1];
    planes[2].normal.y = vp[1][3] + vp[1][1];
    planes[2].normal.z = vp[2][3] + vp[2][1];
    planes[2].distance = vp[3][3] + vp[3][1];
    // 상단 평면
    planes[3].normal.x = vp[0][3] - vp[0][1];
    planes[3].normal.y = vp[1][3] - vp[1][1];
    planes[3].normal.z = vp[2][3] - vp[2][1];
    planes[3].distance = vp[3][3] - vp[3][1];
    // 근거리 평면 (Vulkan 클립 공간: 0 ~ w)
    planes[4].normal.x = vp[0][2];
    planes[4].normal.y = vp[1][2];
    planes[4].normal.z = vp[2][2];
    planes[4].distance = vp[3][2];
    // 원거리 평면
    planes[5].normal.x = vp[0][3] - vp[0][2];
    planes[5].normal.y = vp[1][3] - vp[1][2];
    planes[5].normal.z = vp[2][3] - vp[2][2];
    planes[5].distance = vp[3][3] - vp[3][2];

    for (int i = 0; i < 6; i++) {
        planes[i].normalize();
    }
}

bool Frustum::intersects(const Bound3D& bound) const {
    for (int i = 0; i < 6; i++) {
        glm::vec3 p = bound.min;
        if (planes[i].normal.x >= 0)
            p.x = bound.max.x;
        if (planes[i].normal.y >= 0)
            p.y = bound.max.y;
        if (planes[i].normal.z >= 0)
            p.z = bound.max.z;

        if (glm::dot(planes[i].normal, p) + planes[i].distance < 0.0f) {
            return false;
        }
    }
    return true;
}

