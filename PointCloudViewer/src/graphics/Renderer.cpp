#include "stdafx.h"
#include "Renderer.h"

RenderContext::RenderContext(GLFWwindow* window) {
    createRenderInstance();
    createRenderSurface(window);
    createRenderDevice();
    createMemoryAllocator();
    createDescriptorPools();
    createDescriptorSetLayouts();
}

RenderContext::~RenderContext() {
    if (m_device != VK_NULL_HANDLE) {
        if (m_allocator != VK_NULL_HANDLE) {
            vmaDestroyAllocator(m_allocator);
        }

        vkDestroyDevice(m_device, nullptr);
    }

    if (m_instance != VK_NULL_HANDLE) {
        if (m_surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        }

        vkDestroyInstance(m_instance, nullptr);
    }
}

VkDescriptorSet RenderContext::allocDescriptorSet(VkDescriptorSetLayout layout) {
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkResult res = vkAllocateDescriptorSets(m_device, &allocInfo, &descriptorSet);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format(
                "Failed to allocate Engine descriptor set! (CODE:{:#08x})",
                (int)res
            )
        );
    }

    return descriptorSet;
}

void RenderContext::createRenderInstance() {
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    auto extensions = getRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkResult res = vkCreateInstance(&createInfo, nullptr, &m_instance);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format(
                "Failed to create Vulkan instance! (CODE:{:#08x})", (int)res
            )
        );
    }
}

void RenderContext::createRenderSurface(GLFWwindow* window) {
    VkResult res = glfwCreateWindowSurface(m_instance, window, nullptr, &m_surface);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format(
                "Failed to create window surface! (CODE:{:#08x})", (int)res
            )
        );
    }
}

void RenderContext::createRenderDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

    for (VkPhysicalDevice device : devices) {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        if (queueFamilyCount > 0) {
            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

            uint32_t graphicsFamily = UINT32_MAX;
            for (uint32_t i = 0; i < queueFamilyCount; ++i) {
                if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                    VkBool32 presentSupported = VK_FALSE;
                    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupported);

                    if (presentSupported) {
                        graphicsFamily = i;
                        break;
                    }
                }
            }

            if (graphicsFamily != UINT32_MAX) {
                m_physicalDevice = device;
                m_graphicsQueueFamilyIndex = graphicsFamily;
                break;
            }
        }
    }

    if (m_physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find a suitable GPU!");
    }

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = m_graphicsQueueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceVulkan12Features features12 = {};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.timelineSemaphore = VK_TRUE;

    VkPhysicalDeviceVulkan13Features features13 = {};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.pNext = &features12;
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;

    VkPhysicalDeviceFeatures2 deviceFeatures2 = {};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.pNext = &features13;
    deviceFeatures2.features.shaderTessellationAndGeometryPointSize = VK_TRUE;

    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &deviceFeatures2;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VkResult res = vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format(
                "Failed to create logical device! (CODE:{:#08x})", (int)res
            )
        );
    }

    vkGetDeviceQueue(m_device, m_graphicsQueueFamilyIndex, 0, &m_graphicsQueue);
}

void RenderContext::createMemoryAllocator() {
    VmaAllocatorCreateInfo createInfo = {};
    createInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    createInfo.device = m_device;
    createInfo.instance = m_instance;
    createInfo.physicalDevice = m_physicalDevice;

    VkResult res = vmaCreateAllocator(&createInfo, &m_allocator);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format(
                "Failed to create VMA allocator! (CODE:{:#08x})", (int)res
            )
        );
    }
}

void RenderContext::createDescriptorPools() {
    const std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
    };

    {
        VkDescriptorPoolCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        createInfo.maxSets = 1000;
        createInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        createInfo.pPoolSizes = poolSizes.data();

        VkResult res = vkCreateDescriptorPool(m_device, &createInfo, nullptr, &m_descriptorPool);
        if (res != VK_SUCCESS) {
            throw std::runtime_error(
                std::format(
                    "Failed to create Engine descriptor pool! (CODE:{:#08x})",
                    (int)res
                )
            );
        }
    }

    {
        VkDescriptorPoolCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        createInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        createInfo.maxSets = 500;
        createInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        createInfo.pPoolSizes = poolSizes.data();

        VkResult res = vkCreateDescriptorPool(m_device, &createInfo, nullptr, &m_guiDescriptorPool);
        if (res != VK_SUCCESS) {
            throw std::runtime_error(
                std::format(
                    "Failed to create ImGui descriptor pool! (CODE:{:#08x})",
                    (int)res
                )
            );
        }
    }
}

void RenderContext::createDescriptorSetLayouts() {
    {
        std::vector<VkDescriptorSetLayoutBinding> layoutBindings(1);
        layoutBindings[0].binding = 0;
        layoutBindings[0].stageFlags = VK_SHADER_STAGE_ALL;
        layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        layoutBindings[0].descriptorCount = 1;

        VkDescriptorSetLayoutCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        createInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
        createInfo.pBindings = layoutBindings.data();

        VkResult res = vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &m_globalLayout);
        if (res != VK_SUCCESS) {
            throw std::runtime_error(
                std::format(
                    "Failed to create Global descriptor set layout! "
                    "(CODE:{:#08x})",
                    (int)res
                )
            );
        }
    }
}

