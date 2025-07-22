//
// Created by capma on 7/5/2025.
//

#include "Buffer.h"

#include "ResourceTracker.h"

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

void Buffer::Create(VmaAllocator allocator,
                    vk::DeviceSize size,
                    vk::BufferUsageFlags usage,
                    VmaMemoryUsage memoryUsage,
                    VkBuffer &buffer,
                    VmaAllocation &allocation,
                    VmaAllocationInfo &allocInfo,
                    VmaAllocationCreateFlags flags, ResourceTracker* AllocationTracker, const std::string& AllocName) {
    vk::BufferCreateInfo bufferInfo{};
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = memoryUsage;
    allocCreateInfo.flags = flags;

    vmaCreateBuffer(allocator,
                    reinterpret_cast<VkBufferCreateInfo*>(&bufferInfo),
                    &allocCreateInfo,
                    &buffer,
                    &allocation,
                    &allocInfo);

    if (AllocationTracker == nullptr) {
        std::cout << "Failed to use allocation tracker, is nullptr";
        return;
    }
    AllocationTracker->TrackAllocation(allocation,AllocName);
}

void Buffer::UploadData(VmaAllocator allocator,
                        VmaAllocation allocation,
                        const void* data,
                        size_t size) {
    void* dst;
    vmaMapMemory(allocator, allocation, &dst);
    std::memcpy(dst, data, size);
    vmaUnmapMemory(allocator, allocation);
}

void Buffer::Destroy(VmaAllocator allocator, VkBuffer buffer, VmaAllocation allocation, ResourceTracker* AllocationTracker) {
    AllocationTracker->UntrackAllocation(allocation);
    vmaDestroyBuffer(allocator, buffer, allocation);
}