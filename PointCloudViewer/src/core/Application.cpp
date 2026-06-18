#include "stdafx.h"
#include "Application.h"
#include "Renderer.h"
#include "PointCloudManager.h"
#include "Shader.h"
#include "ShaderLayout.h"

Application::Application(GLFWwindow* window) : m_window(window) {
    m_context = std::make_unique<RenderContext>(window);
    m_swapchain = std::make_unique<RenderSwapchain>(m_context.get(), window);
    m_commandManager = std::make_unique<CommandManager>(m_context.get(), m_swapchain.get());

    createGlobalResources();
    initImGuiResources();
    createSyncObjects();

    m_scene = std::make_unique<MainScene>(m_window, m_context.get());
    m_scene->onEnter();
}

Application::~Application() {
    if (m_context) {
        vkDeviceWaitIdle(m_context->getDevice());

        m_scene.reset();

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        if (m_imageAvailableSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_context->getDevice(), m_imageAvailableSemaphore, nullptr);
        }

        if (m_renderFinishedSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_context->getDevice(), m_renderFinishedSemaphore, nullptr);
        }

        if (m_inFlightFence != VK_NULL_HANDLE) {
            vkDestroyFence(m_context->getDevice(), m_inFlightFence, nullptr);
        }

        cleanupGlobalResources();

        m_commandManager.reset();
        m_swapchain.reset();
        m_context.reset();
    }
}

void Application::dispatchEvent(const Event& event) {
    if (m_scene) {
        m_scene->onEvent(event);
    }
}

void Application::drawFrame(float elapsedTimeSec) {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (m_scene) {
        m_scene->update(elapsedTimeSec);
        m_scene->postUpdate(elapsedTimeSec);
    }

    VkResult res = vkWaitForFences(m_context->getDevice(), 1, &m_inFlightFence, VK_TRUE, UINT64_MAX);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(std::format("Failed to wait for in-flight fence! (CODE:{:#08x})", (int)res));
    }

    uint32_t imageIndex = 0;
    res = vkAcquireNextImageKHR(
        m_context->getDevice(),
        m_swapchain->getSwapchain(),
        UINT64_MAX,
        m_imageAvailableSemaphore,
        VK_NULL_HANDLE,
        &imageIndex
    );

    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    } else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error(
            std::format(
                "Failed to acquire swapchain image! (CODE:{:#08x})", (int)res
            )
        );
    }

    res = vkResetFences(m_context->getDevice(), 1, &m_inFlightFence);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(std::format("Failed to reset in-flight fence! (CODE:{:#08x})", (int)res));
    }

    VkCommandBuffer cmd = m_commandManager->getCommandBuffer(imageIndex);
    res = vkResetCommandBuffer(cmd, NULL);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(std::format("Failed to reset command buffer! (CODE:{:#08x})", (int)res));
    }

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    res = vkBeginCommandBuffer(cmd, &beginInfo);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(std::format("Failed to begin recording command buffer! (CODE:{:#08x})", (int)res));
    }

    RenderQueue queue;

    // 1. 준비: 렌더 큐 수집 및 정렬, 전역 데이터 UBO 업데이트
    prepareRenderQueue(imageIndex, queue);

    // 2. 메인 패스: 파이프라인 배리어, 렌더 패스 시작, 렌더 큐 일괄 처리, 프레젠트 배리어
    renderMainPass(cmd, imageIndex, queue);

    res = vkEndCommandBuffer(cmd);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(std::format("Failed to end recording command buffer! (CODE:{:#08x})", (int)res));
    }

    // 큐 제출
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    VkSemaphore waitSemaphores[] = {m_imageAvailableSemaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    VkSemaphore signalSemaphores[] = {m_renderFinishedSemaphore};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    res = vkQueueSubmit(m_context->getGraphicsQueue(), 1, &submitInfo, m_inFlightFence);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format(
                "Failed to submit draw command buffer! (CODE:{:#08x})", (int)res
            )
        );
    }

    // 프레젠테이션
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapchains[] = {m_swapchain->getSwapchain()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    res = vkQueuePresentKHR(m_context->getGraphicsQueue(), &presentInfo);
    if (res == VK_ERROR_OUT_OF_DATE_KHR ||
        res == VK_SUBOPTIMAL_KHR || m_framebufferResized) {
        m_framebufferResized = false;
        recreateSwapchain();
    } else if (res != VK_SUCCESS) {
        throw std::runtime_error(std::format("Failed to present swapchain image! (CODE:{:#08x})", (int)res));
    }
}

