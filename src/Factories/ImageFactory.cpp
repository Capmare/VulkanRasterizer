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