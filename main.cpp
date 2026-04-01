#include "stdafx.h"
#include "Application.h"
#include "Component.h"
#include "Resource.h"
#include "System.h"

int main(int argc, char** argv) {
    try {
        Application app(L"Point Cloud Viewer", 1280, 720);
        
        app.insertResource<InputState>();

        app.addStartupSystem(PointPipelineSystem)
            .addStartupSystem([](entt::registry& registry) {
            auto entity = registry.create();
            registry.emplace<Camera>(entity);
            registry.emplace<Transform>(
                entity,
                glm::vec3(1.0f),
                glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                glm::vec3(0.0f, 0.0f, 50.0f)
            );
        });

        app.addInputSystem(InputSystem)
            .addInputSystem(onDragAndDropFile)
            .addUpdateSystem(LasLoadSystem)
            .addUpdateSystem(CameraSystem)
            .addRenderSystem(PointRenderSystem);

        app.run();
    }
    catch (const std::exception& e) {
        ATL::CA2T msg(e.what());
        MessageBox(NULL, msg, L"Error", MB_ICONERROR);
        return -1;
    }

    return 0;
}
