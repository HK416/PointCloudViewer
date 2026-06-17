#include "stdafx.h"
#include "Scene.h"
#include "Renderer.h"
#include "Object.h"
#include "PointCloudManager.h"
#include "TransferManager.h"
#include "FileManager.h"
#include "Octree.h"
#include "Frustum.h"

namespace {
    std::vector<char> readShaderFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open shader file: " + filename);
        }
        size_t fileSize = (size_t)file.tellg();
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();
        return buffer;
    }

    VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shader module!");
        }
        return shaderModule;
    }
}

MainScene::MainScene(RenderContext* context) : Scene(context) {
    m_transferManager = std::make_unique<TransferManager>(context);
    m_camera = std::make_unique<PerspectiveCamera>();

    m_camera->getTransform().setPosition(glm::vec3(0.0f, 0.0f, 10.0f));
    initPipeline();
}

MainScene::~MainScene() {
    if (m_context && m_context->getDevice() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_context->getDevice());
        if (m_pipeline) vkDestroyPipeline(m_context->getDevice(), m_pipeline, nullptr);
        if (m_pipelineLayout) vkDestroyPipelineLayout(m_context->getDevice(), m_pipelineLayout, nullptr);
    }
}

void MainScene::onEnter() {}
void MainScene::onExit() {}

