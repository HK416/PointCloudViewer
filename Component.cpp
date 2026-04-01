#include "stdafx.h"
#include "Component.h"

void Mesh::onDestroy(VmaAllocator allocator) {
    if (m_positionBuffer) {
        vmaDestroyBuffer(allocator, m_positionBuffer, m_positionAllocation);
    }

    if (m_colorBuffer) {
        vmaDestroyBuffer(allocator, m_colorBuffer, m_colorAllocation);
    }

    if (m_intensityBuffer) {
        vmaDestroyBuffer(allocator, m_intensityBuffer, m_intensityAllocation);
    }
}

std::vector<VkBuffer> Mesh::getBuffers() const {
    return {m_positionBuffer, m_colorBuffer, m_intensityBuffer};
}

void StandardMaterial::onDestroy(VkDevice device) {
    if (m_graphicsPipeline) {
        vkDestroyPipeline(device, m_graphicsPipeline, VK_NULL_HANDLE);
        vkDestroyPipelineLayout(device, m_pipelineLayout, VK_NULL_HANDLE);
    }
}
