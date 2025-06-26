//
// Created by capma on 6/24/2025.
//

#include "ImageFactory.h"


vk::raii::ImageView ImageFactory::CreateImageView(const vk::raii::Device& device, vk::Image Image, vk::Format Format) {

    vk::ImageViewCreateInfo CreateInfo{};
    CreateInfo.image = Image;
    CreateInfo.format = Format;
    CreateInfo.viewType = vk::ImageViewType::e2D;
    CreateInfo.components.r = vk::ComponentSwizzle::eIdentity;
    CreateInfo.components.g = vk::ComponentSwizzle::eIdentity;
    CreateInfo.components.b = vk::ComponentSwizzle::eIdentity;
    CreateInfo.components.a = vk::ComponentSwizzle::eIdentity;
    CreateInfo.subresourceRange.layerCount = 1;
    CreateInfo.subresourceRange.baseMipLevel = 0;
    CreateInfo.subresourceRange.baseArrayLayer = 0;
    CreateInfo.subresourceRange.levelCount = 1;
    CreateInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;


    return device.createImageView(CreateInfo);
}

void ImageFactory::ShiftImageLayout(const vk::raii::CommandBuffer& commandBuffer, vk::Image image, vk::ImageLayout oldLayout,
    vk::ImageLayout newLayout, vk::AccessFlags srcAccessMask, vk::AccessFlags dstAccessMask,
    vk::PipelineStageFlags srcStage, vk::PipelineStageFlags dstStage) {


    vk::ImageSubresourceRange access;
    access.aspectMask = vk::ImageAspectFlagBits::eColor;
    access.baseMipLevel = 0;
    access.levelCount = 1;
    access.baseArrayLayer = 0;
    access.layerCount = 1;

    vk::ImageMemoryBarrier barrier;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = access;

    vk::PipelineStageFlags sourceStage, destinationStage;

    barrier.srcAccessMask = srcAccessMask;
    barrier.dstAccessMask = dstAccessMask;

    commandBuffer.pipelineBarrier(
        srcStage, dstStage, vk::DependencyFlags(), nullptr, nullptr, barrier);

}
