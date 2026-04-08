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

    pdal::QuickInfo info = reader.preview();
    uint32_t pointCount = (uint32_t)info.m_pointCount;
    if (pointCount == 0) throw std::runtime_error("");
    
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> colors;
    positions.reserve(pointCount);
    colors.reserve(pointCount);

    glm::vec3 min(FLT_MAX), max(-FLT_MAX);
    auto callback = [&](pdal::PointRef& point) -> bool {
        glm::vec3 pos{0.0f}, color{1.0f};

        pos.x = point.getFieldAs<float>(pdal::Dimension::Id::X);
        pos.y = point.getFieldAs<float>(pdal::Dimension::Id::Y);
        pos.z = point.getFieldAs<float>(pdal::Dimension::Id::Z);

        min = glm::min(min, pos);
        max = glm::max(max, pos);

        positions.emplace_back(pos);

        if (point.hasDim(pdal::Dimension::Id::Red)) {
            color.r = point.getFieldAs<float>(pdal::Dimension::Id::Red) / 65535.0f;
            color.g = point.getFieldAs<float>(pdal::Dimension::Id::Green) / 65535.0f;
            color.b = point.getFieldAs<float>(pdal::Dimension::Id::Blue) / 65535.0f;
        }
        
        colors.emplace_back(color);

        return true;
    };

    pdal::StreamCallbackFilter filter;
    filter.setCallback(callback);
    filter.setInput(reader);

    pdal::FixedPointTable table(10240);
    filter.prepare(table);
    filter.execute(table);

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

    glm::vec3 center = (min + max) * 0.5f;
    for (auto& p : positions) {
        p -= center;
    }

    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &m_positionBuffer, &m_positionAllocation, nullptr);
    void* data;
    vmaMapMemory(allocator, m_positionAllocation, &data);
    memcpy(data, positions.data(), posSize);
    vmaUnmapMemory(allocator, m_positionAllocation);

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