void Application::cleanupGlobalResources() {
    for (const auto& res : m_globalResources) {
        if (res.buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(m_context->getAllocator(), res.buffer, res.allocation);
        }
    }
}

void Application::createGlobalResources() {
    uint32_t imageCount = m_swapchain->numSwapchainImages();
    m_globalResources.resize(imageCount);

    for (uint32_t i = 0; i < imageCount; ++i) {
        // 전역 유니폼 버퍼 객체 생성
        VkBufferCreateInfo uboCreateInfo = {};
        uboCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        uboCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        uboCreateInfo.size = sizeof(GlobalData);

        VmaAllocationCreateInfo uboAllocInfo = {};
        uboAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        uboAllocInfo.flags =
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo allocationInfo = {};
        VkResult res = vmaCreateBuffer(
            m_context->getAllocator(),
            &uboCreateInfo,
            &uboAllocInfo,
            &m_globalResources[i].buffer,
            &m_globalResources[i].allocation,
            &allocationInfo
        );
        if (res != VK_SUCCESS) {
            throw std::runtime_error(
                std::format(
                    "Failed to create Global uniform buffer! (CODE:{:#08x})",
                    (int)res
                )
            );
        }

        m_globalResources[i].mappedGlobalData = allocationInfo.pMappedData;

        // 디스크립터 세트 할당
        m_globalResources[i].descriptorSet = m_context->allocDescriptorSet(m_context->getGlobalDescriptorSetLayout());

        // 디스크립터 세트 쓰기 업데이트
        VkDescriptorBufferInfo uboBufferInfo = {};
        uboBufferInfo.buffer = m_globalResources[i].buffer;
        uboBufferInfo.range = sizeof(GlobalData);
        uboBufferInfo.offset = 0;

        std::vector<VkWriteDescriptorSet> writes(1);
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = m_globalResources[i].descriptorSet;
        writes[0].dstBinding = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].descriptorCount = 1;
        writes[0].pBufferInfo = &uboBufferInfo;

        vkUpdateDescriptorSets(
            m_context->getDevice(),
            static_cast<uint32_t>(writes.size()),
            writes.data(),
            0,
            nullptr
        );
    }
}

void Application::initImGuiResources() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(m_window, true);

    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Device = m_context->getDevice();
    initInfo.Instance = m_context->getInstance();
    initInfo.PhysicalDevice = m_context->getPhysicalDevice();
    initInfo.Queue = m_context->getGraphicsQueue();
    initInfo.QueueFamily = m_context->getGraphicsQueueFamilyIndex();
    initInfo.DescriptorPool = m_context->getGuiDescriptorPool();
    initInfo.Subpass = 0;
    initInfo.MinImageCount = m_swapchain->numSwapchainImages();
    initInfo.ImageCount = initInfo.MinImageCount;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.Allocator = nullptr;
    initInfo.CheckVkResultFn = [](VkResult res) {
        if (res != VK_SUCCESS) {
            throw std::runtime_error(
                std::format("[ImGui Vulkan] Error Code:{:#08x}", (int)res)
            );
        }
    };

    initInfo.UseDynamicRendering = true;
    initInfo.PipelineRenderingCreateInfo = {};
    initInfo.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    initInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    initInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &RenderSwapchain::swapchainImageFormat;
    initInfo.PipelineRenderingCreateInfo.depthAttachmentFormat = RenderSwapchain::depthImageFormat;

    ImGui_ImplVulkan_Init(&initInfo);
}

void Application::createSyncObjects() {
    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkResult res = vkCreateSemaphore(m_context->getDevice(), &semaphoreInfo, nullptr, &m_imageAvailableSemaphore);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format(
                "Failed to create synchronization object! (CODE:{:#08x})",
                (int)res
            )
        );
    }

    res = vkCreateSemaphore(m_context->getDevice(), &semaphoreInfo, nullptr, &m_renderFinishedSemaphore);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format(
                "Failed to create synchronization object! (CODE:{:#08x})",
                (int)res
            )
        );
    }

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    res = vkCreateFence(m_context->getDevice(), &fenceInfo, nullptr, &m_inFlightFence);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format(
                "Failed to create synchronization object! (CODE:{:#08x})",
                (int)res
            )
        );
    }
}

