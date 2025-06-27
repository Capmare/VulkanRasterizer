//
// Created by capma on 6/26/2025.
//

#include "Renderer.h"

#include <iostream>
#include <ostream>





vk::raii::CommandPool Renderer::CreateCommandPool(const vk::raii::Device &device, uint32_t queueFamilyIndex) {
    vk::CommandPoolCreateInfo poolInfo = {};
    poolInfo.setFlags(vk::CommandPoolCreateFlags() | vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    poolInfo.setQueueFamilyIndex(queueFamilyIndex);

    return device.createCommandPool(poolInfo);
}

vk::raii::CommandBuffer Renderer::CreateCommandBuffer(const vk::raii::Device &device, const vk::raii::CommandPool& commandPool) {
    vk::CommandBufferAllocateInfo allocInfo = {};
    allocInfo.setCommandBufferCount(1);
    allocInfo.setCommandPool(commandPool);
    allocInfo.level = vk::CommandBufferLevel::ePrimary;

    return std::move(device.allocateCommandBuffers(allocInfo).at(0));
}
