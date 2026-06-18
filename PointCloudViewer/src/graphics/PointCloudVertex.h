#pragma once
#include <glm/glm.hpp>

/// @brief 포인트 클라우드의 단일 정점을 나타내는 구조체입니다.
struct alignas(16) PointCloudVertex {
    glm::vec3 position{0.0f};
    uint32_t _padding0{0};
    glm::vec3 color{1.0f};
    uint32_t _padding1{0};
};
