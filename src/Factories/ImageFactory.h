//
// Created by capma on 6/24/2025.
//

#ifndef IMAGEFACTORY_H
#define IMAGEFACTORY_H

#include "Buffer.h"
#include "ResourceTracker.h"
#include <vma/vk_mem_alloc.h>
#include "vulkan/vulkan_raii.hpp"

struct ImageResource {
    vk::Image image{};
    vk::Extent2D extent{};
    vk::Format format{};
    vk::ImageAspectFlags imageAspectFlags{};
    vk::ImageLayout imageLayout{};
    VmaAllocation allocation{};

};

class ImageFactory {
public:
    ImageFactory() = default;
    virtual ~ImageFactory() = default;

    ImageFactory(const ImageFactory&) = delete;
    ImageFactory(ImageFactory&&) noexcept = delete;
    ImageFactory& operator=(const ImageFactory&) = delete;
    ImageFactory& operator=(ImageFactory&&) noexcept = delete;

    static ImageResource LoadTexture(
        Buffer *buff,
        const std::string &filename,
        const vk::raii::Device &device,
        VmaAllocator allocator,
        const vk::raii::CommandPool &commandPool, const vk::raii::Queue &graphicsQueue, vk::Format ColorFormat, vk::
        ImageAspectFlagBits aspect, ResourceTracker *AllocationTracker);

    static ImageResource LoadHDRTexture(Buffer *buff, const std::string &filename, const vk::raii::Device &device,
                                 VmaAllocator allocator, const vk::raii::CommandPool &commandPool,
                                 const vk::raii::Queue &graphicsQueue, vk::Format ColorFormat,
                                 vk::ImageAspectFlagBits aspect, ResourceTracker *AllocationTracker);

    ImageResource LoadTextureFromMemory(Buffer *buff, const unsigned char *data,
                                        size_t dataSize, const vk::raii::Device &device,
                                        VmaAllocator allocator,
                                        const vk::raii::CommandPool &commandPool, const vk::raii::Queue &graphicsQueue, vk::Format ColorFormat, vk::
                                        ImageAspectFlagBits aspect, ResourceTracker *AllocationTracker);


    static VkImageView CreateImageView(const vk::raii::Device &device, vk::Image Image, vk::Format Format, vk::ImageAspectFlags Aspect, ::ResourceTracker *
                                       ResourceTracker, const std::string &Name, uint32_t BaseArrLayer = 0, vk::ImageViewType ViewType = vk::ImageViewType::e2D);

    static void CreateImage(const vk::raii::Device &device, VmaAllocator Allocator, ImageResource &Image, vk::ImageCreateInfo imageInfo, const std
                            ::string &name);

    static void ShiftImageLayout(
        const vk::CommandBuffer &commandBuffer, ImageResource &image,
        vk::ImageLayout newLayout,
        vk::AccessFlags srcAccessMask, vk::AccessFlags dstAccessMask,
        vk::PipelineStageFlags srcStage, vk::PipelineStageFlags dstStage, uint32_t layerCount = 1
    );
};




#endif //IMAGEFACTORY_H
