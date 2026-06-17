#pragma once

struct alignas(16) GlobalData {
    glm::mat4 view{1.0f};
    glm::mat4 proj{1.0f};
    glm::vec3 cameraPos{0.0f, 0.0f, 0.0f};
    uint32_t _padding{0};
};

class RenderContext {
public:
    RenderContext() = delete;
    RenderContext(const RenderContext&) = delete;
    RenderContext& operator=(const RenderContext&) = delete;

    RenderContext(GLFWwindow* window);
    ~RenderContext();

    VkInstance getInstance() const { return m_instance; }
    VkSurfaceKHR getSurface() const { return m_surface; }
    VkPhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
    VkDevice getDevice() const { return m_device; }
    uint32_t getGraphicsQueueFamilyIndex() const { return m_graphicsQueueFamilyIndex; }
    VkQueue getGraphicsQueue() const { return m_graphicsQueue; }
    VmaAllocator getAllocator() const { return m_allocator; }

    VkDescriptorPool getDescriptorPool() const { return m_descriptorPool; }
    VkDescriptorPool getGuiDescriptorPool() const { return m_guiDescriptorPool; }
    VkDescriptorSetLayout getGlobalDescriptorSetLayout() const { return m_globalLayout; }

    VkDescriptorSet allocDescriptorSet(VkDescriptorSetLayout layout);

private:
    void createRenderInstance();
    void createRenderSurface(GLFWwindow* window);
    void createRenderDevice();
    void createMemoryAllocator();
    void createDescriptorPools();
    void createDescriptorSetLayouts();

    std::vector<const char*> getRequiredExtensions();

private:
    VkInstance m_instance = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    uint32_t m_graphicsQueueFamilyIndex = 0;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VmaAllocator m_allocator = VK_NULL_HANDLE;

    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorPool m_guiDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_globalLayout = VK_NULL_HANDLE;
};

class RenderSwapchain {
public:
    RenderSwapchain() = delete;
    RenderSwapchain(const RenderSwapchain&) = delete;
    RenderSwapchain& operator=(const RenderSwapchain&) = delete;

    RenderSwapchain(RenderContext* context, GLFWwindow* window);
    ~RenderSwapchain();

    VkSwapchainKHR getSwapchain() const { return m_swapchain; }
    const std::vector<VkImage>& getImages() const { return m_swapchainImages; }
    const std::vector<VkImageView>& getImageViews() const { return m_swapchainImageViews; }
    size_t numSwapchainImages() const { return m_swapchainImageViews.size(); }
    VkImage getDepthImage() const { return m_depthImage; }
    VkImageView getDepthImageView() const { return m_depthImageView; }

private:
    void createSwapchainResources(int width, int height);
    void createDepthResources(int width, int height);

private:
    RenderContext* m_context = nullptr;

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;

    VkImage m_depthImage = VK_NULL_HANDLE;
    VkImageView m_depthImageView = VK_NULL_HANDLE;
    VmaAllocation m_depthAllocation = VK_NULL_HANDLE;

public:
    static const VkFormat swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    static const VkFormat depthImageFormat = VK_FORMAT_D32_SFLOAT;
};

class CommandManager {
public:
    CommandManager() = delete;
    CommandManager(const CommandManager&) = delete;
    CommandManager& operator=(const CommandManager&) = delete;

    CommandManager(RenderContext* context, RenderSwapchain* swapchain);
    ~CommandManager();

    VkCommandPool getCommandPool() const { return m_commandPool; }
    VkCommandBuffer getCommandBuffer(uint32_t index) { return m_commandBuffers[index]; }

private:
    void createCommandPool(uint32_t queueFamilyIndex);
    void createCommandBuffers(size_t swapchainImageCount);

private:
    RenderContext* m_context = nullptr;

    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;
};

class RenderUtils {
public:
    static void transitionImageLayout(
        VkCommandBuffer cmd,
        VkImage image,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT
    );
};

class GlobalPointCloudManager;

struct RenderNode {
    uint64_t id;
    uint32_t vertexCount;
    size_t slotIndex;
    glm::mat4 transform;
};

struct PointCloudDrawCmd {
    float distanceToCamera;
    RenderNode node;
    GlobalPointCloudManager* manager;
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
};

class RenderQueue {
public:
    void clear() {
        m_pointCloudCmds.clear();
    }

    void setGlobalData(const GlobalData& data) { m_globalData = data; }
    const GlobalData& getGlobalData() const { return m_globalData; }

    void submitPointCloud(float distance, const RenderNode& node, GlobalPointCloudManager* manager, VkPipeline pipeline, VkPipelineLayout layout) {
        m_pointCloudCmds.push_back({distance, node, manager, pipeline, layout});
    }

    void sort() {
        std::sort(m_pointCloudCmds.begin(), m_pointCloudCmds.end(), 
            [](const PointCloudDrawCmd& a, const PointCloudDrawCmd& b) {
                return a.distanceToCamera < b.distanceToCamera;
            });
    }

    const std::vector<PointCloudDrawCmd>& getPointCloudCmds() const { return m_pointCloudCmds; }

private:
    GlobalData m_globalData;
    std::vector<PointCloudDrawCmd> m_pointCloudCmds;
};
