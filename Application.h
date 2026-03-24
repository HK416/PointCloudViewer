#pragma once

#include <vector>

class Application {
public:
    Application() = delete;
    Application(const Application&) = delete;
    Application(LPCWSTR title, LONG width, LONG height);
    ~Application();

public:
    void run();

    static LRESULT CALLBACK
    wndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
    HWND createWindow(
        HINSTANCE hInstance,
        LPCWSTR className,
        LPCWSTR title,
        LONG width,
        LONG height
    );

    // Vulkan Initialization Helpers
    void createRenderInstance();
    void createRenderSurface(HINSTANCE hInstance, HWND hWnd);
    void createRenderDevice();
    void createRenderSwapchain(LONG width, LONG height);
    void createImageViews();
    void createRenderPass();
    void createDepthResources();
    void createFramebuffers();

    // Rendering Logic Helpers
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();
    void drawFrame();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

private:
    HINSTANCE m_hInstance = NULL;
    HWND m_hWnd = NULL;

    // Vulkan Core
    VkInstance m_instance = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    uint32_t m_graphicsQueueFamilyIndex = 0;

    // VMA Allocator
    VmaAllocator m_allocator = VK_NULL_HANDLE;

    // Swapchain (Render Targets)
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkFormat m_swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkExtent2D m_swapchainExtent = {};
    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;

    // Depth Stencil
    VkImage m_depthImage = VK_NULL_HANDLE;
    VmaAllocation m_depthImageAllocation = VK_NULL_HANDLE;
    VkImageView m_depthImageView = VK_NULL_HANDLE;

    // Framebuffer Resources
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_framebuffers;

    // Command & Sync
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;

    VkSemaphore m_imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore m_renderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence m_inFlightFence = VK_NULL_HANDLE;
};
