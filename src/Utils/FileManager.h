#pragma once
#include "Render/Buffer.h"

struct ChunkSpan {
    size_t offsetBytes = 0;
    size_t pointCount = 0;
};

class PointCloudFileManager {
public:
    PointCloudFileManager() = delete;
    PointCloudFileManager(const PointCloudFileManager&) = delete;
    PointCloudFileManager(const std::string& filePath);
    ~PointCloudFileManager();

    std::vector<PointCloudVertex> readData(ChunkSpan span);
    ChunkSpan writeData(const std::vector<PointCloudVertex>& points);

private:
    std::string m_filePath;
    std::fstream m_dataFile;
    size_t m_currentWriteOffset = 0;

    std::mutex m_fileMutex;
};