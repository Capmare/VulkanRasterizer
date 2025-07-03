//
// Created by capma on 6/24/2025.
//

#include "SwapChainFactory.h"

#include <iostream>
#include <ostream>

#include "ImageFactory.h"

vk::raii::SwapchainKHR SwapChainFactory::Build_SwapChain(const vk::raii::Device &logicalDevice,
                                                         const vk::raii::PhysicalDevice &physicalDevice, vk::SurfaceKHR surface, uint32_t width, uint32_t height) {

    SurfaceInfo SurfaceInfo = Get_Surface_Info(physicalDevice,surface);
    Format = ChooseSwapSurfaceFormat(SurfaceInfo.Formats);
    vk::PresentModeKHR PresentMode = ChoosePresentMode(SurfaceInfo.PresentModes);
    Extent = ChooseExtent(width,height,SurfaceInfo.Capabilities);

    ImageCount = SurfaceInfo.Capabilities.minImageCount + 1;
    if (SurfaceInfo.Capabilities.maxImageCount > 0 && ImageCount > SurfaceInfo.Capabilities.maxImageCount) {
        ImageCount = SurfaceInfo.Capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR SwapChainCreateInfo{
        vk::SwapchainCreateFlagsKHR{},
        surface,
        ImageCount,
        Format.format,
        Format.colorSpace,
        Extent,
        1,
        vk::ImageUsageFlagBits::eColorAttachment,
        vk::SharingMode::eExclusive
    };

    SwapChainCreateInfo.preTransform = SurfaceInfo.Capabilities.currentTransform;
    SwapChainCreateInfo.presentMode = PresentMode;
    SwapChainCreateInfo.clipped = VK_TRUE;
    SwapChainCreateInfo.oldSwapchain = vk::SwapchainKHR(nullptr);

    auto swapchain = logicalDevice.createSwapchainKHR(SwapChainCreateInfo);
    std::vector<vk::Image> swapchainImages = swapchain.getImages();
    for (uint32_t i = 0; i < ImageCount; i++) {
        vk::raii::ImageView ImageView{ImageFactory::CreateImageView(logicalDevice,swapchainImages[i],Format.format)};
        m_ImageViews.push_back(std::move(ImageView));
    }

    return swapchain;
}

SurfaceInfo SwapChainFactory::Get_Surface_Info(const vk::raii::PhysicalDevice &physicalDevice, const vk::SurfaceKHR &surface) {
    SurfaceInfo surfaceInfo = {};
    surfaceInfo.Formats = physicalDevice.getSurfaceFormatsKHR(surface);
    surfaceInfo.PresentModes = physicalDevice.getSurfacePresentModesKHR(surface);
    surfaceInfo.Capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);

    std::cout << surfaceInfo.Formats.data() << std::endl;
    std::cout << surfaceInfo.PresentModes.data() << std::endl;

    return surfaceInfo;
}

vk::Extent2D SwapChainFactory::ChooseExtent(uint32_t width, uint32_t height, vk::SurfaceCapabilitiesKHR capabilities) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    vk::Extent2D swapChainExtent = {};
    swapChainExtent.width = std::min(capabilities.minImageExtent.width, std::max(capabilities.maxImageExtent.width,width));
    swapChainExtent.height = std::min(capabilities.minImageExtent.height, std::max(capabilities.maxImageExtent.height,height));

    return swapChainExtent;
}

vk::PresentModeKHR SwapChainFactory::ChoosePresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
            return availablePresentMode;
        }
    }

    return vk::PresentModeKHR::eFifo;
}

vk::SurfaceFormatKHR SwapChainFactory::ChooseSwapSurfaceFormat(
    const std::vector<vk::SurfaceFormatKHR> &availableFormats) {

    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == vk::Format::eB8G8R8A8Unorm && availableFormat.colorSpace == vk::ColorSpaceKHR::eVkColorspaceSrgbNonlinear) {
            return availableFormat;
        }
    }

    return availableFormats[0];


}
