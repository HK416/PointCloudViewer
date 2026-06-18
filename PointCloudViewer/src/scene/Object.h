#pragma once
#include "Transform.h"

class RenderQueue;
class PointCloudFileManager;
class PointCloudDataManager;
class TransferManager;
class RenderContext;
class Octree;
class Shader;

//
// ================ Object ================
//

/// @brief 씬(Scene) 내에 존재하는 모든 객체의 기본이 되며, 계층 구조와 트랜스폼을 가지는 최상위 클래스입니다.
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


//
// ================ Camera ================
//

/// @brief 뷰 행렬과 투영 행렬을 제공하는 씬 내의 카메라를 나타내는 추상 기저 클래스입니다.
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


//
// ================ PerspectiveCamera ================
//

/// @brief 원근 투영(Perspective Projection) 방식을 사용하는 카메라 클래스입니다.
class PerspectiveCamera : public Camera {
public:
    PerspectiveCamera(const PerspectiveCamera&) = delete;
    PerspectiveCamera& operator=(const PerspectiveCamera&) = delete;

    PerspectiveCamera(GLFWwindow* window = nullptr);
    virtual ~PerspectiveCamera() = default;

    void setMoveSpeed(float speed) { m_moveSpeed = speed; }
    float getMoveSpeed() const { return m_moveSpeed; }

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
    virtual void onUpdate(float elapsedTimeSec) override;

protected:
    struct GLFWwindow* m_window = nullptr;
    float m_moveSpeed = 20.0f;
    float m_fov = 45.0f;
    float m_aspect = 1.0f;
    float m_nearZ = 0.1f;
    float m_farZ = 1000.0f;

    glm::mat4 m_viewMatrix{1.0f};
    glm::mat4 m_projectionMatrix{1.0f};
};


//
// ================ PointCloudObject ================
//

/// @brief 씬 내에 배치되어 포인트 클라우드 데이터를 로드하고 렌더링하는 객체 클래스입니다.
class PointCloudObject : public Object {
public:
    PointCloudObject() = delete;
    PointCloudObject(const PointCloudObject&) = delete;
    PointCloudObject& operator=(const PointCloudObject&) = delete;

    PointCloudObject(
        Shader* shader,
        std::unique_ptr<PointCloudFileManager> fileManager,
        std::unique_ptr<Octree> octree,
        std::unique_ptr<PointCloudDataManager> pointCloudManager
    );
    virtual ~PointCloudObject() = default;

    virtual void render(RenderQueue& queue) override;

    PointCloudDataManager* getManager() const { return m_pointCloudManager.get(); }

private:
    std::unique_ptr<PointCloudFileManager> m_fileManager;
    std::unique_ptr<PointCloudDataManager> m_pointCloudManager;
    std::unique_ptr<Octree> m_octree;

    /// @brief 소유하지 않는 클래스 맴버 변수
    Shader* m_shader = nullptr;
};