void Application::recreateSwapchain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(m_window, &width, &height);

    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(m_window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(m_context->getDevice());
    
    vkResetDescriptorPool(m_context->getDevice(), m_context->getDescriptorPool(), NULL);
    cleanupGlobalResources();
    m_commandManager.reset();
    m_swapchain.reset();

    m_swapchain = std::make_unique<RenderSwapchain>(m_context.get(), m_window);
    m_commandManager = std::make_unique<CommandManager>(m_context.get(), m_swapchain.get());
    createGlobalResources();
}

void Application::prepareRenderQueue(uint32_t frameIndex, RenderQueue& queue) {
    if (m_scene == nullptr) {
        return;
    }

    queue.clear();
    m_scene->render(queue);
    queue.sort();

    GlobalData* ubo = static_cast<GlobalData*>(m_globalResources[frameIndex].mappedGlobalData);
    *ubo = queue.getGlobalData();
}

void Application::renderMainPass(VkCommandBuffer cmd, uint32_t frameIndex, RenderQueue& queue) {
    // 스왑체인 이미지를 COLOR_ATTACHMENT_OPTIMAL 레이아웃으로 전환
    RenderUtils::transitionImageLayout(
        cmd,
        m_swapchain->getImages()[frameIndex],
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    );

    // 동적 렌더링 시작
    int width, height;
    glfwGetFramebufferSize(m_window, &width, &height);

    VkRenderingAttachmentInfo colorAttachment = {};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = m_swapchain->getImageViews()[frameIndex];
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color = { 0.1f, 0.1f, 0.1f, 1.0f };

    VkRenderingAttachmentInfo depthAttachment = {};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = m_swapchain->getDepthImageView();
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.clearValue.depthStencil = { 1.0f, 0 };

    VkRenderingInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.offset = { 0, 0 };
    renderingInfo.renderArea.extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = &depthAttachment;

    vkCmdBeginRendering(cmd, &renderingInfo);

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(width);
    viewport.height = static_cast<float>(height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset = { 0, 0 };
    scissor.extent = renderingInfo.renderArea.extent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    if (m_scene) {
        m_scene->onPreRender(cmd);

        const auto& ptCmds = queue.getPointCloudCmds();
        PointCloudDataManager* currentManager = nullptr;
        Shader* currentShader = nullptr;
        VkPipelineLayout currentLayout = VK_NULL_HANDLE;
        std::vector<RenderNode> batchNodes;

        for (const auto& drawCmd : ptCmds) {
            bool stateChanged = (currentManager != drawCmd.manager) || (currentShader != drawCmd.shader);
            if (stateChanged) {
                if (currentManager != nullptr && !batchNodes.empty()) {
                    currentManager->bindGlobalBuffer(cmd);
                    currentManager->drawNodes(cmd, currentLayout, batchNodes);
                    batchNodes.clear();
                }
                if (currentShader != drawCmd.shader) {
                    currentShader = drawCmd.shader;
                    if (currentShader && currentShader->getLayout()) {
                        currentLayout = currentShader->getLayout()->getPipelineLayout();
                        currentShader->bind(cmd);
                        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, currentLayout, 0, 1, &m_globalResources[frameIndex].descriptorSet, 0, nullptr);
                    } else {
                        spdlog::error("[Render] Invalid shader or layout in draw command!");
                        currentLayout = VK_NULL_HANDLE;
                    }
                }
                currentManager = drawCmd.manager;
            }
            if (currentLayout != VK_NULL_HANDLE) {
                batchNodes.push_back(drawCmd.node);
            }
        }

        if (currentManager != nullptr && !batchNodes.empty()) {
            currentManager->bindGlobalBuffer(cmd);
            currentManager->drawNodes(cmd, currentLayout, batchNodes);
        }

        m_scene->onPostRender(cmd);
        m_scene->onGUI();
    }

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRendering(cmd);

    // 스왑체인 이미지를 PRESENT_SRC_KHR 레이아웃으로 전환
    RenderUtils::transitionImageLayout(
        cmd,
        m_swapchain->getImages()[frameIndex],
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    );
}
