#include "stdafx.h"
#include "Object.h"
#include "Buffer.h"
#include "Octree.h"

PointCloudObject::PointCloudObject(
    LPCWSTR filepath,
    VkDevice device,
    VmaAllocator allocator,
    TransferManager* transferMgr
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

    m_bufferManager = std::make_unique<PointCloudBufferManager>(device, allocator, transferMgr);
    m_fileManager = std::make_unique<PointCloudFileManager>(std::format("{}_temp.bin", path));
    glm::vec3 min{info.m_bounds.minx, info.m_bounds.miny, info.m_bounds.minz};
    glm::vec3 max{info.m_bounds.maxx, info.m_bounds.maxy, info.m_bounds.maxz};
    Bound3D bound{min, max};
    m_octree = std::make_unique<Octree>(m_fileManager.get(), bound);

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
}

void PointCloudObject::updateBufferState(TransferManager* transferManager) {
    if (m_bufferManager) {
        
    }
}

void PointCloudObject::draw(VkCommandBuffer commandBuffer) const {

}
