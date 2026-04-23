#include "stdafx.h"
#include "Object.h"
#include "Buffer.h"
#include "Octree.h"
#include "Frustum.h"

PointCloudObject::PointCloudObject(
    const std::filesystem::path& filePath,
    VkDevice device,
    VmaAllocator allocator,
    TransferManager* transferMgr
) {
    std::u8string u8Path = filePath.u8string();
    std::string utf8PathStr(reinterpret_cast<const char*>(u8Path.c_str()), u8Path.size());

    pdal::Options options;
    options.add("filename", utf8PathStr);

    pdal::LasReader reader;
    reader.setOptions(options);

    pdal::QuickInfo info = reader.preview();
    uint32_t pointCount = (uint32_t)info.m_pointCount;
    if (pointCount == 0)
        throw std::runtime_error("the Point Cloud data is empty!");

    m_fileManager = std::make_unique<PointCloudFileManager>(std::format("{}_temp.bin", utf8PathStr));
    m_bufferManager = std::make_unique<PointCloudBufferManager>(device, allocator, transferMgr, m_fileManager.get());

    m_localOffset = glm::dvec3(
        (info.m_bounds.minx + info.m_bounds.maxx) * 0.5,
        (info.m_bounds.miny + info.m_bounds.maxy) * 0.5,
        (info.m_bounds.minz + info.m_bounds.maxz) * 0.5
    );

    glm::vec3 min{ (float)(info.m_bounds.minx - m_localOffset.x), (float)(info.m_bounds.miny - m_localOffset.y), (float)(info.m_bounds.minz - m_localOffset.z) };
    glm::vec3 max{ (float)(info.m_bounds.maxx - m_localOffset.x), (float)(info.m_bounds.maxy - m_localOffset.y), (float)(info.m_bounds.maxz - m_localOffset.z) };
    
    Bound3D bound{ min, max };
    m_octree = std::make_unique<Octree>(m_fileManager.get(), bound);

    auto callback = [&](pdal::PointRef& point) -> bool {
        glm::vec3 pos{ 0.0f }, color{ 1.0f };

        pos.x = (float)(point.getFieldAs<double>(pdal::Dimension::Id::X) - m_localOffset.x);
        pos.y = (float)(point.getFieldAs<double>(pdal::Dimension::Id::Y) - m_localOffset.y);
        pos.z = (float)(point.getFieldAs<double>(pdal::Dimension::Id::Z) - m_localOffset.z);

        if (point.hasDim(pdal::Dimension::Id::Red)) {
            color.r = point.getFieldAs<float>(pdal::Dimension::Id::Red) / 65535.0f;
            color.g = point.getFieldAs<float>(pdal::Dimension::Id::Green) / 65535.0f;
            color.b = point.getFieldAs<float>(pdal::Dimension::Id::Blue) / 65535.0f;
        }

        m_octree->insert({ pos, color, 0.0f });

        return true;
    };

    pdal::StreamCallbackFilter filter;
    filter.setCallback(callback);
    filter.setInput(reader);

    pdal::FixedPointTable table(10240);
    filter.prepare(table);
    filter.execute(table);
    m_octree->flushRemainingToDisk();

    m_position = glm::vec3(0.0f);
}

void PointCloudObject::updateBufferState() {
    if (m_bufferManager) {
        m_bufferManager->updateBufferState();
    }
}

void PointCloudObject::draw(const Frustum& frustum, VkCommandBuffer commandBuffer) const {
    std::vector<std::pair<uint64_t, ChunkSpan>> chunks;

    if (m_octree)
        m_octree->getVisibleChunks(frustum, chunks);

    std::vector<PointCloudBuffer*> buffers;

    if (m_bufferManager) {
        for (auto&& chunk : chunks) {
            auto buffer = m_bufferManager->getOrRequestBuffer(chunk.first, chunk.second);
            if (buffer)
                buffers.push_back(buffer);
        }
    }

    for (auto buffer : buffers) {
        if (buffer->isReady()) {
            buffer->bind(commandBuffer);
            buffer->draw(commandBuffer);
        }
    }
}

Bound3D PointCloudObject::getTotalBounds() const {
    return m_octree->getTotalBounds();
}
