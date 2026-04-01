#include "stdafx.h"
#include "Resource.h"
#include "Component.h"
#include "System.h"

void onMeshDestroyed(entt::registry& registry, entt::entity entity) {
    auto& vkCtx = registry.ctx().get<RenderContext>();
    VmaAllocator allocator = vkCtx.m_allocator;

    auto& mesh = registry.get<Mesh>(entity);
    mesh.onDestroy(allocator);
}

void onMaterialDestroyed(entt::registry& registry, entt::entity entity) {
    auto& vkCtx = registry.ctx().get<RenderContext>();
    VkDevice device = vkCtx.m_device;

    auto& material = registry.get<StandardMaterial>(entity);
    material.onDestroy(device);
}

void onDragAndDropFile(
    entt::registry& registry, UINT uMsg, WPARAM wParam, LPARAM lParam
) {
    if (uMsg == WM_DROPFILES) {
        HDROP hDrop = (HDROP)wParam;
        UINT fileCount = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
        for (UINT i = 0; i < fileCount; i++) {
            WCHAR szPath[MAX_PATH];
            if (DragQueryFile(hDrop, i, szPath, MAX_PATH)) {
                auto entity = registry.create();
                registry.emplace<LasLoadRequest>(entity, szPath);
            }
        }
        DragFinish(hDrop);
    }
}

void InputSystem(
    entt::registry& registry, UINT uMsg, WPARAM wParam, LPARAM lParam
) {
    auto& input = registry.ctx().get<InputState>();

    switch (uMsg) {
    case WM_KEYDOWN:
        if (wParam < 256) input.m_keys[wParam] = true;
        break;
    case WM_KEYUP:
        if (wParam < 256) input.m_keys[wParam] = false;
        break;
    case WM_RBUTTONDOWN:
        input.m_rightMouseDown = true;
        SetCapture(WindowFromDC(GetDC(NULL))); // Simple capture for demo
        break;
    case WM_RBUTTONUP:
        input.m_rightMouseDown = false;
        ReleaseCapture();
        break;
    case WM_MOUSEMOVE:
    {
        float xPos = (float)LOWORD(lParam);
        float yPos = (float)HIWORD(lParam);
        input.m_mouseDelta = glm::vec2(xPos - input.m_lastMousePos.x, yPos - input.m_lastMousePos.y);
        input.m_lastMousePos = glm::vec2(xPos, yPos);
        break;
    }
    }
}

void CameraSystem(entt::registry& registry) {
    auto& input = registry.ctx().get<InputState>();
    auto& time = registry.ctx().get<Time>();
    
    auto view = registry.view<Camera, Transform>();
    for (auto entity : view) {
        auto& cam = view.get<Camera>(entity);
        auto& trans = view.get<Transform>(entity);

        // Rotate
        if (input.m_rightMouseDown) {
            cam.m_yaw += input.m_mouseDelta.x * cam.m_lookSensitivity;
            cam.m_pitch -= input.m_mouseDelta.y * cam.m_lookSensitivity;
            cam.m_pitch = glm::clamp(cam.m_pitch, -89.0f, 89.0f);
        }
        input.m_mouseDelta = glm::vec2(0.0f); // Reset delta after use

        glm::vec3 front;
        front.x = cos(glm::radians(cam.m_yaw)) * cos(glm::radians(cam.m_pitch));
        front.y = sin(glm::radians(cam.m_pitch));
        front.z = sin(glm::radians(cam.m_yaw)) * cos(glm::radians(cam.m_pitch));
        glm::vec3 forward = glm::normalize(front);
        glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
        glm::vec3 up = glm::normalize(glm::cross(right, forward));

        // Move
        float speed = cam.m_moveSpeed * time.m_deltaTime;
        if (input.m_keys['W']) trans.m_position += forward * speed;
        if (input.m_keys['S']) trans.m_position -= forward * speed;
        if (input.m_keys['A']) trans.m_position -= right * speed;
        if (input.m_keys['D']) trans.m_position += right * speed;
        if (input.m_keys['E']) trans.m_position += up * speed;
        if (input.m_keys['Q']) trans.m_position -= up * speed;

        // View Matrix
        glm::mat4 viewMat = glm::lookAt(trans.m_position, trans.m_position + forward, up);
        
        // Projection Matrix (Assuming 16:9 for simplicity, should get from swapchain extent)
        glm::mat4 projMat = glm::perspective(glm::radians(cam.m_fov), 1280.0f / 720.0f, cam.m_near, cam.m_far);
        projMat[1][1] *= -1; // Vulkan Y is down

        cam.m_viewProjMatrix = projMat * viewMat;
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
    VkShaderModuleCreateInfo createInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    createInfo.codeSize = code.size() * sizeof(uint32_t);
    createInfo.pCode = code.data();
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module!");
    }
    return shaderModule;
}

