#pragma once

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
private:

public:
    PointCloudObject() = delete;
    PointCloudObject(const PointCloudObject&) = delete;
    PointCloudObject(LPCWSTR filepath, VkDevice device, VmaAllocator allocator);
    virtual ~PointCloudObject();

    int getVertexCount() const;
    std::vector<VkBuffer> getBuffers() const;

protected:
    VmaAllocator m_allocator;

    int m_vertexCount = 0;

    VkBuffer m_positionBuffer = VK_NULL_HANDLE;
    VmaAllocation m_positionAllocation = VK_NULL_HANDLE;

    VkBuffer m_colorBuffer = VK_NULL_HANDLE;
    VmaAllocation m_colorAllocation = VK_NULL_HANDLE;
};
