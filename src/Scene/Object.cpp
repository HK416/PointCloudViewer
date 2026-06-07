#include "Core/stdafx.h"
#include "Scene/Object.h"
#include "Render/Buffer.h"
#include "Scene/Octree.h"
#include "Scene/Frustum.h"

PointCloudObject::PointCloudObject(
    const std::filesystem::path& filePath,
    VkDevice device,
    VmaAllocator allocator,
    TransferManager* transferMgr
) {
    std::u8string u8Path = filePath.u8string();
    m_utf8FilePath.assign(reinterpret_cast<const char*>(u8Path.c_str()), u8Path.size());

    std::error_code ec;
    m_fileSize = std::filesystem::file_size(filePath, ec);
    if (ec) m_fileSize = 0;

    pdal::Options options;
    options.add("filename", m_utf8FilePath);

    pdal::LasReader reader;
    reader.setOptions(options);

    pdal::QuickInfo info = reader.preview();
    uint32_t pointCount = (uint32_t)info.m_pointCount;
    if (pointCount == 0)
        throw std::runtime_error("the Point Cloud data is empty!");

    m_pointCount = info.m_pointCount;
    m_localOffset = glm::dvec3(
        (info.m_bounds.minx + info.m_bounds.maxx) * 0.5,
        (info.m_bounds.miny + info.m_bounds.maxy) * 0.5,
        (info.m_bounds.minz + info.m_bounds.maxz) * 0.5
    );
    m_terrainSize = glm::dvec3(
        info.m_bounds.maxx - info.m_bounds.minx,
        info.m_bounds.maxy - info.m_bounds.miny,
        info.m_bounds.maxz - info.m_bounds.minz
    );

    m_fileManager = std::make_unique<PointCloudFileManager>(std::format("{}_temp.bin", m_utf8FilePath));
    m_bufferManager = std::make_unique<PointCloudBufferManager>(device, allocator, transferMgr, m_fileManager.get());

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
    m_rotation = glm::quat(glm::vec3(glm::radians(-90.0f), 0.0f, 0.0f));
}

void PointCloudObject::getAllBounds(std::vector<Bound3D>& outBounds) const {
    if (m_octree) m_octree->getAllBounds(outBounds);
}

void PointCloudObject::updateBufferState() {
    if (m_bufferManager) {
        m_bufferManager->updateBufferState();
    }
}

void PointCloudObject::draw(const Frustum& frustum, glm::vec3 cameraPos, VkCommandBuffer commandBuffer) const {
    if (m_bufferManager) {
        m_bufferManager->beginFrame();
    }

    glm::mat4 model = glm::translate(glm::mat4(1.0f), m_position) *
                      glm::mat4_cast(m_rotation) *
                      glm::scale(glm::mat4(1.0f), m_scale);
    glm::mat4 invModel = glm::inverse(model);
    glm::vec3 localCameraPos = glm::vec3(invModel * glm::vec4(cameraPos, 1.0f));

    std::vector<ChunkRenderInfo> chunks;
    if (m_octree)
        m_octree->getVisibleChunks(frustum, localCameraPos, chunks);

    std::sort(chunks.begin(), chunks.end(), [&localCameraPos](const ChunkRenderInfo& a, const ChunkRenderInfo& b) {
        float distA = glm::dot(localCameraPos - a.center, localCameraPos - a.center);
        float distB = glm::dot(localCameraPos - b.center, localCameraPos - b.center);
        return distA < distB;
    });

    std::vector<PointCloudBuffer*> buffers;

    if (m_bufferManager) {
        for (auto&& chunk : chunks) {
            auto buffer = m_bufferManager->getOrRequestBuffer(chunk.id, chunk.span);
            if (buffer) buffers.push_back(buffer);
        }
    }

    for (auto buffer : buffers) {
        if (buffer->isReady()) {
            buffer->bind(commandBuffer);
            buffer->draw(commandBuffer);
        }
    }
}

const std::string& PointCloudObject::getFilePath() const {
    return m_utf8FilePath;
}

const std::uintmax_t PointCloudObject::getFileSize() const {
    return m_fileSize;
}

const glm::dvec3& PointCloudObject::getLocalOffset() const {
    return m_localOffset;
}

const glm::dvec3& PointCloudObject::getTerrainSize() const {
    return m_terrainSize;
}