void PointPipelineSystem(entt::registry& registry) {
    auto& vkCtx = registry.ctx().get<RenderContext>();

    // 1. Load SPIR-V Shaders from files
    auto vertShaderCode = readFile("vert.spv");
    auto fragShaderCode = readFile("frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(vkCtx.m_device, vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(vkCtx.m_device, fragShaderCode);

    VkPipelineShaderStageCreateInfo shaderStages[] = {
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertShaderModule, "main" },
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragShaderModule, "main" }
    };

    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(glm::mat4);

    VkPipelineLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstantRange;

    StandardMaterial mat;
    if (vkCreatePipelineLayout(vkCtx.m_device, &layoutInfo, nullptr, &mat.m_pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout!");
    }

    // Pipeline State
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    VkVertexInputBindingDescription bindings[] = {
        { 0, sizeof(glm::vec3), VK_VERTEX_INPUT_RATE_VERTEX }, // Pos
        { 1, sizeof(glm::vec3), VK_VERTEX_INPUT_RATE_VERTEX }  // Color
    };
    VkVertexInputAttributeDescription attributes[] = {
        { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },
        { 1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0 }
    };
    vertexInputInfo.vertexBindingDescriptionCount = 2;
    vertexInputInfo.pVertexBindingDescriptions = bindings;
    vertexInputInfo.vertexAttributeDescriptionCount = 2;
    vertexInputInfo.pVertexAttributeDescriptions = attributes;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

    VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;

    VkPipelineMultisampleStateCreateInfo multisampling = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo colorBlending = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    // Dynamic Rendering Info (Vulkan 1.3)
    VkPipelineRenderingCreateInfo pipelineRenderingInfo = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    VkFormat colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
    pipelineRenderingInfo.colorAttachmentCount = 1;
    pipelineRenderingInfo.pColorAttachmentFormats = &colorFormat;
    pipelineRenderingInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

    VkGraphicsPipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
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
    pipelineInfo.layout = mat.m_pipelineLayout;

    if (vkCreateGraphicsPipelines(vkCtx.m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mat.m_graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline!");
    }

    // Cleanup shader modules
    vkDestroyShaderModule(vkCtx.m_device, vertShaderModule, nullptr);
    vkDestroyShaderModule(vkCtx.m_device, fragShaderModule, nullptr);
    
    auto pipelineEntity = registry.create();
    registry.emplace<StandardMaterial>(pipelineEntity, mat);
    registry.ctx().emplace<entt::entity>(pipelineEntity); // Register as default material entity
}

void PointRenderSystem(entt::registry& registry) {
    auto& vkCtx = registry.ctx().get<RenderContext>();
    VkCommandBuffer cmd = vkCtx.m_commandBuffer;

    auto camView = registry.view<Camera>();
    if (camView.empty()) return;
    auto& cam = camView.get<Camera>(camView.front());

    // Set Viewport/Scissor dynamically
    VkViewport viewport = { 0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f };
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor = { {0, 0}, {1280, 720} };
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    auto meshView = registry.view<Mesh, Transform>();
    auto materialEntity = registry.ctx().get<entt::entity>();
    if (!registry.valid(materialEntity)) return;
    auto& mat = registry.get<StandardMaterial>(materialEntity);

    if (mat.m_pipelineLayout == VK_NULL_HANDLE) return;

    if (mat.m_graphicsPipeline != VK_NULL_HANDLE) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mat.m_graphicsPipeline);
    }

    for (auto entity : meshView) {
        auto& mesh = meshView.get<Mesh>(entity);
        auto& trans = meshView.get<Transform>(entity);

        // Push Constants for Matrices
        glm::mat4 model = glm::translate(glm::mat4(1.0f), trans.m_position) * 
                          glm::mat4_cast(trans.m_rotation) * 
                          glm::scale(glm::mat4(1.0f), trans.m_scale);
        glm::mat4 mvp = cam.m_viewProjMatrix * model;
        
        vkCmdPushConstants(cmd, mat.m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mvp);

        VkBuffer vertexBuffers[] = { mesh.m_positionBuffer, mesh.m_colorBuffer };
        VkDeviceSize offsets[] = { 0, 0 };
        vkCmdBindVertexBuffers(cmd, 0, 2, vertexBuffers, offsets);

        vkCmdDraw(cmd, mesh.m_vertexCount, 1, 0, 0);
    }
}

