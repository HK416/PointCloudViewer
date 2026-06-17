#pragma once
#include "Scene.h"

class RenderContext;
class RenderSwapchain;
class CommandManager;

class Application {
public:
    Application() = delete;
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    Application(GLFWwindow* window);
    ~Application();

    void drawFrame(float elapsedTimeSec);
    void dispatchEvent(const Event& event);
    void setFramebufferResized() { m_framebufferResized = true; }

private:
    void cleanupGlobalResources();
    void createGlobalResources();
    void initImGuiResources();
    void createSyncObjects();

    void recreateSwapchain();

    void prepareRenderQueue(uint32_t frameIndex, RenderQueue& queue);
    void renderMainPass(VkCommandBuffer cmd, uint32_t frameIndex, RenderQueue& queue);

private:
    GLFWwindow* m_window = nullptr;

    std::unique_ptr<RenderContext> m_context;
    std::unique_ptr<RenderSwapchain> m_swapchain;
    std::unique_ptr<CommandManager> m_commandManager;

    std::unique_ptr<Scene> m_scene;

    struct FrameResource {
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        void* mappedGlobalData = nullptr;
    };

    std::vector<FrameResource> m_globalResources;

    VkSemaphore m_imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore m_renderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence m_inFlightFence = VK_NULL_HANDLE;

    bool m_framebufferResized = false;
};

