#pragma once

class RenderContext;
class RenderQueue;
class PointCloudObject;
class PerspectiveCamera;
class TransferManager;
class PointCloudFileManager;
class PointCloudDataManager;
class ShaderLayout;
class Shader;
class Octree;
class Object;

//
// ================ Events ================
//

struct WindowResized { int width, height; };
struct KeyEvent { int key, scancode, action, mods; };
struct DragDropEvent { int count; const char** paths; };
struct MouseScrollEvent { double xoffset, yoffset; };
struct MouseButtonEvent { int button, action, mods; };
struct CursorPosEvent { double xpos, ypos; };

/// @brief 입력 및 시스템 이벤트를 하나로 묶어 처리하기 위한 variant 타입입니다.
using Event = std::variant<
    WindowResized,
    KeyEvent,
    DragDropEvent,
    MouseScrollEvent,
    MouseButtonEvent,
    CursorPosEvent
>;

//
// ================ Scene ================
//

/// @brief 애플리케이션에서 하나의 화면이나 상태(예: 메인 화면, 로딩 화면 등)를 나타내는 기반 클래스입니다.
class Scene {
public:
    Scene() = delete;
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    Scene(GLFWwindow* window, RenderContext* context) : m_window(window), m_context(context) {}
    virtual ~Scene() = default;

    virtual void onEnter() {}
    virtual void onExit() {}

    virtual bool onEvent(const Event& event) { return false; }

    virtual void update(float elapsedTimeSec) {}
    virtual void postUpdate(float elapsedTimeSec) {}

    virtual void onPreRender(VkCommandBuffer cmd) {}
    virtual void render(RenderQueue& queue) = 0;
    virtual void onPostRender(VkCommandBuffer cmd) {}

    virtual void onGUI() {}

protected:
    /// @brief 소유하지 않는 클래스 맴버 변수.
    GLFWwindow* m_window = nullptr;
    /// @brief 소유하지 않는 클래스 맴버 변수.
    RenderContext* m_context = nullptr;
};

//
// ================ MainScene ================
//

/// @brief 포인트 클라우드 뷰어의 주 렌더링 루프와 상호작용을 처리하는 메인 씬 클래스입니다.
class MainScene : public Scene {
public:
    MainScene() = delete;
    MainScene(const MainScene&) = delete;
    MainScene& operator=(const MainScene&) = delete;

    MainScene(GLFWwindow* window, RenderContext* context);
    virtual ~MainScene() = default;

    virtual void onEnter() override;

    virtual bool onEvent(const Event& event) override;

    virtual void update(float elapsedTimeSec) override;
    virtual void postUpdate(float elapsedTimeSec) override;
    virtual void render(RenderQueue& queue) override;

    virtual void onGUI() override;

private:
    void initPipeline();

private:
    bool m_wasLoading = false;
    bool m_showDebugView = false;
    std::atomic<bool> m_isLoading = false;
    std::future<void> m_loadingFuture;

    bool m_rightMouseDown = false;
    glm::vec2 m_lastMousePos{0.0f};
    glm::vec2 m_mouseDelta{0.0f};

    float m_pointSizeMultiplier = 100.0f;
    float m_pointSizeMin = 1.0f;
    float m_pointSizeMax = 10.0f;

    std::unique_ptr<TransferManager> m_transferManager;

    std::queue<std::unique_ptr<Object>> m_addedObjects;
    std::mutex m_sceneMutex;

    PerspectiveCamera* m_mainCamera = nullptr;
    std::vector<std::unique_ptr<Object>> m_allObjects;
    std::vector<Object*> m_rootObjects;

    template<typename T>
    using ResourceCache = std::unordered_map<std::string, std::unique_ptr<T>>;

    ResourceCache<Shader> m_shaders;
    ResourceCache<ShaderLayout> m_shaderLayouts;
};

