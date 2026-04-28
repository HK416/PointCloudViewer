#include "stdafx.h"
#include "Scene.h"
#include "Buffer.h"
#include "Frustum.h"
#include "Octree.h"

MainScene::MainScene(
    HWND hWnd,
    VkDevice device,
    VkQueue graphicsQueue,
    VmaAllocator allocator,
    VkCommandPool commandPool,
    TransferManager* transferMgr
) {
    m_hWnd = hWnd;
    m_device = device;
    m_graphicsQueue = graphicsQueue;
    m_allocator = allocator;
    m_commandPool = commandPool;
    m_transferMgr = transferMgr;

    RECT rect;
    if (GetClientRect(hWnd, &rect)) {
        m_viewport.x = 0.0f;
        m_viewport.y = 0.0f;
        m_viewport.width = static_cast<float>(rect.right - rect.left);
        m_viewport.height = static_cast<float>(rect.bottom - rect.top);
        m_viewport.minDepth = 0.0f;
        m_viewport.maxDepth = 1.0f;

        m_scissor.offset.x = 0;
        m_scissor.offset.y = 0;
        m_scissor.extent.width = rect.right - rect.left;
        m_scissor.extent.height = rect.bottom - rect.top;
    }

    buildPipeline(m_device);
}

MainScene::~MainScene() {
    if (m_graphicsPipeline) {
        vkDestroyPipeline(m_device, m_graphicsPipeline, VK_NULL_HANDLE);
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, VK_NULL_HANDLE);
    }

    if (m_device) {
        if (m_descriptorSet) vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        if (m_descriptorSetLayout) vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
        if (m_uniformBuffer) vmaDestroyBuffer(m_allocator, m_uniformBuffer, m_uniformAllocation);
    }
}

std::vector<uint32_t> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file: " + filename);
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

    file.seekg(0);
    file.read((char*)buffer.data(), fileSize);
    file.close();

    return buffer;
}

VkShaderModule createShaderModule(VkDevice device, const std::vector<uint32_t>& code) {
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size() * sizeof(uint32_t);
    createInfo.pCode = code.data();
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module!");
    }
    return shaderModule;
}

void MainScene::buildPipeline(VkDevice device) {
    // 1. Load SPIR-V Shaders from files
    auto vertShaderCode = readFile("vert.spv");
    auto fragShaderCode = readFile("frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(device, vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(device, fragShaderCode);

    VkDescriptorSetLayoutBinding uboLayoutBinding = {};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfoDS = {};
    layoutInfoDS.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfoDS.bindingCount = 1;
    layoutInfoDS.pBindings = &uboLayoutBinding;

    if (vkCreateDescriptorSetLayout(device, &layoutInfoDS, nullptr, &m_descriptorSetLayout)) {
        throw std::runtime_error("Failed to create descriptor set layout!");
    }

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeof(UniformBufferData);
    bufferInfo.usage = VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                      VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo vmaAllocInfo;
    if (vmaCreateBuffer(m_allocator, &bufferInfo, &allocInfo, &m_uniformBuffer, &m_uniformAllocation, &vmaAllocInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create uniform buffer!");
    }
    m_uniformMapped = vmaAllocInfo.pMappedData;

    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool!");
    }

    VkDescriptorSetAllocateInfo dsAllocInfo = {};
    dsAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsAllocInfo.descriptorPool = m_descriptorPool;
    dsAllocInfo.descriptorSetCount = 1;
    dsAllocInfo.pSetLayouts = &m_descriptorSetLayout;

    if (vkAllocateDescriptorSets(device, &dsAllocInfo, &m_descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets!");
    }

    VkDescriptorBufferInfo dbi = {};
    dbi.buffer = m_uniformBuffer;
    dbi.offset = 0;
    dbi.range = sizeof(UniformBufferData);

    VkWriteDescriptorSet descriptorWrite = {};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = m_descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &dbi;

    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

    VkPipelineShaderStageCreateInfo shaderStages[2] = {};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertShaderModule;
    shaderStages[0].pName = "main";

    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragShaderModule;
    shaderStages[1].pName = "main";

    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(glm::mat4);

    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &m_descriptorSetLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout!");
    }

    // Pipeline State
    VkVertexInputBindingDescription bindings[] = {
        {0, sizeof(PointCloudVertex), VK_VERTEX_INPUT_RATE_VERTEX}, // Point Cloud Vertex
    };
    VkVertexInputAttributeDescription attributes[] = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(PointCloudVertex, position)},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(PointCloudVertex, color)},
        {2, 0, VK_FORMAT_R32_SFLOAT, offsetof(PointCloudVertex, intensity)}
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = bindings;
    vertexInputInfo.vertexAttributeDescriptionCount = 3;
    vertexInputInfo.pVertexAttributeDescriptions = attributes;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    // Dynamic Rendering Info (Vulkan 1.3)
    VkFormat colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkPipelineRenderingCreateInfo pipelineRenderingInfo = {};
    pipelineRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    pipelineRenderingInfo.colorAttachmentCount = 1;
    pipelineRenderingInfo.pColorAttachmentFormats = &colorFormat;
    pipelineRenderingInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &pipelineRenderingInfo;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_pipelineLayout;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline!");
    }

    // Cleanup shader modules
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    vkDestroyShaderModule(device, fragShaderModule, nullptr);
}

