//
// Created by capma on 7/5/2025.
//

#ifndef BUFFER_H
#define BUFFER_H

#define VULKAN_HPP_NO_EXCEPTIONS
#include <vulkan/vulkan.hpp>
#include <vma/vk_mem_alloc.h>


class Buffer {
public:
    Buffer() = default;
    virtual ~Buffer() = default;

    Buffer(const Buffer&) = delete;
    Buffer(Buffer&&) noexcept = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer& operator=(Buffer&&) noexcept = delete;

    static void Copy(vk::Buffer SourceBuffer, VmaAllocationInfo SourceInfo, vk::Buffer DestBuffer, VmaAllocationInfo DestInfo, vk::Queue
                     Queue, vk::CommandBuffer CommandBuffer);
};



#endif //BUFFER_H
