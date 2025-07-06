//
// Created by capma on 6/24/2025.
//

#ifndef IMAGEFACTORY_H
#define IMAGEFACTORY_H

#include "vk_mem_alloc.h"
#include "vulkan/vulkan_raii.hpp"

struct ImageResource {
    vk::raii::Image image;
    VmaAllocation allocation;
    vk::raii::ImageView imageView;
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
        const std::string &filename,
        const vk::raii::Device &device,
        VmaAllocator allocator,
        const vk::raii::CommandPool &commandPool,
        const vk::raii::Queue &graphicsQueue);

    static vk::raii::ImageView CreateImageView(const vk::raii::Device& device,vk::Image Image ,vk::Format Format);

    static void ShiftImageLayout(
        const vk::CommandBuffer &commandBuffer, vk::Image image,
        vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
        vk::AccessFlags srcAccessMask, vk::AccessFlags dstAccessMask,
        vk::PipelineStageFlags srcStage, vk::PipelineStageFlags dstStage
    );
};




#endif //IMAGEFACTORY_H
