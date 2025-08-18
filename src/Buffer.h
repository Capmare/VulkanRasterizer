#ifndef BUFFER_H
#define BUFFER_H

#define VULKAN_HPP_NO_EXCEPTIONS
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>
#include <cstring>

#include "vulkan/vulkan_raii.hpp"

struct BufferInfo {
    VkBuffer m_Buffer{};
    VmaAllocation m_Allocation{};
    VmaAllocationInfo m_AllocInfo{};
    void* m_MappedData{};
    VkDeviceSize size{};

};

class Buffer {

public:
    Buffer() = default;
    virtual ~Buffer() = default;

    Buffer(const Buffer&) = delete;
    Buffer(Buffer&&) noexcept = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer& operator=(Buffer&&) noexcept = delete;

    BufferInfo CreateMapped(VmaAllocator allocator, vk::DeviceSize size, vk::BufferUsageFlags usage, VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags flags = 0,
        class ResourceTracker* AllocationTracker = nullptr,
        const std::string& AllocName = "Buffer");

    BufferInfo CreateUnmapped(VmaAllocator allocator, vk::DeviceSize size, vk::BufferUsageFlags usage, VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags flags = 0,
        class ResourceTracker* AllocationTracker = nullptr,
        const std::string& AllocName = "Buffer");


    static void Copy(vk::Buffer SourceBuffer, VmaAllocationInfo SourceInfo,
                     vk::Buffer DestBuffer, VmaAllocationInfo DestInfo,
                     vk::Queue Queue, vk::CommandBuffer CommandBuffer);

    static void UploadData(
        BufferInfo &buff, const void *data, size_t size, size_t offset = 0);

    static void Destroy(VmaAllocator allocator, VkBuffer buffer, VmaAllocation allocation, ResourceTracker *AllocationTracker);

private:
    std::vector<BufferInfo> m_BufferInfos{10};


};

#endif //BUFFER_H
