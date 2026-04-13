#include "stdafx.h"
#include "Object.h"

PointCloudObject::PointCloudObject(
    LPCWSTR filepath,
    VkDevice device,
    VmaAllocator allocator,
    VkCommandBuffer commandBuffer
) {
    std::string path((char*)CW2A(filepath));

    pdal::Options options;
    options.add("filename", path);

    pdal::LasReader reader;
    reader.setOptions(options);

    pdal::QuickInfo info = reader.preview();
    uint32_t pointCount = (uint32_t)info.m_pointCount;
    if (pointCount == 0)
        throw std::runtime_error("the Point Cloud data is empty!");

    glm::vec3 min{info.m_bounds.minx, info.m_bounds.miny, info.m_bounds.minz};
    glm::vec3 max{info.m_bounds.maxx, info.m_bounds.maxy, info.m_bounds.maxz};
    m_octree = std::make_unique<Octree>(Bound3D{min, max});

    std::vector<PointCloudVertex> vertices;
    vertices.reserve(pointCount);

    auto callback = [&](pdal::PointRef& point) -> bool {
        glm::vec3 pos{0.0f}, color{1.0f};

        pos.x = point.getFieldAs<float>(pdal::Dimension::Id::X);
        pos.y = point.getFieldAs<float>(pdal::Dimension::Id::Y);
        pos.z = point.getFieldAs<float>(pdal::Dimension::Id::Z);

        if (point.hasDim(pdal::Dimension::Id::Red)) {
            color.r = point.getFieldAs<float>(pdal::Dimension::Id::Red) / 65535.0f;
            color.g = point.getFieldAs<float>(pdal::Dimension::Id::Green) / 65535.0f;
            color.b = point.getFieldAs<float>(pdal::Dimension::Id::Blue) / 65535.0f;
        }

        vertices.emplace_back(pos, color, 0.0f);
        m_octree->insert({pos, color, 0.0f});

        return true;
    };

    pdal::StreamCallbackFilter filter;
    filter.setCallback(callback);
    filter.setInput(reader);

    pdal::FixedPointTable table(10240);
    filter.prepare(table);
    filter.execute(table);
    m_octree->flushRemainingToDisk();

    glm::vec3 center = (min + max) * 0.5f;
    for (auto& v : vertices) {
        v.position -= center;
    }

    m_mesh = std::make_unique<PointCloudMesh>(
        device, allocator, commandBuffer, vertices
    );
}

const std::unique_ptr<PointCloudMesh>& PointCloudObject::getMesh() const {
    return m_mesh;
}

const std::unique_ptr<Octree>& PointCloudObject::getHierarchy() const {
    return m_octree;
}
