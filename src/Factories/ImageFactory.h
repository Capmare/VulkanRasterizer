//
// Created by capma on 6/24/2025.
//

#ifndef IMAGEFACTORY_H
#define IMAGEFACTORY_H

#include "Buffer.h"
#include "ResourceTracker.h"
#include "vk_mem_alloc.h"
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

    static VkImageView CreateImageView(const vk::raii::Device &device, vk::Image Image, vk::Format Format, vk::ImageAspectFlags Aspect, ResourceTracker *
                                       ResourceTracker, const std::string &Name);

    static void CreateImage(VmaAllocator Allocator,ImageResource& Image, vk::ImageCreateInfo imageInfo);;

    static void ShiftImageLayout(
        const vk::CommandBuffer &commandBuffer, ImageResource &image,
        vk::ImageLayout newLayout,
        vk::AccessFlags srcAccessMask, vk::AccessFlags dstAccessMask,
        vk::PipelineStageFlags srcStage, vk::PipelineStageFlags dstStage
    );
};




#endif //IMAGEFACTORY_H