void MainScene::onUpdate(float elapsedTimeSec) {
    if (m_isLoading && m_loadingFuture.valid()) {
        if (m_loadingFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            try {
                m_pointCloud = m_loadingFuture.get();
                m_camera.m_position = glm::vec3(0.0f);
                m_camera.m_rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            }
            catch (const std::exception& e) {
                ATL::CA2T msg(e.what());
                MessageBox(m_hWnd, msg, L"Load Error", MB_ICONERROR);
            }
            m_isLoading = false;
        }
    }

    // Rotate
    if (m_rightMouseDown) {
        m_camera.m_yaw += m_mouseDelta.x * m_camera.m_lookSensitivity;
        m_camera.m_pitch -= m_mouseDelta.y * m_camera.m_lookSensitivity;
    }
    m_mouseDelta = glm::vec2(0.0f); // Reset delta after use

    glm::vec3 front;
    front.x = cos(glm::radians(m_camera.m_yaw)) * cos(glm::radians(m_camera.m_pitch));
    front.y = sin(glm::radians(m_camera.m_pitch));
    front.z = sin(glm::radians(m_camera.m_yaw)) * cos(glm::radians(m_camera.m_pitch));
    
    glm::vec3 forward = glm::normalize(front);
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
    glm::vec3 up = glm::normalize(glm::cross(right, forward));

    // Synchronize object rotation with yaw/pitch
    m_camera.m_rotation = glm::quat(glm::vec3(glm::radians(m_camera.m_pitch), glm::radians(-m_camera.m_yaw - 90.0f), 0.0f));

    // Move
    float speed = m_camera.m_moveSpeed * elapsedTimeSec;
    if (m_keys['W'])
        m_camera.m_position += forward * speed;
    if (m_keys['S'])
        m_camera.m_position -= forward * speed;
    if (m_keys['A'])
        m_camera.m_position -= right * speed;
    if (m_keys['D'])
        m_camera.m_position += right * speed;
    if (m_keys['E'])
        m_camera.m_position += up * speed;
    if (m_keys['Q'])
        m_camera.m_position -= up * speed;

    // Update Mesh Status (Async Loading Check)
    if (m_pointCloud)
        m_pointCloud->updateBufferState();
}

