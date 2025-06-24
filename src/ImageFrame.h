//
// Created by capma on 6/24/2025.
//

#ifndef IMAGEFRAME_H
#define IMAGEFRAME_H
#include <Vulkan/vulkan.hpp>

#include "Factories/ImageFactory.h"
#include "vulkan/vulkan_raii.hpp"


struct ImageFrame {

    ImageFrame(vk::Image Image, const vk::raii::Device& LogicalDevice, vk::Format Format) {
        m_ImageView = ImageFactory::CreateImageView(LogicalDevice, Image, Format);

    }

    vk::Image m_Image{};
    vk::ImageView m_ImageView{};

};

#endif //IMAGEFRAME_H
