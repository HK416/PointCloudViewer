#pragma once
#include "Buffer.h"

struct ChunkSpan {
    size_t offsetBytes = 0;
    size_t pointCount = 0;
};

class PointCloudFileManager {
public:
    PointCloudFileManager() = delete;
    PointCloudFileManager(const PointCloudFileManager&) = delete;
    PointCloudFileManager(const std::string& filename);
    ~PointCloudFileManager();

    std::vector<PointCloudVertex> readData(ChunkSpan span);
    ChunkSpan writeData(const std::vector<PointCloudVertex>& points);

private:
    std::fstream m_dataFile;
    size_t m_currentWriteOffset = 0;
};