void LasLoadSystem(entt::registry& registry) {
    auto view = registry.view<LasLoadRequest>();
    if (view.empty()) return;

    auto& vkCtx = registry.ctx().get<RenderContext>();

    for (auto entity : view) {
        auto& request = view.get<LasLoadRequest>(entity);
        
        try {
            std::string path((char*)CW2A(request.m_path.c_str()));
            
            pdal::Options options;
            options.add("filename", path);

            pdal::LasReader reader;
            reader.setOptions(options);

            pdal::PointTable table;
            reader.prepare(table);
            pdal::PointViewSet viewSet = reader.execute(table);
            pdal::PointViewPtr pointView = *viewSet.begin();

            uint32_t pointCount = (uint32_t)pointView->size();
            if (pointCount == 0) continue;

            // Create Buffers via VMA
            Mesh mesh;
            mesh.m_vertexCount = pointCount;

            VkDeviceSize posSize = pointCount * sizeof(glm::vec3);
            VkDeviceSize colorSize = pointCount * sizeof(glm::vec3); // Assuming RGB

            VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
            bufferInfo.size = posSize;
            bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            
            VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU; // For simplicity in this example

            // Positions
            std::vector<glm::vec3> positions(pointCount);
            for (pdal::PointId i = 0; i < pointCount; ++i) {
                positions[i].x = pointView->getFieldAs<float>(pdal::Dimension::Id::X, i);
                positions[i].y = pointView->getFieldAs<float>(pdal::Dimension::Id::Y, i);
                positions[i].z = pointView->getFieldAs<float>(pdal::Dimension::Id::Z, i);
            }

            vmaCreateBuffer(vkCtx.m_allocator, &bufferInfo, &allocInfo, &mesh.m_positionBuffer, &mesh.m_positionAllocation, nullptr);
            void* data;
            vmaMapMemory(vkCtx.m_allocator, mesh.m_positionAllocation, &data);
            memcpy(data, positions.data(), posSize);
            vmaUnmapMemory(vkCtx.m_allocator, mesh.m_positionAllocation);

            // Colors (Optional, LAS might have 16-bit colors)
            std::vector<glm::vec3> colors(pointCount, glm::vec3(1.0f));
            if (pointView->hasDim(pdal::Dimension::Id::Red)) {
                for (pdal::PointId i = 0; i < pointCount; ++i) {
                    colors[i].r = pointView->getFieldAs<float>(pdal::Dimension::Id::Red, i) / 65535.0f;
                    colors[i].g = pointView->getFieldAs<float>(pdal::Dimension::Id::Green, i) / 65535.0f;
                    colors[i].b = pointView->getFieldAs<float>(pdal::Dimension::Id::Blue, i) / 65535.0f;
                }
            }
            
            bufferInfo.size = colorSize;
            vmaCreateBuffer(vkCtx.m_allocator, &bufferInfo, &allocInfo, &mesh.m_colorBuffer, &mesh.m_colorAllocation, nullptr);
            vmaMapMemory(vkCtx.m_allocator, mesh.m_colorAllocation, &data);
            memcpy(data, colors.data(), colorSize);
            vmaUnmapMemory(vkCtx.m_allocator, mesh.m_colorAllocation);

            registry.emplace<Mesh>(entity, mesh);
            registry.emplace<Transform>(entity);
            registry.remove<LasLoadRequest>(entity);

            std::cout << "Loaded LAS: " << path << " (" << pointCount << " points)" << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "Failed to load LAS: " << e.what() << std::endl;
            registry.destroy(entity);
        }
    }
}
