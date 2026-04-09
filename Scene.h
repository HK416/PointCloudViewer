#pragma once

#include "Object.h"

class Scene {
public:
    virtual ~Scene() {}

public:
    virtual void onUpdate(float elapsedTimeSec) = 0;
    virtual void onDraw(VkCommandBuffer commandBuffer) = 0;
    virtual void onResize(LONG width, LONG height) = 0;

    virtual void onHandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) = 0;
};

class MainScene : public Scene {
public:
    MainScene() = delete;
    MainScene(const MainScene&) = delete;
    MainScene(
        HWND hWnd,
        VkDevice device,
        VkQueue graphicsQueue,
        VmaAllocator allocator,
        VkCommandPool commandPool
    );
    virtual ~MainScene();

private:
    void buildPipeline(VkDevice device);

public:
    virtual void onUpdate(float elapsedTimeSec) override;
    virtual void onDraw(VkCommandBuffer commandBuffer) override;
    virtual void onResize(LONG width, LONG height) override;

    virtual void onHandleMessage(
        HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam
    ) override;

private:
    HWND m_hWnd;
    VkDevice m_device;
    VkQueue m_graphicsQueue;
    VmaAllocator m_allocator;
    VkCommandPool m_commandPool;

    std::bitset<256> m_keys;
    glm::vec2 m_lastMousePos{0.0f};
    glm::vec2 m_mouseDelta{0.0f};
    bool m_rightMouseDown = false;

    VkRect2D m_scissor;
    VkViewport m_viewport;
    VkPipeline m_graphicsPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;

    CameraObject m_camera;
    std::unique_ptr<PointCloudObject> m_pointCloud;
};
