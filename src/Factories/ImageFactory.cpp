//
// Created by capma on 6/24/2025.
//

#include "ImageFactory.h"

#include "Buffer.h"
#define STB_IMAGE_IMPLEMENTATION
#include <filesystem>
#include <iostream>

#include "stb_image.h"

ImageResource ImageFactory::LoadTexture(Buffer* buff,
    const std::string &filename,
    const vk::raii::Device &device,
    VmaAllocator allocator,
    const vk::raii::CommandPool &commandPool,
    const vk::raii::Queue &graphicsQueue,
    vk::Format ColorFormat,
    vk::ImageAspectFlagBits aspect,
    ResourceTracker* AllocationTracker)
{
    ImageResource imgResource{};
    imgResource.imageAspectFlags = aspect;
    imgResource.format = ColorFormat;

    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(filename.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    std::filesystem::path absPath = std::filesystem::absolute(filename);

    if (!pixels) {
        throw std::runtime_error("Failed to load texture image: " + absPath.string());
    }

    if (texWidth == 0 || texHeight == 0) {
        stbi_image_free(pixels);
        throw std::runtime_error("Loaded texture has zero size: " + absPath.string());
    }

    imgResource.extent = vk::Extent2D(texWidth, texHeight);

    vk::DeviceSize imageSize = texWidth * texHeight * 4;

    // Create a staging buffer, mapped to CPU memory, for uploading texture data
    BufferInfo StagingBuffer = buff->CreateMapped(
        allocator,
        imageSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        VMA_MEMORY_USAGE_CPU_ONLY,
        0,
        AllocationTracker,
        filename + "StagingBuffer"
    );

    // Upload pixel data into staging buffer
    Buffer::UploadData(StagingBuffer, pixels, static_cast<size_t>(imageSize));
    stbi_image_free(pixels);

    // === Create VkImage via VMA ===
    vk::ImageCreateInfo imageInfo{};
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.extent = vk::Extent3D{ static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = ColorFormat;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.initialLayout = vk::ImageLayout::eUndefined;
    imageInfo.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
    imageInfo.samples = vk::SampleCountFlagBits::e1;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;

    VkImageCreateInfo imgInfo = static_cast<VkImageCreateInfo>(imageInfo);

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

    VkImage rawImage = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo allocationInfo{};

    VkResult result = vmaCreateImage(
        allocator,
        &imgInfo,
        &allocCreateInfo,
        &rawImage,
        &allocation,
        &allocationInfo
    );

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image with VMA.");
    }

    imgResource.image = rawImage;
    imgResource.allocation = allocation;

    // === Transition image layout and copy from staging buffer ===
    vk::raii::CommandBuffer commandBuffer = std::move(device.allocateCommandBuffers(
        vk::CommandBufferAllocateInfo{
            *commandPool,
            vk::CommandBufferLevel::ePrimary,
            1
        })->front());

    commandBuffer.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

    ShiftImageLayout(
        *commandBuffer, imgResource,
        vk::ImageLayout::eUndefined,
        vk::AccessFlags(),
        vk::AccessFlagBits::eTransferWrite,
        vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eTransfer);

    ShiftImageLayout(
        *commandBuffer, imgResource,
        vk::ImageLayout::eTransferDstOptimal,
        vk::AccessFlags(),
        vk::AccessFlagBits::eTransferWrite,
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eTransfer);

    vk::BufferImageCopy copyRegion{};
    copyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent = vk::Extent3D{ static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1 };
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;

    commandBuffer.copyBufferToImage(
        StagingBuffer.m_Buffer,
        imgResource.image,
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

    Buffer::Destroy(allocator, StagingBuffer.m_Buffer, StagingBuffer.m_Allocation, AllocationTracker);

    imgResource.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    return imgResource;
}

ImageResource ImageFactory::LoadTextureFromMemory(
    Buffer* buff,
    const unsigned char* data,
    size_t dataSize,
    const vk::raii::Device& device,
    VmaAllocator allocator,
    const vk::raii::CommandPool& commandPool,
    const vk::raii::Queue& graphicsQueue,
    vk::Format ColorFormat,
    vk::ImageAspectFlagBits aspect,
    ResourceTracker* AllocationTracker)
{
    ImageResource imgResource{};
    imgResource.imageAspectFlags = aspect;
    imgResource.format = ColorFormat;

    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load_from_memory(data, static_cast<int>(dataSize), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    if (!pixels) {
        throw std::runtime_error("Failed to load texture from memory");
    }

    if (texWidth == 0 || texHeight == 0) {
        stbi_image_free(pixels);
        throw std::runtime_error("Loaded texture from memory has zero size");
    }

    imgResource.extent = vk::Extent2D(texWidth, texHeight);

    vk::DeviceSize imageSize = texWidth * texHeight * 4;

    // === Create staging buffer ===
    BufferInfo StagingBuffer = buff->CreateMapped(
        allocator,
        imageSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        VMA_MEMORY_USAGE_CPU_ONLY,
        0,
        AllocationTracker,
        "MemoryTextureStagingBuffer"
    );

    // Upload pixel data to staging buffer
    Buffer::UploadData(StagingBuffer, pixels, static_cast<size_t>(imageSize));
    stbi_image_free(pixels);

    // === Create VkImage via VMA ===
    vk::ImageCreateInfo imageInfo{};
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.extent = vk::Extent3D{ static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = ColorFormat;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.initialLayout = vk::ImageLayout::eUndefined;
    imageInfo.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
    imageInfo.samples = vk::SampleCountFlagBits::e1;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;

    VkImageCreateInfo imgInfo = static_cast<VkImageCreateInfo>(imageInfo);

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

    VkImage rawImage = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo allocationInfo{};

    VkResult result = vmaCreateImage(
        allocator,
        &imgInfo,
        &allocCreateInfo,
        &rawImage,
        &allocation,
        &allocationInfo
    );

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image from memory with VMA.");
    }

    imgResource.image = rawImage;
    imgResource.allocation = allocation;

    // === Transition image layout and copy ===
    vk::raii::CommandBuffer commandBuffer = std::move(device.allocateCommandBuffers(
        vk::CommandBufferAllocateInfo{
            *commandPool,
            vk::CommandBufferLevel::ePrimary,
            1
        })->front());

    commandBuffer.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

    // Transition UNDEFINED -> TRANSFER_DST
    ShiftImageLayout(
        *commandBuffer, imgResource,
        vk::ImageLayout::eUndefined,
        vk::AccessFlags(),
        vk::AccessFlagBits::eTransferWrite,
        vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eTransfer);

    vk::BufferImageCopy copyRegion{};
    copyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent = vk::Extent3D{ static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1 };
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;

    // Copy staging buffer -> image
    commandBuffer.copyBufferToImage(
        StagingBuffer.m_Buffer,
        imgResource.image,
        vk::ImageLayout::eTransferDstOptimal,
        copyRegion);

    // Transition TRANSFER_DST -> SHADER_READ_ONLY
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

    // Destroy staging buffer
    Buffer::Destroy(allocator, StagingBuffer.m_Buffer, StagingBuffer.m_Allocation, AllocationTracker);

    imgResource.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    return imgResource;
}



VkImageView ImageFactory::CreateImageView(const vk::raii::Device &device, vk::Image Image, vk::Format Format, vk::ImageAspectFlags Aspect, ResourceTracker* ResourceTracker, const std::string& Name) {

    VkImageViewCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = Image;
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = static_cast<VkFormat>(Format);

    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    createInfo.subresourceRange.aspectMask = static_cast<VkImageAspectFlags>(Aspect);
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    VkResult result = vkCreateImageView(*device, &createInfo, NULL, &imageView);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Failed to create VkImageView (error %d)\n", result);
        imageView = VK_NULL_HANDLE;
    }

    ResourceTracker->TrackImageView(imageView,Name);

    return imageView;
}

void ImageFactory::CreateImage(VmaAllocator Allocator, ImageResource &Image, vk::ImageCreateInfo imageInfo) {


    VkImage img = VK_NULL_HANDLE;
    VkImageCreateInfo imgInfo{static_cast<VkImageCreateInfo>(imageInfo)};
    VmaAllocationCreateInfo vmaAllocationCreateInfo{};

    vmaCreateImage(Allocator, &imgInfo, &vmaAllocationCreateInfo, &img, &Image.allocation, nullptr);

    Image.image = img;
}

void ImageFactory::ShiftImageLayout(const vk::CommandBuffer &commandBuffer, ImageResource& image,
                                    vk::ImageLayout newLayout, vk::AccessFlags srcAccessMask, vk::AccessFlags dstAccessMask,
                                    vk::PipelineStageFlags srcStage, vk::PipelineStageFlags dstStage) {


    vk::ImageSubresourceRange access;
    access.aspectMask = image.imageAspectFlags;
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
