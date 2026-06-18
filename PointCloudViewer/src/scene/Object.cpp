#include "stdafx.h"
#include "Object.h"
#include "Renderer.h"
#include "Frustum.h"
#include "Octree.h"
#include "PointCloudManager.h"
#include "FileManager.h"
#include "Shader.h"

//
// ================ Object ================
//

Object::~Object() {
    removeParent();

    for (Object* child : m_children) {
        if (child) {
            child->removeParent();
        }
    }
}

void Object::update(float elapsedTimeSec) {
    onUpdate(elapsedTimeSec);

    if (m_transform.isDirty() || m_worldDirty) {
        updateWorldMatrix();
    }

    for (Object* child : m_children) {
        if (child) {
            child->update(elapsedTimeSec);
        }
    }
}

void Object::lateUpdate(float elapsedTimeSec) {
    onLateUpdate(elapsedTimeSec);

    for (Object* child : m_children) {
        if (child) {
            child->lateUpdate(elapsedTimeSec);
        }
    }
}

void Object::render(RenderQueue& queue) {
    for (Object* child : m_children) {
        if (child) {
            child->render(queue);
        }
    }
}

void Object::setParent(Object* newParent) {
    if (m_parent == newParent) {
        return;
    }

    if (m_parent) {
        auto& siblings =  m_parent->m_children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
    }

    m_parent = newParent;

    if (m_parent) {
        m_parent->m_children.push_back(this);
    }

    setWorldDirty();
}

void Object::removeParent() {
    setParent(nullptr);
}

void Object::addChild(Object* child) {
    if (child) {
        child->setParent(this);
    }
}

void Object::removeChild(Object* child) {
    if (child && child->m_parent == this) {
        child->setParent(nullptr);
    }
}

void Object::destroy() {
    if (m_isPendingDestroy) {
        return;
    }
    m_isPendingDestroy = true;

    if (m_parent && !m_parent->isPendingDestroy()) {
        removeParent();
    }
    m_parent = nullptr;

    for (Object* child : m_children) {
        if (child) {
            child->destroy();
        }
    }

    m_children.clear();
}

void Object::setWorldDirty() {
    m_worldDirty = true;
    for (Object* child : m_children) {
        child->setWorldDirty();
    }
}

void Object::updateWorldMatrix() {
    if (m_parent) {
        m_worldMatrix = m_parent->m_worldMatrix * m_transform.getLocalMatrix();
    } else {
        m_worldMatrix = m_transform.getLocalMatrix();
    }
    m_worldDirty = false;
}

//
// ================ Camera ================
//

void Camera::applyToQueue(RenderQueue& queue) {
    queue.setCamera(getViewMatrix(), getProjectionMatrix(), getTransform().getPosition());
}

//
// ================ PerspectiveCamera ================
//

PerspectiveCamera::PerspectiveCamera(GLFWwindow* window) : m_window(window) {
    setPerspective(45.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
}

void PerspectiveCamera::onUpdate(float elapsedTimeSec) {
    if (!m_window) return;

    glm::quat rot = m_transform.getRotation();
    glm::vec3 forwardDir = rot * glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 rightDir = rot * glm::vec3(1.0f, 0.0f, 0.0f);

    glm::vec3 pos = m_transform.getPosition();
    bool moved = false;

    if (glfwGetKey(m_window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(m_window, GLFW_KEY_UP) == GLFW_PRESS) {
        pos += forwardDir * m_moveSpeed * elapsedTimeSec;
        moved = true;
    }
    if (glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(m_window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        pos -= forwardDir * m_moveSpeed * elapsedTimeSec;
        moved = true;
    }
    if (glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(m_window, GLFW_KEY_LEFT) == GLFW_PRESS) {
        pos -= rightDir * m_moveSpeed * elapsedTimeSec;
        moved = true;
    }
    if (glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(m_window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        pos += rightDir * m_moveSpeed * elapsedTimeSec;
        moved = true;
    }

    if (moved) {
        m_transform.setPosition(pos);
    }
}

void PerspectiveCamera::setPerspective(
    float fovYDegree, float aspect, float nearZ, float farZ
) {
    m_fov = fovYDegree;
    m_aspect = aspect;
    m_nearZ = nearZ;
    m_farZ = farZ;

    m_projectionMatrix = glm::perspective(glm::radians(m_fov), m_aspect, m_nearZ, m_farZ);
    m_projectionMatrix[1][1] *= -1.0f;
}

void PerspectiveCamera::setAspectRatio(float aspect) {
    setPerspective(m_fov, aspect, m_nearZ, m_farZ);
}

const glm::mat4& PerspectiveCamera::getViewMatrix() const {
    return m_viewMatrix;
}

const glm::mat4& PerspectiveCamera::getProjectionMatrix() const {
    return m_projectionMatrix;
}

void PerspectiveCamera::updateWorldMatrix() {
    Object::updateWorldMatrix();
    m_viewMatrix = glm::inverse(m_worldMatrix);
}

//
// ================ PointCloudObject ================
//

PointCloudObject::PointCloudObject(
    Shader* shader,
    std::unique_ptr<PointCloudFileManager> fileManager,
    std::unique_ptr<Octree> octree,
    std::unique_ptr<PointCloudDataManager> pointCloudManager
) : m_shader(shader), m_fileManager(std::move(fileManager)), m_octree(std::move(octree)),
      m_pointCloudManager(std::move(pointCloudManager)) {}

void PointCloudObject::render(RenderQueue& queue) {
    const GlobalData& globalData = queue.getGlobalData();
    
    glm::mat4 viewProj = globalData.proj * globalData.view;
    glm::mat4 localViewProj = viewProj * m_worldMatrix;
    Frustum frustum(localViewProj);

    glm::mat4 invWorld = glm::inverse(m_worldMatrix);
    glm::vec3 localCameraPos = glm::vec3(invWorld * glm::vec4(globalData.cameraPos, 1.0f));

    auto visibleChunks = m_octree->getVisibleChunks(frustum, localCameraPos);

    // 거리에 따라 보이는 청크 정렬 (가까운 순)
    std::sort(visibleChunks.begin(), visibleChunks.end(), [&](const ChunkRenderInfo& a, const ChunkRenderInfo& b) {
        return glm::distance(localCameraPos, a.center) < glm::distance(localCameraPos, b.center);
    });

    // 최대 용량으로 요청을 제한하여 캐시 스래싱 방지
    size_t limit = std::min(visibleChunks.size(), m_pointCloudManager->getCapacity());

    std::vector<StreamRequest> requests;
    requests.reserve(limit);
    for (size_t i = 0; i < limit; ++i) {
        requests.push_back({visibleChunks[i].id, visibleChunks[i].span});
    }
    m_pointCloudManager->requestNodes(requests);
    m_pointCloudManager->updateStreamingState();

    for (const auto& chunk : visibleChunks) {
        RenderNode node;
        if (m_pointCloudManager->getRenderNode(chunk.id, node)) {
            node.transform = m_worldMatrix;
            float distance = glm::distance(localCameraPos, chunk.center);
            queue.submitPointCloudCmd(
                PointCloudDrawCmd{
                    distance, node, m_shader, m_pointCloudManager.get()
                }
            );
        }
    }

    Object::render(queue);
}
