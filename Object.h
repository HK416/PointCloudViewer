#pragma once
#include "Mesh.h"

class Object {
public:
    virtual ~Object() { }

    glm::vec3 m_scale{1.0f, 1.0f, 1.0f};
    glm::quat m_rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 m_position{0.0f, 0.0f, 0.0f};

    glm::mat4 m_worldMatrix{1.0f};
};

class CameraObject : public Object {
public:
    virtual ~CameraObject() {}

    float m_fov = 45.0f;
    float m_near = 0.1f;
    float m_far = 1000.0f;

    float m_yaw = -90.0f;
    float m_pitch = 0.0f;
    float m_moveSpeed = 10.0f;
    float m_lookSensitivity = 0.1f;
};

class PointCloudObject : public Object {
public:
    PointCloudObject() = delete;
    PointCloudObject(const PointCloudObject&) = delete;
    PointCloudObject(LPCWSTR filepath, VkDevice device, VmaAllocator allocator, VkCommandBuffer commandBuffer);

    const std::unique_ptr<PointCloudMesh>& getMesh() const;

protected:
    std::unique_ptr<PointCloudMesh> m_mesh = nullptr;
};
