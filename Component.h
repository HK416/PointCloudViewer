#pragma once

struct Transform {
    glm::vec3 m_scale{1.0f, 1.0f, 1.0f};
    glm::quat m_rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 m_position{0.0f, 0.0f, 0.0f};
};

struct WorldTransform {
    glm::mat4x4 m_worldMatrix{1.0f};
};

struct Camera {
    glm::mat4x4 m_viewProjMatrix;
    float m_fov = 45.0f;
    float m_near = 0.1f;
    float m_far = 10000.0f;
    float m_yaw = -90.0f;
    float m_pitch = 0.0f;
    float m_moveSpeed = 10.0f;
    float m_lookSensitivity = 0.1f;
};

struct Mesh {
    uint32_t m_vertexCount = 0;

    VkBuffer m_positionBuffer = VK_NULL_HANDLE;
    VmaAllocation m_positionAllocation = VK_NULL_HANDLE;

    VkBuffer m_colorBuffer = VK_NULL_HANDLE;
    VmaAllocation m_colorAllocation = VK_NULL_HANDLE;

    VkBuffer m_intensityBuffer = VK_NULL_HANDLE;
    VmaAllocation m_intensityAllocation = VK_NULL_HANDLE;

    void onDestroy(VmaAllocator allocator);

    std::vector<VkBuffer> getBuffers() const;
};

struct StandardMaterial {
    float m_pointSize = 1.0f;
    VkPipeline m_graphicsPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;

    void onDestroy(VkDevice device);
};

struct LasLoadRequest {
    std::wstring m_path;
};
