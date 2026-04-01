#pragma once

void onMeshDestroyed(entt::registry& registry, entt::entity entity);

void onMaterialDestroyed(entt::registry& registry, entt::entity entity);

void onDragAndDropFile(entt::registry& registry, UINT uMsg, WPARAM wParam, LPARAM lParam);

void InputSystem(entt::registry& registry, UINT uMsg, WPARAM wParam, LPARAM lParam);

void CameraSystem(entt::registry& registry);

void PointRenderSystem(entt::registry& registry);

void PointPipelineSystem(entt::registry& registry);

void LasLoadSystem(entt::registry& registry);