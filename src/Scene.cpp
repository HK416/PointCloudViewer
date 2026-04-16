#include "stdafx.h"
#include "Scene.h"
#include "Buffer.h"

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
    // Rotate
    if (m_rightMouseDown) {
        m_camera.m_yaw += m_mouseDelta.x * m_camera.m_lookSensitivity;
        m_camera.m_pitch -= m_mouseDelta.y * m_camera.m_lookSensitivity;
        m_camera.m_pitch = glm::clamp(m_camera.m_pitch, -89.0f, 89.0f);
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
        m_pointCloud->updateBufferState(m_transferMgr);
}

void MainScene::onDraw(VkCommandBuffer commandBuffer) {
    vkCmdSetViewport(commandBuffer, 0, 1, &m_viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &m_scissor);

    if (m_pipelineLayout == VK_NULL_HANDLE)
        return;

    if (m_graphicsPipeline != VK_NULL_HANDLE) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
    }

    if (m_pointCloud) {
        // Push Constants for Matrices
        glm::mat4 model = glm::translate(glm::mat4(1.0f), m_pointCloud->m_position) *
                          glm::mat4_cast(m_pointCloud->m_rotation) *
                          glm::scale(glm::mat4(1.0f), m_pointCloud->m_scale);

        glm::vec3 forward = glm::normalize(m_camera.m_rotation * glm::vec3(0.0f, 0.0f, -1.0f));
        glm::vec3 up = glm::normalize(m_camera.m_rotation * glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 viewMat = glm::lookAt(m_camera.m_position, m_camera.m_position + forward, up);

        glm::mat4 projMat = glm::perspective(glm::radians(m_camera.m_fov), m_viewport.width / m_viewport.height, m_camera.m_near, m_camera.m_far);
        projMat[1][1] *= -1; // Vulkan Y is down

        glm::mat4 mvp = projMat * viewMat * model;

        vkCmdPushConstants(commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mvp);

        m_pointCloud->draw(commandBuffer);
    }
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
        m_rightMouseDown = true;
        SetCapture(hWnd);
        break;
    case WM_RBUTTONUP:
        m_rightMouseDown = false;
        ReleaseCapture();
        break;
    case WM_MOUSEMOVE:
    {
        float xPos = (float)LOWORD(lParam);
        float yPos = (float)HIWORD(lParam);
        m_mouseDelta += glm::vec2(xPos - m_lastMousePos.x, yPos - m_lastMousePos.y);
        m_lastMousePos = glm::vec2(xPos, yPos);
        break;
    }
    case WM_DROPFILES:
    {
        HDROP hDrop = (HDROP)wParam;
        UINT fileCount = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
        if (fileCount > 0) {
            WCHAR szPath[MAX_PATH];
            if (DragQueryFile(hDrop, 0, szPath, MAX_PATH)) {
                // Async Load
                m_pointCloud = std::make_unique<PointCloudObject>(szPath, m_device, m_allocator, m_transferMgr);
            }
        }
        DragFinish(hDrop);
        break;
    }
    }
}
