#pragma once

struct PointCloudVertex;

struct ChunkSpan {
    size_t offsetBytes = 0;
    size_t pointCount = 0;
};

struct StreamRequest {
    uint64_t id;
    ChunkSpan span;
};

class PointCloudFileManager {
public:
    PointCloudFileManager() = delete;
    PointCloudFileManager(const PointCloudFileManager&) = delete;
    PointCloudFileManager& operator=(const PointCloudFileManager&) = delete;

    PointCloudFileManager(const std::filesystem::path& filePath);
    ~PointCloudFileManager();

    std::vector<PointCloudVertex> readData(ChunkSpan span);
    std::future<std::vector<PointCloudVertex>> readDataAsync(ChunkSpan span);

    ChunkSpan writeData(const std::vector<PointCloudVertex>& points);
    void flush();

private:
    std::filesystem::path m_filePath;

    std::fstream m_dataFile;
    size_t m_currentWriteOffset = 0;

    std::mutex m_fileMutex;
};