std::vector<const char*> RenderContext::getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    return extensions;
}

RenderSwapchain::RenderSwapchain(RenderContext* context, GLFWwindow* window) 
    : m_context(context) {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);

    createSwapchainResources(width, height);
    createDepthResources(width, height);
}

RenderSwapchain::~RenderSwapchain() {
    if (m_context) {
        if (m_depthImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(m_context->getDevice(), m_depthImageView, nullptr);
        }

        if (m_depthImage != VK_NULL_HANDLE) {
            vmaDestroyImage(m_context->getAllocator(), m_depthImage, m_depthAllocation);
        }

        for (VkImageView view : m_swapchainImageViews) {
            if (view != VK_NULL_HANDLE) {
                vkDestroyImageView(m_context->getDevice(), view, nullptr);
            }
        }

        if (m_swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(m_context->getDevice(), m_swapchain, nullptr);
        }
    }
}

void RenderSwapchain::createSwapchainResources(int width, int height) {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_context->getPhysicalDevice(), m_context->getSurface(), &caps);

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
        imageCount = caps.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_context->getSurface();
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = swapchainImageFormat;
    createInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    createInfo.imageExtent.width = static_cast<uint32_t>(width);
    createInfo.imageExtent.height = static_cast<uint32_t>(height);
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = caps.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    createInfo.clipped = VK_TRUE;

    VkResult res = vkCreateSwapchainKHR(m_context->getDevice(), &createInfo, nullptr, &m_swapchain);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format("Failed to create swapchain! (CODE:{:#08x})", (int)res)
        );
    }

    vkGetSwapchainImagesKHR(m_context->getDevice(), m_swapchain, &imageCount, nullptr);
    m_swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_context->getDevice(), m_swapchain, &imageCount, m_swapchainImages.data());

    m_swapchainImageViews.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i) {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swapchainImageFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;

        res = vkCreateImageView(m_context->getDevice(), &viewInfo, nullptr, &m_swapchainImageViews[i]);
        if (res != VK_SUCCESS) {
            throw std::runtime_error(
                std::format(
                    "Failed to create swapchain image view! (CODE:{:#08x})",
                    (int)res
                )
            );
        }
    }
}

void RenderSwapchain::createDepthResources(int width, int height) {
    VkImageCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.extent.width = static_cast<uint32_t>(width);
    createInfo.extent.height = static_cast<uint32_t>(height);
    createInfo.extent.depth = 1;
    createInfo.mipLevels = 1;
    createInfo.arrayLayers = 1;
    createInfo.format = depthImageFormat;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    createInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VkResult res = vmaCreateImage(
        m_context->getAllocator(),
        &createInfo,
        &allocInfo,
        &m_depthImage,
        &m_depthAllocation,
        nullptr
    );
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format(
                "Failed to create depth image via VMA! (CODE:{:#08x})", (int)res
            )
        );
    }

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_depthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = depthImageFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;

    res = vkCreateImageView(m_context->getDevice(), &viewInfo, nullptr, &m_depthImageView);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format(
                "Failed to create depth image view! (CODE:{:#08x})", (int)res
            )
        );
    }
}

CommandManager::CommandManager(RenderContext* context, RenderSwapchain* swapchain) : m_context(context) {
    createCommandPool(context->getGraphicsQueueFamilyIndex());
    createCommandBuffers(swapchain->numSwapchainImages());
}

CommandManager::~CommandManager() {
    if (m_context && m_commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_context->getDevice(), m_commandPool, nullptr);
    }
}

void CommandManager::createCommandPool(uint32_t queueFamilyIndex) {
    VkCommandPoolCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    createInfo.queueFamilyIndex = queueFamilyIndex;

    VkResult res = vkCreateCommandPool(m_context->getDevice(), &createInfo, nullptr, &m_commandPool);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format(
                "Failed to create command pool! (CODE:{:#08x})", (int)res
            )
        );
    }
}

void CommandManager::createCommandBuffers(size_t swapchainImageCount) {
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_commandPool;
    allocInfo.commandBufferCount = static_cast<uint32_t>(swapchainImageCount);

    m_commandBuffers.resize(swapchainImageCount);
    VkResult res = vkAllocateCommandBuffers(m_context->getDevice(), &allocInfo, m_commandBuffers.data());
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format(
                "Failed to allocate command buffer! (CODE:{:#08x})", (int)res
            )
        );
    }
}

void RenderUtils::transitionImageLayout(
    VkCommandBuffer cmd,
    VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    VkImageAspectFlags aspectMask) {
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspectMask;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = 0;
        sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    } else {
        throw std::invalid_argument("Unsupported layout transition!");
    }

    vkCmdPipelineBarrier(
        cmd,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
}
