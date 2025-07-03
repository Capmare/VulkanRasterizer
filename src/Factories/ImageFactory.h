//
// Created by capma on 6/24/2025.
//

#ifndef IMAGEFACTORY_H
#define IMAGEFACTORY_H
#include "vulkan/vulkan_raii.hpp"


class ImageFactory {
public:
    ImageFactory() = default;
    virtual ~ImageFactory() = default;

    ImageFactory(const ImageFactory&) = delete;
    ImageFactory(ImageFactory&&) noexcept = delete;
    ImageFactory& operator=(const ImageFactory&) = delete;
    ImageFactory& operator=(ImageFactory&&) noexcept = delete;

    static vk::raii::ImageView CreateImageView(const vk::raii::Device& device,vk::Image Image ,vk::Format Format);

    static void ShiftImageLayout(
        const vk::CommandBuffer &commandBuffer, vk::Image image,
        vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
        vk::AccessFlags srcAccessMask, vk::AccessFlags dstAccessMask,
        vk::PipelineStageFlags srcStage, vk::PipelineStageFlags dstStage
    );
};




#endif //IMAGEFACTORY_H
