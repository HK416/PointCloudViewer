#include "stdafx.h"
#include "Scene.h"
#include "Renderer.h"
#include "Object.h"
#include "PointCloudManager.h"
#include "TransferManager.h"
#include "FileManager.h"
#include "Octree.h"
#include "Frustum.h"
#include "Shader.h"
#include "ShaderLayout.h"

//
// ================ MainScene ================
//

MainScene::MainScene(GLFWwindow* window, RenderContext* context) : Scene(window, context) {
    m_transferManager = std::make_unique<TransferManager>(context);

    // Create main camera
    auto camera = std::make_unique<PerspectiveCamera>(m_window);
    camera->getTransform().setPosition({0.0f, 0.0f, 0.0f});

    m_mainCamera = camera.get();
    m_rootObjects.push_back(camera.get());
    m_allObjects.push_back(std::move(camera));
}

void MainScene::onEnter() {
    // Create shader layout
    auto pcLayout = ShaderLayoutBuilder()
            .addDescriptorSetLayout(m_context->getGlobalDescriptorSetLayout())
            .addPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4))
            .build(m_context);
    m_shaderLayouts["PointCloud"] = std::move(pcLayout);

    // Create point cloud shader
    auto pcShader = std::make_unique<PointCloudShader>(m_context, m_shaderLayouts["PointCloud"].get());
    m_shaders["PointCloud"] = std::move(pcShader);
}

bool MainScene::onEvent(const Event& event) {
    if (const auto* resizeEvent = std::get_if<WindowResized>(&event)) {
        if (resizeEvent->height > 0) {
            m_mainCamera->setAspectRatio(static_cast<float>(resizeEvent->width) / static_cast<float>(resizeEvent->height));
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
            
            glm::vec3 euler = m_mainCamera->getTransform().getEulerAngles();
            
            float pitchAngle = euler.x - delta.y * sensitivity * 100.0f;
            float rollAngle  = euler.z - delta.x * sensitivity * 100.0f;
            
            m_mainCamera->getTransform().setRotationEuler(glm::vec3(pitchAngle, 0.0f, rollAngle));
        }
        m_lastMousePos = pos;
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
                    std::unique_ptr<Octree> octree;
                    
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
                    auto pointCloudManager = std::make_unique<PointCloudDataManager>(
                        m_context, m_transferManager.get(), fileManager.get(), 256
                    );

                    // 로드된 모델이 보이도록 카메라 이동
                    glm::vec3 center = bound.getCenter();
                    glm::vec3 size = bound.getSize();
                    float maxDim = std::max({size.x, size.y, size.z});
                    
                    spdlog::info("[Camera] Focusing on model. Center: ({:.2f}, {:.2f}, {:.2f}), Size: {:.2f}", center.x, center.y, center.z, maxDim);
                    m_mainCamera->getTransform().setPosition(center + glm::vec3(0.0f, 0.0f, maxDim * 1.0f));
                    m_mainCamera->getTransform().setRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

                    auto pc = std::make_unique<PointCloudObject>(
                        m_shaders["PointCloud"].get(),
                        std::move(fileManager),
                        std::move(octree),
                        std::move(pointCloudManager)
                    );

                    {
                        std::lock_guard<std::mutex> lock(m_sceneMutex);
                        m_addedObjects.push(std::move(pc));
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
    for (Object* rootObj : m_rootObjects) {
        if (rootObj && !rootObj->isPendingDestroy()) {
            rootObj->update(elapsedTimeSec);
        }
    }
}

void MainScene::postUpdate(float elapsedTimeSec) {
    for (Object* rootObj : m_rootObjects) {
        if (rootObj && !rootObj->isPendingDestroy()) {
            rootObj->lateUpdate(elapsedTimeSec);
        }
    }

    // remove destroyed objects
    m_rootObjects.erase(
        std::remove_if(
            m_rootObjects.begin(),
            m_rootObjects.end(),
            [](Object* obj) {
                return obj == nullptr || obj->isPendingDestroy();
            }
        ),
        m_rootObjects.end()
    );
    
    m_allObjects.erase(
        std::remove_if(
            m_allObjects.begin(),
            m_allObjects.end(),
            [](const auto& obj) {
                return obj == nullptr || obj->isPendingDestroy();
            }
        ),
        m_allObjects.end()
    );

    // add new objects
    std::lock_guard<std::mutex> lock(m_sceneMutex);
    while (!m_addedObjects.empty()) {
        auto obj = std::move(m_addedObjects.front());
        m_addedObjects.pop();

        m_rootObjects.push_back(obj.get());
        m_allObjects.push_back(std::move(obj));
    }
}

void MainScene::render(RenderQueue& queue) {
    if (m_mainCamera) {
        m_mainCamera->applyToQueue(queue);
    }
    queue.setPointSizeParams(m_pointSizeMultiplier, m_pointSizeMin, m_pointSizeMax);

    for (Object* obj : m_rootObjects) {
        if (obj) {
            obj->render(queue);
        }
    }
}

void MainScene::onGUI() {
    // 화면 상단에 카메라 정보 모달 창 표시
    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav;
    const float PAD = 10.0f;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 work_pos = viewport->WorkPos;
    ImVec2 window_pos = ImVec2(work_pos.x + viewport->WorkSize.x / 2.0f, work_pos.y + PAD);
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.65f);

    if (m_mainCamera && ImGui::Begin("Camera Info", nullptr, window_flags)) {
        glm::vec3 pos = m_mainCamera->getTransform().getPosition();
        glm::vec3 euler = m_mainCamera->getTransform().getEulerAngles();

        ImGui::Text("Camera Position: X: %.2f, Y: %.2f, Z: %.2f", pos.x, pos.y, pos.z);
        ImGui::Text("Camera Rotation: Pitch: %.2f, Yaw: %.2f, Roll: %.2f", euler.x, euler.y, euler.z);

        ImGui::Separator();
        float speed = m_mainCamera->getMoveSpeed();
        if (ImGui::SliderFloat("Move Speed", &speed, 1.0f, 5000.0f)) {
            m_mainCamera->setMoveSpeed(speed);
        }

        ImGui::Separator();
        ImGui::Text("Point Cloud Settings:");
        ImGui::SliderFloat("Size Multiplier", &m_pointSizeMultiplier, 10.0f, 2000.0f);
        ImGui::SliderFloat("Min Point Size", &m_pointSizeMin, 1.0f, 5.0f);
        ImGui::SliderFloat("Max Point Size", &m_pointSizeMax, 1.0f, 50.0f);
    }
    ImGui::End();
}
