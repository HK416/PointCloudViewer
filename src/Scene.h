#pragma once

#include "Object.h"
#include "TransferManager.h"

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
private:
    struct UniformBufferData {
        float pointSize;
        float minZ;
        float maxZ;
        long viewMode;
    };

public:
    MainScene() = delete;
    MainScene(const MainScene&) = delete;
    MainScene(
        HWND hWnd,
        VkDevice device,
        VkQueue graphicsQueue,
        VmaAllocator allocator,
        VkCommandPool commandPool,
        TransferManager* transferMgr
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
    TransferManager* m_transferMgr;

    std::bitset<256> m_keys;
    glm::vec2 m_lastMousePos{0.0f};
    glm::vec2 m_mouseDelta{0.0f};
    bool m_rightMouseDown = false;
    POINT m_capturedMousePos{0, 0};

    VkBuffer m_uniformBuffer = VK_NULL_HANDLE;
    VmaAllocation m_uniformAllocation = VK_NULL_HANDLE;
    void* m_uniformMapped = nullptr;

    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;

    VkRect2D m_scissor;
    VkViewport m_viewport;
    VkPipeline m_graphicsPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;

    CameraObject m_camera;
    std::unique_ptr<PointCloudObject> m_pointCloud;

    bool m_wasLoading = false;
    std::atomic<bool> m_isLoading{false};
    std::future<std::unique_ptr<PointCloudObject>> m_loadingFuture;

    float m_pointSize = 2.0f;
    int m_viewMode = 0;
    bool m_showDebugView = false;
};
