#pragma once

struct Time {
    float m_deltaTime = 0.0f;
    float m_totalTime = 0.0f;
};

struct RenderContext {
    VkDevice m_device = VK_NULL_HANDLE;
    VmaAllocator m_allocator = VK_NULL_HANDLE;
    VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
};

struct InputState {
    bool m_keys[256] = { false };
    glm::vec2 m_lastMousePos{ 0.0f };
    glm::vec2 m_mouseDelta{ 0.0f };
    bool m_rightMouseDown = false;
};