bool MainScene::onEvent(const Event& event) {
    if (const auto* resizeEvent = std::get_if<WindowResized>(&event)) {
        if (resizeEvent->height > 0) {
            m_camera->setAspectRatio(static_cast<float>(resizeEvent->width) / static_cast<float>(resizeEvent->height));
        }
        return true;
    }

    if (const auto* keyEvent = std::get_if<KeyEvent>(&event)) {
        if (keyEvent->key >= 0 && keyEvent->key < 1024) {
            m_keys[keyEvent->key] = (keyEvent->action != GLFW_RELEASE);
        }
        return true;
    }

    if (const auto* mouseBtn = std::get_if<MouseButtonEvent>(&event)) {
        if (mouseBtn->button == GLFW_MOUSE_BUTTON_RIGHT) {
            m_rightMouseDown = (mouseBtn->action == GLFW_PRESS);
        }
        return true;
    }

    if (const auto* cursorPos = std::get_if<CursorPosEvent>(&event)) {
        glm::vec2 pos(static_cast<float>(cursorPos->xpos), static_cast<float>(cursorPos->ypos));
        if (m_rightMouseDown) {
            glm::vec2 delta = pos - m_lastMousePos;
            float sensitivity = 0.003f;
            
            glm::quat yaw = glm::angleAxis(-delta.x * sensitivity, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::quat pitch = glm::angleAxis(-delta.y * sensitivity, glm::vec3(1.0f, 0.0f, 0.0f));
            
            glm::quat currentRot = m_camera->getTransform().getRotation();
            m_camera->getTransform().setRotation(yaw * currentRot * pitch);
        }
        m_lastMousePos = pos;
        return true;
    }

    if (const auto* scroll = std::get_if<MouseScrollEvent>(&event)) {
        glm::vec3 forward = m_camera->getTransform().getRotation() * glm::vec3(0.0f, 0.0f, -1.0f);
        m_camera->getTransform().setPosition(
            m_camera->getTransform().getPosition() + forward * static_cast<float>(scroll->yoffset) * 1.0f
        );
        return true;
    }

    if (const auto* drop = std::get_if<DragDropEvent>(&event)) {
        if (drop->count > 0 && !m_isLoading) {
            m_isLoading = true;
            std::string filePath = drop->paths[0];

            m_loadingFuture = std::async(std::launch::async, [this, filePath]() {
                try {
                    std::string tempBinPath = filePath + "_temp.bin";
                    auto fileManager = std::make_unique<PointCloudFileManager>(tempBinPath);

                    Bound3D bound;
                    // 임시 바운딩 박스 설정 (Octree 생성 시 엄격한 크기 정보가 필요하므로 헤더 파싱 후 다시 생성됨)
                    auto octree = std::make_unique<Octree>(fileManager.get(), Bound3D{glm::vec3(-1000.0f), glm::vec3(1000.0f)});
                    
                    std::string ext = std::filesystem::path(filePath).extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                    if (ext == ".las" || ext == ".laz") {
                        spdlog::info("[Parser] Detected LAS/LAZ format. Opening via laszip...");
                        laszip_POINTER reader;
                        if (laszip_create(&reader)) throw std::runtime_error("Failed to create laszip reader!");
                        
                        laszip_BOOL is_compressed;
                        if (laszip_open_reader(reader, filePath.c_str(), &is_compressed)) {
                            laszip_destroy(reader);
                            throw std::runtime_error("Failed to open LAS/LAZ file!");
                        }
                        
                        laszip_header* header;
                        laszip_get_header_pointer(reader, &header);

                        bound.min = glm::vec3(header->min_x, header->min_y, header->min_z);
                        bound.max = glm::vec3(header->max_x, header->max_y, header->max_z);
                        octree = std::make_unique<Octree>(fileManager.get(), bound);

                        uint64_t pointCount = header->number_of_point_records;
                        if (header->extended_number_of_point_records > 0) {
                            pointCount = header->extended_number_of_point_records;
                        }

                        spdlog::info("[Parser] Header loaded. Total points to read: {}", pointCount);

                        laszip_point* point;
                        laszip_get_point_pointer(reader, &point);

                        for (uint64_t i = 0; i < pointCount; ++i) {
                            laszip_read_point(reader);
                            glm::vec3 pos;
                            pos.x = static_cast<float>(header->x_offset + point->X * header->x_scale_factor);
                            pos.y = static_cast<float>(header->y_offset + point->Y * header->y_scale_factor);
                            pos.z = static_cast<float>(header->z_offset + point->Z * header->z_scale_factor);

                            glm::vec3 color{1.0f};
                            color.r = point->rgb[0] / 65535.0f;
                            color.g = point->rgb[1] / 65535.0f;
                            color.b = point->rgb[2] / 65535.0f;

                            octree->insert({pos, 0, color, 0});

                            if (i % 1000000 == 0 && i > 0) {
                                spdlog::info("[Parser] Processed {} million points...", i / 1000000);
                            }
                        }
                        
                        spdlog::info("[Parser] Finished reading LAS/LAZ points.");
                        laszip_close_reader(reader);
                        laszip_destroy(reader);
                    } else if (ext == ".ply") {
                        std::ifstream ss(filePath, std::ios::binary);
                        if (ss.fail()) throw std::runtime_error("Failed to open PLY file!");

                        tinyply::PlyFile plyFile;
                        plyFile.parse_header(ss);

                        std::shared_ptr<tinyply::PlyData> vertices, colors;
                        try { vertices = plyFile.request_properties_from_element("vertex", { "x", "y", "z" }); } catch (...) {}
                        try { colors = plyFile.request_properties_from_element("vertex", { "red", "green", "blue" }); } catch (...) {}

                        plyFile.read(ss);

                        if (vertices) {
                            const size_t numVertices = vertices->count;
                            const float* vPtr = reinterpret_cast<const float*>(vertices->buffer.get());
                            const uint8_t* cPtr = colors ? reinterpret_cast<const uint8_t*>(colors->buffer.get()) : nullptr;

                            glm::vec3 min(std::numeric_limits<float>::max());
                            glm::vec3 max(std::numeric_limits<float>::lowest());
                            
                            for (size_t i = 0; i < numVertices; ++i) {
                                glm::vec3 pos(vPtr[i*3], vPtr[i*3+1], vPtr[i*3+2]);
                                min = glm::min(min, pos);
                                max = glm::max(max, pos);
                            }
                            
                            bound.min = min;
                            bound.max = max;
                            octree = std::make_unique<Octree>(fileManager.get(), bound);

                            for (size_t i = 0; i < numVertices; ++i) {
                                glm::vec3 pos(vPtr[i*3], vPtr[i*3+1], vPtr[i*3+2]);
                                glm::vec3 color(1.0f);
                                if (cPtr) {
                                    color.r = cPtr[i*3] / 255.0f;
                                    color.g = cPtr[i*3+1] / 255.0f;
                                    color.b = cPtr[i*3+2] / 255.0f;
                                }
                                octree->insert({pos, 0, color, 0});
                            }
                        }
                    } else {
                        throw std::runtime_error("Unsupported file format!");
                    }

                    spdlog::info("[Octree] Flushing remaining chunks to disk...");
                    octree->flushRemainingToDisk();
                    spdlog::info("[Octree] Build complete!");

                    // 256개 노드 용량 설정 (약 2GB VRAM)
                    auto pointCloudManager = std::make_unique<GlobalPointCloudManager>(
                        m_context, m_transferManager.get(), fileManager.get(), 256
                    );

                    // 로드된 모델이 보이도록 카메라 이동
                    glm::vec3 center = bound.getCenter();
                    glm::vec3 size = bound.getSize();
                    float maxDim = std::max({size.x, size.y, size.z});
                    
                    spdlog::info("[Camera] Focusing on model. Center: ({:.2f}, {:.2f}, {:.2f}), Size: {:.2f}", center.x, center.y, center.z, maxDim);
                    m_camera->getTransform().setPosition(center + glm::vec3(0.0f, 0.0f, maxDim * 1.0f));
                    m_camera->getTransform().setRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

                    auto pc = std::make_shared<PointCloudObject>(
                        m_context, std::move(fileManager), std::move(octree), std::move(pointCloudManager),
                        m_pipeline, m_pipelineLayout
                    );

                    {
                        std::lock_guard<std::mutex> lock(m_sceneMutex);
                        m_pointClouds.push_back(pc);
                    }

                    m_isLoading = false;
                    spdlog::info("[System] Loading task finished successfully.");
                }
                catch (const std::exception& e) {
                    spdlog::error("[System] Loading failed with exception: {}", e.what());
                    m_isLoading = false;
                }
            });
        }
        return true;
    }

    return false;
}

void MainScene::update(float elapsedTimeSec) {
    m_camera->update(elapsedTimeSec);

    // 카메라 이동 속도 설정
    float moveSpeed = 10.0f * elapsedTimeSec;

    glm::vec3 forward = m_camera->getTransform().getRotation() * glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 right = m_camera->getTransform().getRotation() * glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f); // 월드 기준 위쪽 방향

    glm::vec3 pos = m_camera->getTransform().getPosition();

    // WASD 이동
    if (m_keys[GLFW_KEY_W]) pos += forward * moveSpeed;
    if (m_keys[GLFW_KEY_S]) pos -= forward * moveSpeed;
    if (m_keys[GLFW_KEY_A]) pos -= right * moveSpeed;
    if (m_keys[GLFW_KEY_D]) pos += right * moveSpeed;

    // Space/Shift 수직 이동
    if (m_keys[GLFW_KEY_SPACE]) pos += up * moveSpeed;
    if (m_keys[GLFW_KEY_LEFT_SHIFT] || m_keys[GLFW_KEY_RIGHT_SHIFT]) pos -= up * moveSpeed;

    m_camera->getTransform().setPosition(pos);

    std::lock_guard<std::mutex> lock(m_sceneMutex);
    for (auto& pc : m_pointClouds) {
        if (pc) pc->update(elapsedTimeSec);
    }
}

