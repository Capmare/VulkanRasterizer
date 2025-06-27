//
// Created by capma on 6/26/2025.
//

#ifndef RENDERER_H
#define RENDERER_H

#include <vulkan/vulkan.hpp>

#include "ImageFrame.h"
#include "vulkan/vulkan_raii.hpp"

class Renderer {
public:
    Renderer() = default;
    virtual ~Renderer() = default;

    Renderer(const Renderer&) = delete;
    Renderer(Renderer&&) noexcept = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer& operator=(Renderer&&) noexcept = delete;

    vk::raii::CommandPool CreateCommandPool(const vk::raii::Device& device, uint32_t queueFamilyIndex);
    vk::raii::CommandBuffer CreateCommandBuffer(const vk::raii::Device &device, const vk::raii::CommandPool &commandPool);


private:
	std::vector<ImageFrame> m_ImageFrames;


};



#endif //RENDERER_H
