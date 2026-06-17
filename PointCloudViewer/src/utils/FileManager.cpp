#include "stdafx.h"
#include "FileManager.h"
#include "PointCloudManager.h"

PointCloudFileManager::PointCloudFileManager(const std::filesystem::path& filePath) 
    : m_filePath(filePath) 
{
    // 기존 파일 열기 시도
    m_dataFile.open(m_filePath, std::ios::binary | std::ios::in | std::ios::out);
    
    if (!m_dataFile.is_open()) {
        m_dataFile.clear();
        // 새 빈 파일 생성
        std::ofstream create_file(m_filePath, std::ios::binary | std::ios::out | std::ios::trunc);
        create_file.close();
        
        // 읽기 및 쓰기 모드로 다시 열기
        m_dataFile.open(m_filePath, std::ios::binary | std::ios::in | std::ios::out);
    }

    if (m_dataFile.is_open()) {
        m_dataFile.seekp(0, std::ios::end);
        m_currentWriteOffset = m_dataFile.tellp();
        spdlog::info("[PointCloudFileManager] Initialized file: {} (Size: {} bytes)", m_filePath.string(), m_currentWriteOffset);
    } else {
        spdlog::error("[PointCloudFileManager] Failed to open file: {}", m_filePath.string());
        m_currentWriteOffset = 0;
    }
}

PointCloudFileManager::~PointCloudFileManager() {
    if (m_dataFile.is_open()) {
        m_dataFile.close();
    }
}

std::vector<PointCloudVertex> PointCloudFileManager::readData(ChunkSpan span) {
    if (span.offsetBytes >= m_currentWriteOffset || span.pointCount <= 0) {
        return {};
    }

    size_t sizeInBytes = span.pointCount * sizeof(PointCloudVertex);
    std::vector<PointCloudVertex> points(span.pointCount);

    std::lock_guard<std::mutex> lock(m_fileMutex);

    m_dataFile.clear();
    m_dataFile.seekg(span.offsetBytes, std::ios::beg);
    m_dataFile.read(reinterpret_cast<char*>(points.data()), sizeInBytes);

    if (m_dataFile.fail()) {
        spdlog::error("[PointCloudFileManager] Failed to read {} bytes at offset {}!", sizeInBytes, span.offsetBytes);
        return {};
    }

    return points;
}

std::future<std::vector<PointCloudVertex>> PointCloudFileManager::readDataAsync(ChunkSpan span) {
    return std::async(std::launch::async, [this, span] {
        return this->readData(span);
    });
}

ChunkSpan PointCloudFileManager::writeData(const std::vector<PointCloudVertex>& points) {
    if (!m_dataFile.is_open() || points.empty()) {
        return {0, 0};
    }

    size_t sizeInBytes = points.size() * sizeof(PointCloudVertex);

    std::lock_guard<std::mutex> lock(m_fileMutex);

    ChunkSpan span{m_currentWriteOffset, points.size()};

    m_dataFile.clear();
    m_dataFile.seekp(m_currentWriteOffset, std::ios::beg);
    m_dataFile.write(reinterpret_cast<const char*>(points.data()), sizeInBytes);
    // 작은 데이터 쓰기 시 성능 저하를 막기 위해 flush 생략

    if (m_dataFile.fail()) {
        spdlog::error("[PointCloudFileManager] Failed to write {} bytes at offset {}!", sizeInBytes, m_currentWriteOffset);
        return {0, 0};
    }

    m_currentWriteOffset += sizeInBytes;
    return span;
}

void PointCloudFileManager::flush() {
    std::lock_guard<std::mutex> lock(m_fileMutex);
    if (m_dataFile.is_open()) {
        m_dataFile.flush();
    }
}