void MainScene::onPreRender(VkCommandBuffer cmd) {
}

void MainScene::render(RenderQueue& queue) {
    m_camera->applyToQueue(queue);

    std::lock_guard<std::mutex> lock(m_sceneMutex);
    for (auto& pc : m_pointClouds) {
        if (pc) pc->render(queue);
    }
}

void MainScene::onPostRender(VkCommandBuffer cmd) {
}

void MainScene::onGUI() {
    // 화면 상단에 카메라 정보 모달 창 표시
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    const float PAD = 10.0f;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 work_pos = viewport->WorkPos;
    ImVec2 window_pos = ImVec2(work_pos.x + viewport->WorkSize.x / 2.0f, work_pos.y + PAD);
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.65f);

    if (ImGui::Begin("Camera Info", nullptr, window_flags)) {
        glm::vec3 pos = m_camera->getTransform().getPosition();
        glm::vec3 euler = glm::degrees(glm::eulerAngles(m_camera->getTransform().getRotation()));

        ImGui::Text("Camera Position: X: %.2f, Y: %.2f, Z: %.2f", pos.x, pos.y, pos.z);
        ImGui::Text("Camera Rotation: Pitch: %.2f, Yaw: %.2f, Roll: %.2f", euler.x, euler.y, euler.z);
    }
    ImGui::End();
}

void MainScene::initPipeline() {
    VkDevice device = m_context->getDevice();

    std::string vertPath = "PointCloudViewer/shaders/point.vert.spv";
    if (!std::filesystem::exists(vertPath)) vertPath = "shaders/point.vert.spv";

    std::string fragPath = "PointCloudViewer/shaders/point.frag.spv";
    if (!std::filesystem::exists(fragPath)) fragPath = "shaders/point.frag.spv";

    auto vertShaderCode = readShaderFile(vertPath);
    auto fragShaderCode = readShaderFile(fragPath);

    VkShaderModule vertShaderModule = createShaderModule(device, vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(device, fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = GlobalPointCloudManager::vertexStride;
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::vector<VkVertexInputAttributeDescription> attributeDescriptions(2);
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(PointCloudVertex, position);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(PointCloudVertex, color);

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPushConstantRange pushConstant{};
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstant.offset = 0;
    pushConstant.size = sizeof(glm::mat4);

    VkDescriptorSetLayout globalLayout = m_context->getGlobalDescriptorSetLayout();

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &globalLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstant;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout!");
    }

    VkPipelineRenderingCreateInfo renderingCreateInfo{};
    renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingCreateInfo.colorAttachmentCount = 1;
    renderingCreateInfo.pColorAttachmentFormats = &RenderSwapchain::swapchainImageFormat;
    renderingCreateInfo.depthAttachmentFormat = RenderSwapchain::depthImageFormat;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &renderingCreateInfo;
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

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
}