void MainScene::onDraw(VkCommandBuffer commandBuffer) {
    // --- [GUI] ---
    ImGui::NewFrame();

    if (m_isLoading && !m_wasLoading) {
        ImGui::OpenPopup("Loading Modal");
    }
    m_wasLoading = m_isLoading;

    ImGuiWindowFlags modalFlags =
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Loading Modal", NULL, modalFlags)) {
        ImGui::Text("Now Loading...");
        ImGui::Separator();

        if (!m_isLoading) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGuiWindowFlags infoFlags = ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoTitleBar |
                                 ImGuiWindowFlags_NoMove;

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::Begin("Viewer Info", nullptr, infoFlags);

    if (m_pointCloud) {
        ImGui::Text("%s", m_pointCloud->getFilePath());
        ImGui::Text("File Size: %.2f MB", m_pointCloud->getFileSize() / (1024.0f * 1024.0f));

        const auto& size = m_pointCloud->getTerrainSize();
        ImGui::Text("Terrain Size: %.1f x %.1f x %.1f", size.x, size.y, size.z);
    } else {
        ImGui::Text("None");
        ImGui::Text("File Size: 0 MB");
        ImGui::Text("Terrain Size: 0 x 0 x 0");
    }

    ImGui::Separator();
    
    const auto& pos = m_camera.m_position;
    ImGui::Text("Position: X: %.2f, Y: %.2f, Z: %.2f", pos.x, pos.y, pos.z);
    ImGui::SliderFloat("Point Size", &m_pointSize, 1.0f, 10.0f);

    ImGui::Separator();

    ImGui::RadioButton("Color Map", &m_viewMode, 0); ImGui::SameLine();
    ImGui::RadioButton("Height Map", &m_viewMode, 1);

    ImGui::Separator();
    ImGui::Checkbox("Show Debug View", &m_showDebugView);

    ImGui::End();

    if (m_pipelineLayout == VK_NULL_HANDLE)
        return;

    if (m_graphicsPipeline != VK_NULL_HANDLE) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
    }

    if (m_pointCloud) {
        UniformBufferData ubo{};
        ubo.pointSize = m_pointSize;
        const auto& center = m_pointCloud->getLocalOffset();
        const auto& size = m_pointCloud->getTerrainSize();
        ubo.minZ = center.z - size.z;
        ubo.maxZ = center.z + size.z;
        ubo.viewMode = m_viewMode;
        memcpy(m_uniformMapped, &ubo, sizeof(ubo));

        // Push Constants for Matrices
        glm::mat4 model = glm::translate(glm::mat4(1.0f), m_pointCloud->m_position) *
                          glm::mat4_cast(m_pointCloud->m_rotation) *
                          glm::scale(glm::mat4(1.0f), m_pointCloud->m_scale);

        // --- [Main Viewport] ---
        vkCmdSetViewport(commandBuffer, 0, 1, &m_viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &m_scissor);

        glm::vec3 forward = glm::normalize(m_camera.m_rotation * glm::vec3(0.0f, 0.0f, -1.0f));
        glm::vec3 up = glm::normalize(m_camera.m_rotation * glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 viewMain = glm::lookAt(m_camera.m_position, m_camera.m_position + forward, up);
        glm::mat4 projMain = glm::perspective(glm::radians(m_camera.m_fov), m_viewport.width / m_viewport.height, m_camera.m_near, m_camera.m_far);
        projMain[1][1] *= -1; // Vulkan Y is down
        
        glm::mat4 mvpMain = projMain * viewMain * model;
        Frustum mainFrustum(mvpMain);

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
        vkCmdPushConstants(commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mvpMain);
        m_pointCloud->draw(mainFrustum, m_camera.m_position, commandBuffer);

        // --- [Debug Viewport] ---
        if (m_showDebugView) {
            float pipW = m_viewport.width / 2.0f;
            float pipH = m_viewport.height / 2.0f;
            float pipX = m_viewport.width - pipW - 20.0f;
            float pipY = m_viewport.height - pipH - 20.0f;

            VkViewport pipViewport{pipX, pipY, pipW, pipH, 0.0f, 1.0f};
            VkRect2D pipScissor{{(int32_t)pipX, (int32_t)pipY}, {(uint32_t)pipW, (uint32_t)pipH}};

            VkClearAttachment clearDepth = {};
            clearDepth.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            clearDepth.clearValue.depthStencil = {1.0f, 0};
            VkClearRect clearRect = {};
            clearRect.rect = pipScissor;
            clearRect.baseArrayLayer = 0;
            clearRect.layerCount = 1;
            vkCmdClearAttachments(commandBuffer, 1, &clearDepth, 1, &clearRect);

            vkCmdSetViewport(commandBuffer, 0, 1, &pipViewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &pipScissor);
            
            float maxDim = std::max({size.x, size.y, size.z});
            float dist = maxDim;
            glm::mat4 viewPip = glm::lookAt(glm::vec3(dist, dist, dist), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 projPip = glm::perspective(glm::radians(60.0f), pipW / pipH, 0.1f, dist * 5.0f);
            projPip[1][1] *= -1.0f;

            glm::mat4 mvpPip = projPip * viewPip * model;

            vkCmdPushConstants(commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mvpPip);
            m_pointCloud->draw(mainFrustum, m_camera.m_position, commandBuffer);

            // --- [Octree Chunks] ---
            ImDrawList* drawList = ImGui::GetForegroundDrawList();
            auto projectToPip = [&](const glm::vec3& worldPos, ImVec2& outScreen) -> bool {
                glm::vec4 clip = mvpPip * glm::vec4(worldPos, 1.0f);
                if (clip.w < 0.1f) 
                    return false;

                glm::vec3 ndc = glm::vec3(clip) / clip.w;
                outScreen.x = pipX + (ndc.x * 0.5f + 0.5f) * pipW;
                outScreen.y = pipY + (ndc.y * 0.5f + 0.5f) * pipH;
                return true;
            };

            auto drawBox = [&](const Bound3D& b, ImU32 color) {
                glm::vec3 corners[8] = {
                    {b.min.x, b.min.y, b.min.z}, 
                    {b.max.x, b.min.y, b.min.z},
                    {b.max.x, b.max.y, b.min.z},
                    {b.min.x, b.max.y, b.min.z},
                    {b.min.x, b.min.y, b.max.z},
                    {b.max.x, b.min.y, b.max.z},
                    {b.max.x, b.max.y, b.max.z},
                    {b.min.x, b.max.y, b.max.z}
                };

                ImVec2 pts[8];
                for (int i = 0; i < 8; ++i) {
                    if (!projectToPip(corners[i], pts[i]))
                        return;
                }

                int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
                for (int i = 0; i < 12; ++i) {
                    drawList->AddLine(pts[edges[i][0]], pts[edges[i][1]], color, 1.0f);
                }
            };

            std::vector<Bound3D> allBounds;
            m_pointCloud->getAllBounds(allBounds);

            drawList->AddRectFilled(ImVec2(pipX, pipY), ImVec2(pipX + pipW, pipY + pipH), IM_COL32(0, 0, 0, 150));
            drawList->AddRect(ImVec2(pipX, pipY), ImVec2(pipX + pipW, pipY + pipH), IM_COL32(255, 255, 255, 255));

            for (const auto& bound : allBounds) {
                if (mainFrustum.intersects(bound))
                    drawBox(bound, IM_COL32(0, 255, 0, 200));
                else 
                    drawBox(bound, IM_COL32(255, 0, 0, 100));
            }
        }
    }
    ImGui::Render();
}

void MainScene::onResize(LONG width, LONG height) {
    if (width == 0 || height == 0) return;

    m_viewport.width = (float)width;
    m_viewport.height = (float)height;
    m_scissor.extent.width = (uint32_t)width;
    m_scissor.extent.height = (uint32_t)height;
}

void MainScene::onHandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (m_hWnd != hWnd)
        return;
    
    switch (uMsg) {
    case WM_KEYDOWN:
        if (wParam < 256)
            m_keys.set(wParam, true);
        break;
    case WM_KEYUP:
        if (wParam < 256)
            m_keys.set(wParam, false);
        break;
    case WM_RBUTTONDOWN:
        if (!m_rightMouseDown) {
            m_rightMouseDown = true;
            SetCapture(hWnd);

            GetCursorPos(&m_capturedMousePos);
            ShowCursor(FALSE);

            RECT rect;
            GetClientRect(hWnd, &rect);
            POINT center = {(rect.right - rect.left) / 2, (rect.bottom - rect.top) / 2};

            m_lastMousePos = glm::vec2((float)center.x, (float)center.y);
            ClientToScreen(hWnd, &center);
            SetCursorPos(center.x, center.y);
        }
        break;
    case WM_RBUTTONUP:
        if (m_rightMouseDown) {
            m_rightMouseDown = false;
            ReleaseCapture();

            SetCursorPos(m_capturedMousePos.x, m_capturedMousePos.y);
            ShowCursor(TRUE);

            POINT clientPos = m_capturedMousePos;
            ScreenToClient(hWnd, &clientPos);
            m_lastMousePos = glm::vec2((float)clientPos.x, (float)clientPos.y);
            m_mouseDelta = glm::vec2(0.0f);
        }
        break;
    case WM_MOUSEMOVE:
    {
        float xPos = (float)LOWORD(lParam);
        float yPos = (float)HIWORD(lParam);

        if (m_rightMouseDown) {
            float dx = xPos - m_lastMousePos.x;
            float dy = yPos - m_lastMousePos.y;

            if (dx != 0.0f || dy != 0.0f) {
                m_mouseDelta += glm::vec2(dx, dy);

                RECT rect;
                GetClientRect(hWnd, &rect);
                POINT center { (rect.right - rect.left) / 2, (rect.bottom - rect.top) / 2};

                m_lastMousePos = glm::vec2((float)center.x, (float)center.y);

                ClientToScreen(hWnd, &center);
                SetCursorPos(center.x, center.y);
            } 
        }
        break;
    }
    case WM_DROPFILES:
    {
        HDROP hDrop = (HDROP)wParam;
        UINT fileCount = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);

        if (fileCount > 0 && !m_isLoading) {
            WCHAR szPath[MAX_PATH];
            if (DragQueryFile(hDrop, 0, szPath, MAX_PATH)) {
                std::filesystem::path filePath(szPath);
                m_isLoading = true;

                m_loadingFuture = std::async(
                    std::launch::async,
                    [filePath,
                     device = m_device,
                     allocator = m_allocator,
                     transferMgr = m_transferMgr]() {
                        return std::make_unique<PointCloudObject>(
                            filePath, device, allocator, transferMgr
                        );
                    }
                );
            }
        }
        DragFinish(hDrop);
        break;
    }
    }
}
