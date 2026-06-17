#pragma once
#include <memory>
#include <filesystem>
#include "Transform.h"

class RenderQueue;
class PointCloudFileManager;
class GlobalPointCloudManager;
class TransferManager;
class RenderContext;
class Octree;

// ========== 오브젝트 기본 클래스 ==========
class Object {
public:
    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;

    Object() = default;
    virtual ~Object();

    Transform& getTransform() { return m_transform; }
    const glm::mat4& getWorldMatrix() const { return m_worldMatrix; }
    Object* getParent() const { return m_parent; }
    const std::vector<Object*>& getChildren() const { return m_children; }

    void update(float elapsedTimeSec);
    void lateUpdate(float elapsedTimeSec);
    virtual void render(RenderQueue& queue);

    void setName(const std::string& name) { m_name = name; }
    const std::string& getName() const { return m_name; }

    void setParent(Object* newParent);
    void removeParent();
    void addChild(Object* child);
    void removeChild(Object* child);

    virtual void destroy();
    bool isPendingDestroy() const { return m_isPendingDestroy; }

protected:
    void setWorldDirty();
    virtual void updateWorldMatrix();
    virtual void onUpdate(float elapsedTimeSec) {}
    virtual void onLateUpdate(float elapsedTimeSec) {}

protected:
    std::string m_name = "Object";

    Transform m_transform;
    glm::mat4 m_worldMatrix{1.0f};

    Object* m_parent = nullptr;
    std::vector<Object*> m_children;

    bool m_worldDirty = true;
    bool m_isPendingDestroy = false;
};

// ========== 카메라 ==========
class Camera : public Object {
public:
    Camera(const Camera&) = delete;
    Camera& operator=(const Camera&) = delete;

    Camera() = default;
    virtual ~Camera() = default;

    virtual const glm::mat4& getViewMatrix() const = 0;
    virtual const glm::mat4& getProjectionMatrix() const = 0;

    void applyToQueue(RenderQueue& queue);
};

// ========== 원근(Perspective) 카메라 ==========
class PerspectiveCamera : public Camera {
public:
    PerspectiveCamera(const PerspectiveCamera&) = delete;
    PerspectiveCamera& operator=(const PerspectiveCamera&) = delete;

    PerspectiveCamera();
    virtual ~PerspectiveCamera() = default;

    void setPerspective(float fovYDegree, float aspect, float nearZ, float farZ);
    void setAspectRatio(float aspect);

    float getFovY() const { return m_fov; }
    float getAspectRatio() const { return m_aspect; }
    float getNearZ() const { return m_nearZ; }
    float getFarZ() const { return m_farZ; }

    virtual const glm::mat4& getViewMatrix() const override;
    virtual const glm::mat4& getProjectionMatrix() const override;

protected:
    virtual void updateWorldMatrix() override;

protected:
    float m_fov = 45.0f;
    float m_aspect = 1.0f;
    float m_nearZ = 0.1f;
    float m_farZ = 1000.0f;

    glm::mat4 m_viewMatrix{1.0f};
    glm::mat4 m_projectionMatrix{1.0f};
};

// ========== 포인트 클라우드 오브젝트 ==========
class PointCloudObject : public Object {
public:
    PointCloudObject() = delete;
    PointCloudObject(const PointCloudObject&) = delete;
    PointCloudObject& operator=(const PointCloudObject&) = delete;

    PointCloudObject(
        RenderContext* context, 
        std::unique_ptr<PointCloudFileManager> fileManager,
        std::unique_ptr<Octree> octree,
        std::unique_ptr<GlobalPointCloudManager> pointCloudManager,
        VkPipeline pipeline,
        VkPipelineLayout pipelineLayout
    );
    virtual ~PointCloudObject();

    virtual void render(RenderQueue& queue) override;

    GlobalPointCloudManager* getManager() const { return m_pointCloudManager.get(); }

private:
    std::unique_ptr<PointCloudFileManager> m_fileManager;
    std::unique_ptr<GlobalPointCloudManager> m_pointCloudManager;
    std::unique_ptr<Octree> m_octree;

    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
};
