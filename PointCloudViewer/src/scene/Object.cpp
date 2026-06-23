#include "stdafx.h"
#include "Object.h"
#include "Renderer.h"
#include "Frustum.h"
#include "Octree.h"
#include "PointCloudManager.h"
#include "FileManager.h"
#include "Shader.h"
#include "ShaderLayout.h"
#include "Texture.h"

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

void PerspectiveCamera::applyToQueue(RenderQueue& queue) {
    queue.setCamera(
        getViewMatrix(),
        getProjectionMatrix(),
        m_nearZ,
        m_farZ,
        getTransform().getPosition()
    );
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
    
    glm::mat4 viewProj = queue.globalData.proj * queue.globalData.view;
    glm::mat4 localViewProj = viewProj * m_worldMatrix;
    Frustum frustum(localViewProj);

    glm::mat4 invWorld = glm::inverse(m_worldMatrix);
    glm::vec3 localCameraPos = glm::vec3(invWorld * glm::vec4(queue.globalData.cameraPos, 1.0f));

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

//
// ================ SkyboxObject ================
//

SkyboxObject::SkyboxObject(RenderContext* context, VkCommandBuffer cmd) : m_context(context) {
    createShader();
    createUniformBuffer();
    createCubemap(cmd);
    createDescriptorSet();
}

SkyboxObject::~SkyboxObject() {
    if (m_context) {
        if (m_buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(m_context->getAllocator(), m_buffer, m_allocation);
        }

        m_shader.reset();
        m_shaderLayout.reset();
        m_texture.reset();
    }
}

void SkyboxObject::applyToQueue(RenderQueue& queue) {
    queue.skybox = this;
}

void SkyboxObject::drawDirectly(VkCommandBuffer cmd) {
    if (m_dirty) {
        updateDescriptorSet();
        m_dirty = false;
    }

    m_shader->bind(cmd);
    vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_shaderLayout->getPipelineLayout(),
        1,
        1,
        &m_descriptorSet,
        0,
        nullptr
    );
    vkCmdDraw(cmd, 36, 1, 0, 0);
}

void SkyboxObject::createShader() {
    ShaderLayoutBuilder builder;
    builder.addDescriptorSetLayout(m_context->getGlobalDescriptorSetLayout());
    builder.addDescriptorSetLayout({
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT}
    });
    m_shaderLayout = builder.build(m_context);
    m_shader = std::make_unique<SkyboxShader>(m_context, m_shaderLayout.get());
}

void SkyboxObject::createUniformBuffer() {
    VkBufferCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.size = sizeof(SkyboxParams);
    createInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    VkResult res = vmaCreateBuffer(
        m_context->getAllocator(),
        &createInfo,
        &allocInfo,
        &m_buffer,
        &m_allocation,
        nullptr
    );
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format(
                "Failed to create skybox uniform buffer! (CODE:{:#08x})",
                (int)res
            )
        );
    }
}

void SkyboxObject::createCubemap(VkCommandBuffer cmd) {
    const std::filesystem::path filePath{"./assets/skybox.ktx2"};
    try {
        m_texture = KtxTextureBuilder().setFile(filePath, true).build(m_context, cmd);
    }
    catch (const std::exception& e) {
        spdlog::warn(e.what());
    }

    if (m_texture == nullptr) {
        uint32_t cubeMapColors[6] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
        std::vector<SubresourceData> subresources(6);
        for (size_t i = 0; i < subresources.size(); ++i) {
            subresources[i].arrayLayer = static_cast<uint32_t>(i);
            subresources[i].mipLevel = 0;
            subresources[i].width = 1;
            subresources[i].height = 1;
            subresources[i].size = 4;
            subresources[i].offset = static_cast<uint32_t>(i * 4);
        }

        m_texture = MemoryTextureBuilder()
                        .setRawDataWithSubresources(reinterpret_cast<const uint8_t*>(cubeMapColors), sizeof(cubeMapColors), 1, 1, 1, 6, VK_FORMAT_R8G8B8A8_SRGB, subresources)
                        .build(m_context, cmd);
    }
}

void SkyboxObject::createDescriptorSet() {
    const auto& layout = m_shaderLayout->getDescriptorSetLayouts();
    m_descriptorSet = m_context->allocDescriptorSet(layout[1]);
}

void SkyboxObject::updateDescriptorSet() {
    void* data = nullptr;
    vmaMapMemory(m_context->getAllocator(), m_allocation, &data);
    memcpy(data, &m_params, sizeof(SkyboxParams));
    vmaUnmapMemory(m_context->getAllocator(), m_allocation);

    std::vector<VkWriteDescriptorSet> writes(2);
    
    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer = m_buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(SkyboxParams);

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = m_descriptorSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo = &bufferInfo;

    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = m_texture->getImageView();
    imageInfo.sampler = m_texture->getSampler();

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_descriptorSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(
        m_context->getDevice(),
        static_cast<uint32_t>(writes.size()),
        writes.data(),
        0,
        nullptr
    );
}
