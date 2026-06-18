#pragma once
#include "Scene.h"

class RenderContext;
class RenderSwapchain;
class CommandManager;

/// @brief 프로그램의 메인 루프 및 전역 리소스를 관리하는 애플리케이션 클래스입니다.
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
    /// @brief 소유하지 않는 클래스 맴버 변수.
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

