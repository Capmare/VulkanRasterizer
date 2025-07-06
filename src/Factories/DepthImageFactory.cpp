#include "DepthImageFactory.h"
#include <stdexcept>

DepthImageFactory::DepthImageFactory(vk::raii::Device& device, VmaAllocator allocator, vk::Extent2D extent, std::deque<std::function<void(VmaAllocator)>>& deletionQueue)
    : m_Device(&device), m_Allocator(allocator) {
    CreateDepthResources(extent, deletionQueue);
}


void DepthImageFactory::CreateDepthResources(vk::Extent2D extent, std::deque<std::function<void(VmaAllocator)>>& deletionQueue) {
    vk::ImageCreateInfo imageInfo{};
    imageInfo.setImageType(vk::ImageType::e2D)
        .setFormat(m_Format)
        .setExtent({extent.width, extent.height, 1})
        .setMipLevels(1)
        .setArrayLayers(1)
        .setSamples(vk::SampleCountFlagBits::e1)
        .setTiling(vk::ImageTiling::eOptimal)
        .setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment)
        .setSharingMode(vk::SharingMode::eExclusive)
        .setInitialLayout(vk::ImageLayout::eUndefined);

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkImage rawImage = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;

    VkResult result = vmaCreateImage(
        m_Allocator,
        reinterpret_cast<VkImageCreateInfo*>(&imageInfo),
        &allocInfo,
        &rawImage,
        &allocation,
        nullptr
    );
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create depth image with VMA");
    }

    m_RawImage = rawImage;
    m_Allocation = allocation;

    m_Image = vk::Image(rawImage);

    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.setImage(m_Image)
        .setViewType(vk::ImageViewType::e2D)
        .setFormat(m_Format)
        .setSubresourceRange(
            vk::ImageSubresourceRange{}
                .setAspectMask(vk::ImageAspectFlagBits::eDepth)
                .setBaseMipLevel(0)
                .setLevelCount(1)
                .setBaseArrayLayer(0)
                .setLayerCount(1)
        );

    m_ImageView = vk::raii::ImageView(*m_Device, viewInfo);

    deletionQueue.emplace_back([rawImage, allocation](VmaAllocator allocator) {
        vmaDestroyImage(allocator, rawImage, allocation);
    });
}
