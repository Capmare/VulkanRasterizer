#ifndef SWAPCHAINFACTORY_H
#define SWAPCHAINFACTORY_H

#include "vulkan/vulkan_raii.hpp"

struct SurfaceInfo {
    vk::SurfaceCapabilitiesKHR Capabilities;
    std::vector<vk::SurfaceFormatKHR> Formats;
    std::vector<vk::PresentModeKHR> PresentModes;
};

class SwapChainFactory {
public:
    SwapChainFactory() = default;
    virtual ~SwapChainFactory() = default;

    SwapChainFactory(const SwapChainFactory&) = delete;
    SwapChainFactory(SwapChainFactory&&) noexcept = delete;
    SwapChainFactory& operator=(const SwapChainFactory&) = delete;
    SwapChainFactory& operator=(SwapChainFactory&&) noexcept = delete;

    vk::raii::SwapchainKHR Build_SwapChain(
        const vk::raii::Device& logicalDevice,
        const vk::raii::PhysicalDevice& physicalDevice,
        vk::SurfaceKHR surface,
        uint32_t width,
        uint32_t height);

    vk::SurfaceFormatKHR Format{};
    vk::Extent2D Extent{};
    std::vector<vk::raii::ImageView> m_ImageViews;

    vk::raii::Image m_DepthImage{nullptr};
    vk::raii::DeviceMemory m_DepthImageMemory{nullptr};
    vk::raii::ImageView m_DepthImageView{nullptr};
    vk::Format m_DepthFormat{};

private:
    SurfaceInfo Get_Surface_Info(const vk::raii::PhysicalDevice& physicalDevice, const vk::SurfaceKHR& surface);
    vk::Extent2D ChooseExtent(uint32_t width, uint32_t height, vk::SurfaceCapabilitiesKHR capabilities);
    vk::PresentModeKHR ChoosePresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes);
    vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);
    vk::Format FindDepthFormat(const vk::raii::PhysicalDevice& physicalDevice);

    uint32_t ImageCount{};

};

#endif //SWAPCHAINFACTORY_H
