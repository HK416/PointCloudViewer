#pragma once

class Octree;
class TransferManager;
class PointCloudFileManager;
class PointCloudBufferManager;
struct Bound3D;
struct Frustum;

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

    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
    float m_moveSpeed = 10.0f;
    float m_lookSensitivity = 0.1f;
};

class PointCloudObject : public Object {
public:
    PointCloudObject() = delete;
    PointCloudObject(const PointCloudObject&) = delete;
    PointCloudObject(const std::filesystem::path& filePath, VkDevice device, VmaAllocator allocator, TransferManager* transferMgr);

public:
    void updateBufferState();
    void draw(const Frustum& frustum, VkCommandBuffer commandBuffer) const;

    Bound3D getTotalBounds() const;

protected:
    std::unique_ptr<Octree> m_octree = nullptr;
    std::unique_ptr<PointCloudFileManager> m_fileManager = nullptr;
    std::unique_ptr<PointCloudBufferManager> m_bufferManager = nullptr;

    glm::dvec3 m_localOffset{0.0};
};
