#include "stdafx.h"
#include "Frustum.h"
#include "Octree.h"

void Plane::normalize() {
    float length = glm::length(normal);
    normal /= length;
    distance /= length;
}

Frustum::Frustum(const glm::mat4& vp) {
    // Left
    planes[0].normal.x = vp[0][3] + vp[0][0];
    planes[0].normal.y = vp[1][3] + vp[1][0];
    planes[0].normal.z = vp[2][3] + vp[2][0];
    planes[0].distance = vp[3][3] + vp[3][0];
    // Right
    planes[1].normal.x = vp[0][3] - vp[0][0];
    planes[1].normal.y = vp[1][3] - vp[1][0];
    planes[1].normal.z = vp[2][3] - vp[2][0];
    planes[1].distance = vp[3][3] - vp[3][0];
    // Bottom
    planes[2].normal.x = vp[0][3] + vp[0][1];
    planes[2].normal.y = vp[1][3] + vp[1][1];
    planes[2].normal.z = vp[2][3] + vp[2][1];
    planes[2].distance = vp[3][3] + vp[3][1];
    // Top
    planes[3].normal.x = vp[0][3] - vp[0][1];
    planes[3].normal.y = vp[1][3] - vp[1][1];
    planes[3].normal.z = vp[2][3] - vp[2][1];
    planes[3].distance = vp[3][3] - vp[3][1];
    // Near (Vulkan Clip Space: 0 to w)
    planes[4].normal.x = vp[0][2];
    planes[4].normal.y = vp[1][2];
    planes[4].normal.z = vp[2][2];
    planes[4].distance = vp[3][2];
    // Far
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
        // 평면의 법선(normal) 방향을 기준으로 가장 가까운 점을 찾습니다.
        glm::vec3 p = bound.min;
        if (planes[i].normal.x >= 0)
            p.x = bound.max.x;
        if (planes[i].normal.y >= 0)
            p.y = bound.max.y;
        if (planes[i].normal.z >= 0)
            p.z = bound.max.z;

        // 점이 평면 바깥에 있다면 이 박스는 시야 밖입니다.
        if (glm::dot(planes[i].normal, p) + planes[i].distance < 0.0f) {
            return false;
        }
    }
    return true;
}
