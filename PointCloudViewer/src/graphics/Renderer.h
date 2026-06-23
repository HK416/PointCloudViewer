#pragma once

class PointCloudDataManager;
class SkyboxObject;
class Shader;

/// @brief 셰이더에 전역적으로 전달되는 뷰, 투영 행렬 및 카메라 정보를 담는 구조체입니다.
struct alignas(16) GlobalData {
    glm::mat4 view{1.0f};
    glm::mat4 proj{1.0f};
    alignas(16) glm::vec3 cameraPos{0.0f, 0.0f, 0.0f};
    float pointSizeMultiplier{100.0f};
    float pointSizeMin{1.0f};
    float pointSizeMax{10.0f};
    float gamma{2.2f};
};

/// @brief Eye Dome Lighting(EDL) 후처리에 사용되는 파라미터 구조체입니다.
struct EDLParams {
    float screenWidth;
    float screenHeight;
    float strength = 3.0f;
    float radius = 1.4f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
};

/// @brief Vulkan 인스턴스, 디바이스 등 렌더링에 필요한 핵심 환경을 초기화하고 관리하는 컨텍스트 클래스입니다.
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

/// @brief 화면에 이미지를 출력하기 위한 스왑체인과 이미지 & 깊이 버퍼 리소스를 관리하는 클래스입니다.
class RenderSwapchain {
public:
    RenderSwapchain() = delete;
    RenderSwapchain(const RenderSwapchain&) = delete;
    RenderSwapchain& operator=(const RenderSwapchain&) = delete;

    RenderSwapchain(
        RenderContext* context,
        GLFWwindow* window,
        VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE
    );
    ~RenderSwapchain();

    VkExtent2D getExtent() const { return m_extent; }
    VkSwapchainKHR getSwapchain() const { return m_swapchain; }
    const std::vector<VkImage>& getImages() const { return m_swapchainImages; }
    const std::vector<VkImageView>& getImageViews() const { return m_swapchainImageViews; }
    uint32_t numSwapchainImages() const { return static_cast<uint32_t>(m_swapchainImageViews.size()); }

    VkImage getDepthImage() const { return m_depthImage; }
    VkImageView getDepthImageView() const { return m_depthImageView; }
    VkSampler getDepthSampler() const { return m_depthSampler; }

    VkImage getColorImage() const { return m_colorImage; }
    VkImageView getColorImageView() const { return m_colorImageView; }
    VkSampler getColorSampler() const { return m_colorSampler; }

private:
    void createSwapchainResources(int width, int height, VkSwapchainKHR oldSwapchain);
    void createColorResources(int width, int height);
    void createDepthResources(int width, int height);

private:
    /// @brief 소유하지 않는 클래스 맴버 변수.
    RenderContext* m_context = nullptr;

    VkExtent2D m_extent = {0, 0};
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;

    // Color
    VkImage m_colorImage = VK_NULL_HANDLE;
    VkImageView m_colorImageView = VK_NULL_HANDLE;
    VmaAllocation m_colorAllocation = VK_NULL_HANDLE;
    VkSampler m_colorSampler = VK_NULL_HANDLE;

    // Depth
    VkImage m_depthImage = VK_NULL_HANDLE;
    VkImageView m_depthImageView = VK_NULL_HANDLE;
    VmaAllocation m_depthAllocation = VK_NULL_HANDLE;
    VkSampler m_depthSampler = VK_NULL_HANDLE;

public:
    static const VkFormat swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    static const VkFormat depthImageFormat = VK_FORMAT_D32_SFLOAT;
};

/// @brief 렌더링 명령을 기록하고 제출하기 위한 커맨드 풀 및 커맨드 버퍼를 관리하는 클래스입니다.
class CommandManager {
public:
    CommandManager() = delete;
    CommandManager(const CommandManager&) = delete;
    CommandManager& operator=(const CommandManager&) = delete;

    CommandManager(RenderContext* context, RenderSwapchain* swapchain);
    ~CommandManager();

    VkCommandPool getCommandPool() const { return m_commandPool; }
    VkCommandBuffer getCommandBuffer(uint32_t index) { return m_commandBuffers[index]; }

    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer cmd, VkQueue queue);

private:
    void createCommandPool(uint32_t queueFamilyIndex);
    void createCommandBuffers(size_t swapchainImageCount);

private:
    /// @brief 소유하지 않는 클래스 맴버 변수.
    RenderContext* m_context = nullptr;

    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;
};

/// @brief 렌더링 및 Vulkan 관련 유틸리티 함수들을 제공하는 클래스입니다.
class RenderUtils {
public:
    static void transitionImageLayout(
        VkCommandBuffer cmd,
        VkImage image,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT
    );

    static void transitionImageLayout(
        VkCommandBuffer cmd,
        VkImage image,
        VkFormat format,
        uint32_t baseMip,
        uint32_t mipCount,
        uint32_t baseLayer,
        uint32_t layerCount,
        VkImageLayout oldLayout,
        VkImageLayout newLayout
    );
};

/// @brief 개별 렌더링 객체 노드의 ID, 정점 수, GPU 버퍼 내 슬롯 인덱스 및 변환 행렬을 가지는 구조체입니다.
struct RenderNode {
    uint64_t id;
    uint32_t vertexCount;
    size_t slotIndex;
    glm::mat4 transform;
};

/// @brief 카메라로부터의 거리와 렌더링할 노드 정보 등 포인트 클라우드 그리기 명령을 담는 구조체입니다.
struct PointCloudDrawCmd {
    float distanceToCamera;
    RenderNode node;
    Shader* shader;
    PointCloudDataManager* manager;

};

bool operator<(const PointCloudDrawCmd& a, const PointCloudDrawCmd& b);

/// @brief 렌더링할 그리기 명령들을 수집하고 카메라 거리를 기준으로 정렬하여 관리하는 큐 클래스입니다.
class RenderQueue {
public:
    void clear();

    void setCamera(
        const glm::mat4& view,
        const glm::mat4& proj,
        float nearPlane,
        float farPlane,
        const glm::vec3& pos
    );
    void setPointSizeParams(float multiplier, float minSize, float maxSize);
    void setEDLParams(float strength, float radius, bool enabled);
    void submitPointCloudCmd(const PointCloudDrawCmd& cmd);

    void sort();

    const std::vector<PointCloudDrawCmd>& getPointCloudCmds() const { return m_pointCloudCmds; }

private:
    std::vector<PointCloudDrawCmd> m_pointCloudCmds;

public:
    SkyboxObject* skybox = nullptr;
    GlobalData globalData;
    EDLParams edlParams;
    bool edlEnabled = true;
};
