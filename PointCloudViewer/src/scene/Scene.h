#pragma once
#include <bitset>

class RenderContext;
class RenderQueue;
class PointCloudObject;
class PerspectiveCamera;
class TransferManager;
class PointCloudFileManager;
class GlobalPointCloudManager;
class Octree;

struct WindowResized { int width, height; };
struct KeyEvent { int key, scancode, action, mods; };
struct DragDropEvent { int count; const char** paths; };
struct MouseScrollEvent { double xoffset, yoffset; };
struct MouseButtonEvent { int button, action, mods; };
struct CursorPosEvent { double xpos, ypos; };

using Event = std::variant<
    WindowResized,
    KeyEvent,
    DragDropEvent,
    MouseScrollEvent,
    MouseButtonEvent,
    CursorPosEvent
>;

class Scene {
public:
    Scene() = delete;
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    Scene(RenderContext* context) : m_context(context) {}
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
    RenderContext* m_context = nullptr;
};

class MainScene : public Scene {
public:
    MainScene() = delete;
    MainScene(const MainScene&) = delete;
    MainScene& operator=(const MainScene&) = delete;

    MainScene(RenderContext* context);
    virtual ~MainScene();

    virtual void onEnter() override;
    virtual void onExit() override;

    virtual bool onEvent(const Event& event) override;

    virtual void update(float elapsedTimeSec) override;
    virtual void render(RenderQueue& queue) override;

    virtual void onGUI() override;
    virtual void onPreRender(VkCommandBuffer cmd) override;
    virtual void onPostRender(VkCommandBuffer cmd) override;

private:
    void initPipeline();

private:
    bool m_wasLoading = false;
    bool m_showDebugView = false;
    std::atomic<bool> m_isLoading = false;
    std::future<void> m_loadingFuture;
    std::mutex m_sceneMutex;

    std::unique_ptr<TransferManager> m_transferManager;
    
    std::vector<std::shared_ptr<PointCloudObject>> m_pointClouds;

    std::unique_ptr<PerspectiveCamera> m_camera;

    bool m_rightMouseDown = false;
    glm::vec2 m_lastMousePos{0.0f};
    glm::vec2 m_mouseDelta{0.0f};
    std::bitset<1024> m_keys;

    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
};

