#ifndef BUFFER_H
#define BUFFER_H

#define VULKAN_HPP_NO_EXCEPTIONS
#include <vulkan/vulkan.hpp>
#include <vma/vk_mem_alloc.h>
#include <cstring>

class Buffer {
public:
    static void Copy(vk::Buffer SourceBuffer, VmaAllocationInfo SourceInfo,
                     vk::Buffer DestBuffer, VmaAllocationInfo DestInfo,
                     vk::Queue Queue, vk::CommandBuffer CommandBuffer);

    static void Create(VmaAllocator allocator,
                       vk::DeviceSize size,
                       vk::BufferUsageFlags usage,
                       VmaMemoryUsage memoryUsage,
                       VkBuffer &buffer,
                       VmaAllocation &allocation,
                       VmaAllocationInfo &allocInfo,
                       VmaAllocationCreateFlags flags = 0);

    static void UploadData(VmaAllocator allocator,
                           VmaAllocation allocation,
                           const void* data,
                           size_t size);

    static void Destroy(VmaAllocator allocator, VkBuffer buffer, VmaAllocation allocation);
};

#endif //BUFFER_H
