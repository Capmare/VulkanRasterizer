#include "SwapChainFactory.h"

vk::Format SwapChainFactory::FindDepthFormat(const vk::raii::PhysicalDevice& physicalDevice) {
    const std::vector<vk::Format> candidates = {
        vk::Format::eD32Sfloat,
        vk::Format::eD32SfloatS8Uint,
        vk::Format::eD24UnormS8Uint
    };

    for (vk::Format format : candidates) {
        auto props = physicalDevice.getFormatProperties(format);
        if (props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment) {
            return format;
        }
    }
    throw std::runtime_error("Failed to find supported depth format");
}

SurfaceInfo SwapChainFactory::Get_Surface_Info(const vk::raii::PhysicalDevice& physicalDevice, const vk::SurfaceKHR& surface) {
    SurfaceInfo info;
    info.Capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
    info.Formats = physicalDevice.getSurfaceFormatsKHR(surface);
    info.PresentModes = physicalDevice.getSurfacePresentModesKHR(surface);
    return info;
}

vk::Extent2D SwapChainFactory::ChooseExtent(uint32_t width, uint32_t height, vk::SurfaceCapabilitiesKHR capabilities) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        vk::Extent2D actualExtent = { width, height };
        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return actualExtent;
    }
}

vk::PresentModeKHR SwapChainFactory::ChoosePresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes) {
    for (const auto& mode : availablePresentModes) {
        if (mode == vk::PresentModeKHR::eMailbox) return mode;
    }
    return vk::PresentModeKHR::eFifo;
}

vk::SurfaceFormatKHR SwapChainFactory::ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
    for (const auto& format : availableFormats) {
        if (format.format == vk::Format::eB8G8R8A8Unorm && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            return format;
        }
    }
    return availableFormats.front();
}

vk::raii::SwapchainKHR SwapChainFactory::Build_SwapChain(
    const vk::raii::Device& logicalDevice,
    const vk::raii::PhysicalDevice& physicalDevice,
    vk::SurfaceKHR surface,
    uint32_t width,
    uint32_t height) {

    auto surfaceInfo = Get_Surface_Info(physicalDevice, surface);
    Format = ChooseSwapSurfaceFormat(surfaceInfo.Formats);
    Extent = ChooseExtent(width, height, surfaceInfo.Capabilities);
    auto presentMode = ChoosePresentMode(surfaceInfo.PresentModes);

    ImageCount = std::clamp(surfaceInfo.Capabilities.minImageCount + 1,
                            surfaceInfo.Capabilities.minImageCount,
                            surfaceInfo.Capabilities.maxImageCount);

    vk::SwapchainCreateInfoKHR createInfo{};
    createInfo.surface = surface;
    createInfo.minImageCount = ImageCount;
    createInfo.imageFormat = Format.format;
    createInfo.imageColorSpace = Format.colorSpace;
    createInfo.imageExtent = Extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

    createInfo.imageSharingMode = vk::SharingMode::eExclusive;
    createInfo.preTransform = surfaceInfo.Capabilities.currentTransform;
    createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    vk::raii::SwapchainKHR swapchain(logicalDevice, createInfo);

    auto swapImages = swapchain.getImages();
    m_ImageViews.clear();
    for (auto& image : swapImages) {
        vk::ImageViewCreateInfo viewInfo{};
        viewInfo.image = image;
        viewInfo.viewType = vk::ImageViewType::e2D;
        viewInfo.format = Format.format;
        viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        m_ImageViews.emplace_back(logicalDevice, viewInfo);
    }

    m_DepthFormat = FindDepthFormat(physicalDevice);

    vk::ImageCreateInfo depthImageInfo{};
    depthImageInfo.imageType = vk::ImageType::e2D;
    depthImageInfo.extent.width = Extent.width;
    depthImageInfo.extent.height = Extent.height;
    depthImageInfo.extent.depth = 1;
    depthImageInfo.mipLevels = 1;
    depthImageInfo.arrayLayers = 1;
    depthImageInfo.format = m_DepthFormat;
    depthImageInfo.tiling = vk::ImageTiling::eOptimal;
    depthImageInfo.initialLayout = vk::ImageLayout::eUndefined;
    depthImageInfo.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
    depthImageInfo.samples = vk::SampleCountFlagBits::e1;
    depthImageInfo.sharingMode = vk::SharingMode::eExclusive;

    m_DepthImage = vk::raii::Image(logicalDevice, depthImageInfo);
    auto memReqs = m_DepthImage.getMemoryRequirements();

    vk::MemoryAllocateInfo allocInfo{};
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = 0;

    auto memProps = physicalDevice.getMemoryProperties();
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((memReqs.memoryTypeBits & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal)) {
            allocInfo.memoryTypeIndex = i;
            break;
        }
    }

    m_DepthImageMemory = vk::raii::DeviceMemory(logicalDevice, allocInfo);
    m_DepthImage.bindMemory(*m_DepthImageMemory, 0);

    vk::ImageViewCreateInfo depthViewInfo{};
    depthViewInfo.image = *m_DepthImage;
    depthViewInfo.viewType = vk::ImageViewType::e2D;
    depthViewInfo.format = m_DepthFormat;
    depthViewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
    depthViewInfo.subresourceRange.baseMipLevel = 0;
    depthViewInfo.subresourceRange.levelCount = 1;
    depthViewInfo.subresourceRange.baseArrayLayer = 0;
    depthViewInfo.subresourceRange.layerCount = 1;

    m_DepthImageView = vk::raii::ImageView(logicalDevice, depthViewInfo);

    return swapchain;
}
