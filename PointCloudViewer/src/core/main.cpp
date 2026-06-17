#include "stdafx.h"
#include "Application.h"

constexpr uint32_t DEF_WIDTH = 1280;
constexpr uint32_t DEF_HEIGHT = 720;

void handleKeyInput(GLFWwindow* window, int key, int scancode, int action, int mods) {
    ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);

    void* ptr = glfwGetWindowUserPointer(window);
    Application* application = reinterpret_cast<Application*>(ptr);
    application->dispatchEvent(KeyEvent{key, scancode, action, mods});
}

void handleFileDrop(GLFWwindow* window, int count, const char** paths) {
    void* ptr = glfwGetWindowUserPointer(window);
    Application* application = reinterpret_cast<Application*>(ptr);
    application->dispatchEvent(DragDropEvent{count, paths});
}

void handleMouseScroll(GLFWwindow* window, double xoffset, double yoffset) {
    ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);

    void* ptr = glfwGetWindowUserPointer(window);
    Application* application = reinterpret_cast<Application*>(ptr);
    application->dispatchEvent(MouseScrollEvent{xoffset, yoffset});
}

void handleMouseButton(GLFWwindow* window, int button, int action, int mods) {
    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);

    if (ImGui::GetIO().WantCaptureMouse) {
        return;
    }

    void* ptr = glfwGetWindowUserPointer(window);
    Application* application = reinterpret_cast<Application*>(ptr);
    application->dispatchEvent(MouseButtonEvent{button, action, mods});
}

void handleCursorPosition(GLFWwindow* window, double xpos, double ypos) {
    ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);

    void* ptr = glfwGetWindowUserPointer(window);
    Application* application = reinterpret_cast<Application*>(ptr);
    application->dispatchEvent(CursorPosEvent{xpos, ypos});
}

void handleWindowResized(GLFWwindow* window, int width, int height) {
    void* ptr = glfwGetWindowUserPointer(window);
    Application* application = reinterpret_cast<Application*>(ptr);
    application->setFramebufferResized();
}

int main(int argc, char** argv) {
    bool initialized = false;
    GLFWwindow* window = nullptr;

    try {
        if (glfwInit() == GLFW_FALSE) {
            throw std::runtime_error("Failed to initialize GLFW!");
        }
        initialized = true;

        if (glfwVulkanSupported() == GLFW_FALSE) {
            throw std::runtime_error("Vulkan is not supported on this system!");
        }

        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_TRUE);

        window = glfwCreateWindow(DEF_WIDTH, DEF_HEIGHT, "Point Cloud Viewer", nullptr, nullptr);
        if (window == nullptr) {
            throw std::runtime_error("Failed to create GLFW window!");
        }

        auto application = std::make_unique<Application>(window);
        glfwSetWindowUserPointer(window, application.get());

        glfwSetKeyCallback(window, handleKeyInput);
        glfwSetDropCallback(window, handleFileDrop);
        glfwSetScrollCallback(window, handleMouseScroll);
        glfwSetMouseButtonCallback(window, handleMouseButton);
        glfwSetCursorPosCallback(window, handleCursorPosition);
        glfwSetFramebufferSizeCallback(window, handleWindowResized);

        float lastTime = static_cast<float>(glfwGetTime());
        float currentTime = lastTime;
        float elapsedTimeSec = 0.0f;

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            currentTime = static_cast<float>(glfwGetTime());
            elapsedTimeSec = currentTime - lastTime;
            lastTime = currentTime;

            if (elapsedTimeSec > 0.25f) {
                elapsedTimeSec = 0.25f;
            }

            application->drawFrame(elapsedTimeSec);
        }
    } catch (const std::exception& e) {
        spdlog::error(e.what());
        pfd::message("Error", e.what(), pfd::choice::ok, pfd::icon::error);
    }

    if (window) {
        glfwDestroyWindow(window);
    }

    if (initialized) {
        glfwTerminate();
    }

    return 0;
}
