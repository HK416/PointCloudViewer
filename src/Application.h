#pragma once

#include "Scene.h"
#include "TransferManager.h"

class Application {
public:
    Application(LPCWSTR title, LONG width, LONG height);
    ~Application();

    void run();

private:
    HWND createWindow(HINSTANCE hInstance, LPCWSTR className, LPCWSTR title, LONG width, LONG height);
    
    // Core Initializers
    void createRenderInstance();
    void createRenderSurface(HINSTANCE hInstance, HWND hWnd);
    void createRenderDevice();
    
    // Resource Layouts
    void createRenderSwapchain(LONG width, LONG height);
    void createImageViews();
    void createDepthResources();

    // Execution Objects
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();

    // Loop logic
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void drawFrame();
    void recreateSwapchain();

    LRESULT onHandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK wndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
    HINSTANCE m_hInstance;
    HWND m_hWnd;

    VkInstance m_instance = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    uint32_t m_graphicsQueueFamilyIndex = 0;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;

    VmaAllocator m_allocator = VK_NULL_HANDLE;

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkFormat m_swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkExtent2D m_swapchainExtent;
    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;

    VkImage m_depthImage = VK_NULL_HANDLE;
    VmaAllocation m_depthImageAllocation = VK_NULL_HANDLE;
    VkImageView m_depthImageView = VK_NULL_HANDLE;

    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;

    VkSemaphore m_imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore m_renderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence m_inFlightFence = VK_NULL_HANDLE;

    bool m_framebufferResized = false;

    std::unique_ptr<TransferManager> m_transferManager;
    std::unique_ptr<MainScene> m_scene;
};
