//
// Created by capma on 6/24/2025.
//

#include "ImageFactory.h"

#include "Buffer.h"
#define STB_IMAGE_IMPLEMENTATION
#include <filesystem>
#include <iostream>

#include "stb_image.h"

ImageResource ImageFactory::LoadTexture(
    const std::string &filename,
    const vk::raii::Device &device,
    VmaAllocator allocator,
    const vk::raii::CommandPool &commandPool,
    const vk::raii::Queue &graphicsQueue)
{

    ImageResource imgResource{};

    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(filename.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    std::filesystem::path absPath = std::filesystem::absolute(filename);

    if (!pixels) {
        throw std::runtime_error("Failed to load texture image: " + absPath.string());
    }

    if (!pixels || texWidth == 0 || texHeight == 0) {
        std::cout << "Loaded texture: " << texWidth << "x" << texHeight << " from " << filename << "\n";
        throw std::runtime_error("Failed to load texture or texture has zero size: " + absPath.string());
    }

    vk::DeviceSize imageSize = texWidth * texHeight * 4;

    // === Create staging buffer ===
    VkBuffer stagingBuffer;
    VmaAllocation stagingAlloc;
    VmaAllocationInfo stagingInfo;
    Buffer::Create(
        allocator,
        imageSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        VMA_MEMORY_USAGE_CPU_ONLY,
        stagingBuffer,
        stagingAlloc,
        stagingInfo);

    Buffer::UploadData(allocator, stagingAlloc, pixels, static_cast<size_t>(imageSize));
    stbi_image_free(pixels);

    // === Create vk::raii::Image ===
    vk::ImageCreateInfo imageInfo{};
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.extent = vk::Extent3D{ static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = vk::Format::eR8G8B8A8Srgb;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.initialLayout = vk::ImageLayout::eUndefined;
    imageInfo.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
    imageInfo.samples = vk::SampleCountFlagBits::e1;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;

    vk::raii::Image textureImage(device, imageInfo);

    imgResource.image = textureImage;

    // === Allocate memory via VMA and bind ===
    VmaAllocation textureAlloc;
    VmaAllocationInfo textureAllocInfo{};
    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    if (vmaAllocateMemoryForImage(
            allocator,
            static_cast<VkImage>(*textureImage),
            &allocInfo,
            &textureAlloc,
            &textureAllocInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate memory for image!");
    }

    vmaBindImageMemory(allocator, textureAlloc, static_cast<VkImage>(*textureImage));

    // === Transition layout and copy ===
    vk::raii::CommandBuffer commandBuffer = std::move(device.allocateCommandBuffers(
        vk::CommandBufferAllocateInfo{
            *commandPool,
            vk::CommandBufferLevel::ePrimary,
            1
        }).front());

    commandBuffer.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

    ShiftImageLayout(
        *commandBuffer, imgResource,
        vk::ImageLayout::eTransferDstOptimal,
        vk::AccessFlagBits::eNone,
        vk::AccessFlagBits::eTransferWrite,
        vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eTransfer);

    vk::BufferImageCopy copyRegion{};
    copyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    imageInfo.extent = vk::Extent3D{ static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1 };
    copyRegion.imageExtent = imageInfo.extent;


    commandBuffer.copyBufferToImage(
        stagingBuffer,
        *textureImage,
        vk::ImageLayout::eTransferDstOptimal,
        copyRegion);

    ShiftImageLayout(
        *commandBuffer, imgResource,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::AccessFlagBits::eTransferWrite,
        vk::AccessFlagBits::eShaderRead,
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eFragmentShader);

    commandBuffer.end();

    vk::SubmitInfo submitInfo{};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &*commandBuffer;
    graphicsQueue.submit(submitInfo);
    graphicsQueue.waitIdle();

    Buffer::Destroy(allocator, stagingBuffer, stagingAlloc);

    // === Create image view ===
    vk::raii::ImageView imageView = CreateImageView(device, *textureImage, vk::Format::eR8G8B8A8Srgb);

    imgResource.image = std::move(textureImage);
    imgResource.allocation = textureAlloc;
    imgResource.imageView = std::move(imageView);

    return std::move(imgResource);
}


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

void ImageFactory::ShiftImageLayout(const vk::CommandBuffer &commandBuffer, ImageResource& image,
                                    vk::ImageLayout newLayout, vk::AccessFlags srcAccessMask, vk::AccessFlags dstAccessMask,
                                    vk::PipelineStageFlags srcStage, vk::PipelineStageFlags dstStage) {


    vk::ImageSubresourceRange access;
    access.aspectMask = vk::ImageAspectFlagBits::eColor;
    access.baseMipLevel = 0;
    access.levelCount = 1;
    access.baseArrayLayer = 0;
    access.layerCount = 1;

    vk::ImageMemoryBarrier barrier;
    barrier.oldLayout = image.imageLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image.image;
    barrier.subresourceRange = access;

    vk::PipelineStageFlags sourceStage, destinationStage;

    barrier.srcAccessMask = srcAccessMask;
    barrier.dstAccessMask = dstAccessMask;

    image.imageLayout = newLayout;
    
    commandBuffer.pipelineBarrier(
        srcStage, dstStage, vk::DependencyFlags(), nullptr, nullptr, barrier);

}
