#include "stdafx.h"
#include "Object.h"

PointCloudObject::PointCloudObject(
    LPCWSTR filepath, VkDevice device, VmaAllocator allocator
) {
    std::string path((char*)CW2A(filepath));

    pdal::Options options;
    options.add("filename", path);

    pdal::LasReader reader;
    reader.setOptions(options);

    pdal::PointTable table;
    reader.prepare(table);
    pdal::PointViewSet viewSet = reader.execute(table);
    pdal::PointViewPtr pointView = *viewSet.begin();

    uint32_t pointCount = (uint32_t)pointView->size();
    if (pointCount == 0) throw std::runtime_error("");

    m_allocator = allocator;
    m_vertexCount = pointCount;

    VkDeviceSize posSize = pointCount * sizeof(glm::vec3);
    VkDeviceSize colorSize = pointCount * sizeof(glm::vec3);

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = posSize;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    // Positions
    std::vector<glm::vec3> positions(pointCount);
    glm::vec3 min(FLT_MAX), max(-FLT_MAX);

    for (pdal::PointId i = 0; i < pointCount; ++i) {
        positions[i].x = pointView->getFieldAs<float>(pdal::Dimension::Id::X, i);
        positions[i].y = pointView->getFieldAs<float>(pdal::Dimension::Id::Y, i);
        positions[i].z = pointView->getFieldAs<float>(pdal::Dimension::Id::Z, i);

        min = glm::min(min, positions[i]);
        max = glm::max(max, positions[i]);
    }

    glm::vec3 center = (min + max) * 0.5f;
    for (auto& p : positions) {
        p -= center;
    }

    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &m_positionBuffer, &m_positionAllocation, nullptr);
    void* data;
    vmaMapMemory(allocator, m_positionAllocation, &data);
    memcpy(data, positions.data(), posSize);
    vmaUnmapMemory(allocator, m_positionAllocation);

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
    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &m_colorBuffer, &m_colorAllocation, nullptr);
    vmaMapMemory(allocator, m_colorAllocation, &data);
    memcpy(data, colors.data(), colorSize);
    vmaUnmapMemory(allocator, m_colorAllocation);
}

PointCloudObject::~PointCloudObject() {
    if (m_positionBuffer) {
        vmaDestroyBuffer(m_allocator, m_positionBuffer, m_positionAllocation);
    }

    if (m_colorBuffer) {
        vmaDestroyBuffer(m_allocator, m_colorBuffer, m_colorAllocation);
    }
}

int PointCloudObject::getVertexCount() const {
    return m_vertexCount;
}

std::vector<VkBuffer> PointCloudObject::getBuffers() const {
    return {m_positionBuffer, m_colorBuffer};
}
