#include "stdafx.h"
#include "TransferManager.h"

TransferManager::TransferManager(VkDevice device, uint32_t queueFamilyIndex, VkQueue queue) 
    : m_device(device), m_queue(queue) {
    
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndex;
    vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool);

    VkSemaphoreTypeCreateInfo typeInfo = {};
    typeInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    typeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    typeInfo.initialValue = 0;

    VkSemaphoreCreateInfo semInfo = {};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semInfo.pNext = &typeInfo;
    vkCreateSemaphore(m_device, &semInfo, nullptr, &m_timelineSemaphore);
}

TransferManager::~TransferManager() {
    vkDestroySemaphore(m_device, m_timelineSemaphore, nullptr);
    vkDestroyCommandPool(m_device, m_commandPool, nullptr);
}

uint64_t TransferManager::requestTransfer(VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    m_currentValue++;
    uint64_t waitValue = m_currentValue;

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cb;
    vkAllocateCommandBuffers(m_device, &allocInfo, &cb);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &beginInfo);

    VkBufferCopy copyRegion = { 0, 0, size };
    vkCmdCopyBuffer(cb, src, dst, 1, &copyRegion);

    vkEndCommandBuffer(cb);

    VkTimelineSemaphoreSubmitInfo timelineInfo = {};
    timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    timelineInfo.signalSemaphoreValueCount = 1;
    timelineInfo.pSignalSemaphoreValues = &waitValue;

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = &timelineInfo;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cb;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &m_timelineSemaphore;

    vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE);

    return waitValue;
}

bool TransferManager::isFinished(uint64_t value) {
    uint64_t counter;
    vkGetSemaphoreCounterValue(m_device, m_timelineSemaphore, &counter);
    return counter >= value;
}
