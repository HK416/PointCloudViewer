#pragma once

struct Bound3D;

/// @brief 3차원 공간상의 평면을 법선 벡터와 원점으로부터의 거리로 정의하는 구조체입니다.
struct Plane {
    glm::vec3 normal;
    float distance;

    void normalize();
};

/// @brief 카메라의 시야각에 의해 형성되는 절두체를 6개의 평면으로 정의하여 컬링(Culling)에 사용하는 구조체입니다.
struct Frustum {
    Plane planes[6];

    Frustum(const glm::mat4& vp);

    bool intersects(const Bound3D& bound) const;
};

