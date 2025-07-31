//
// Created by capma on 7/5/2025.
//

#include "Buffer.h"

#include "ResourceTracker.h"

BufferInfo Buffer::CreateMapped(VmaAllocator allocator, vk::DeviceSize size, vk::BufferUsageFlags usage, VmaMemoryUsage memoryUsage,
    VmaAllocationCreateFlags flags, class ResourceTracker *AllocationTracker, const std::string &AllocName) {

    return CreateUnmapped(allocator, size, usage, memoryUsage, flags | VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, AllocationTracker, AllocName);
}

BufferInfo Buffer::CreateUnmapped(VmaAllocator allocator, vk::DeviceSize size, vk::BufferUsageFlags usage,
    VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags flags, class ResourceTracker *AllocationTracker,
    const std::string &AllocName) {
    vk::BufferCreateInfo bufferInfo{};
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;


    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = memoryUsage;
    allocCreateInfo.flags = flags ;

    m_BufferInfos.emplace_back(BufferInfo());
    vmaCreateBuffer(allocator,
                    reinterpret_cast<VkBufferCreateInfo*>(&bufferInfo),
                    &allocCreateInfo,
                    &m_BufferInfos.back().m_Buffer,
                    &m_BufferInfos.back().m_Allocation,
                    &m_BufferInfos.back().m_AllocInfo);

    m_BufferInfos.back().m_MappedData = m_BufferInfos.back().m_AllocInfo.pMappedData;
    m_BufferInfos.back().size = bufferInfo.size;

    if (AllocationTracker == nullptr) {
        std::cout << "Failed to use allocation tracker, is nullptr";
        return m_BufferInfos.back();
    }
    AllocationTracker->TrackAllocation(m_BufferInfos.back().m_Allocation,AllocName);

    return m_BufferInfos.back();
}


void Buffer::Copy(vk::Buffer SourceBuffer, VmaAllocationInfo SourceInfo, vk::Buffer DestBuffer,
                  VmaAllocationInfo DestInfo, vk::Queue Queue, vk::CommandBuffer CommandBuffer) {

    if (DestInfo.size < SourceInfo.size) {
        throw std::runtime_error("Destination buffer too small for source data!");
    }

    auto result = CommandBuffer.reset();


    vk::CommandBufferBeginInfo BufferInfo;
    BufferInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    result = CommandBuffer.begin(BufferInfo);


    vk::BufferCopy copyRegion;
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = SourceInfo.size;

    CommandBuffer.copyBuffer(SourceBuffer, DestBuffer, 1, &copyRegion);
    result = CommandBuffer.end();

    vk::SubmitInfo SubmitInfo;
    SubmitInfo.setCommandBufferCount(1);
    SubmitInfo.setPCommandBuffers(&CommandBuffer);
    result = Queue.submit(1, &SubmitInfo,nullptr);

    result = Queue.waitIdle();

}

void Buffer::UploadData(BufferInfo& buff, const void* data, size_t size, size_t offset) {
    assert(buff.m_MappedData != nullptr);
    assert(offset + size <= buff.size);

    std::memcpy(static_cast<char*>(buff.m_MappedData) + offset, data, size);
}

void Buffer::Destroy(VmaAllocator allocator, VkBuffer buffer, VmaAllocation allocation, ResourceTracker* AllocationTracker) {
    AllocationTracker->UntrackAllocation(allocation);
    vmaDestroyBuffer(allocator, buffer, allocation);
}