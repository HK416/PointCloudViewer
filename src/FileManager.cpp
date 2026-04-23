#include "stdafx.h"
#include "FileManager.h"

PointCloudFileManager::PointCloudFileManager(const std::string& filePath) {
    m_dataFile.open(filePath, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
    if (!m_dataFile.is_open()) {
        throw std::runtime_error("Cannot open data file!");
    }

    m_filePath = filePath;
    m_dataFile.seekp(0, std::ios::end);
    m_currentWriteOffset = m_dataFile.tellp();
}

PointCloudFileManager::~PointCloudFileManager() {
    if (m_dataFile.is_open())
        m_dataFile.close();

    std::filesystem::remove(m_filePath);
}

std::vector<PointCloudVertex> PointCloudFileManager::readData(ChunkSpan span) {
    if (span.offsetBytes >= m_currentWriteOffset || span.pointCount <= 0)
        return {};

    size_t sizeInBytes = span.pointCount * sizeof(PointCloudVertex);
    std::vector<PointCloudVertex> points(span.pointCount);

    std::lock_guard<std::mutex> lock(m_fileMutex);

    m_dataFile.clear();
    m_dataFile.seekg(span.offsetBytes, std::ios::beg);
    m_dataFile.read(reinterpret_cast<char*>(points.data()), sizeInBytes);

    return points;
}

ChunkSpan PointCloudFileManager::writeData(const std::vector<PointCloudVertex>& points) {
    if (!m_dataFile.is_open() || points.empty())
        return {0, 0};

    size_t sizeInBytes = points.size() * sizeof(PointCloudVertex);
    ChunkSpan span{m_currentWriteOffset, points.size()};

    m_dataFile.seekp(m_currentWriteOffset, std::ios::beg);
    m_dataFile.write(reinterpret_cast<const char*>(points.data()), sizeInBytes);
    m_dataFile.flush();

    m_currentWriteOffset += sizeInBytes;
    return span;
}
