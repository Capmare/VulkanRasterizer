#ifndef DEPTHIMAGEFACTORY_H
#define DEPTHIMAGEFACTORY_H

#include <vulkan/vulkan_raii.hpp>
#include <vma/vk_mem_alloc.h>
#include <deque>
#include <functional>

class DepthImageFactory {
public:
    DepthImageFactory(vk::raii::Device& device, VmaAllocator allocator, vk::Extent2D extent, std::deque<std::function<void(VmaAllocator)>>& deletionQueue);

    vk::Format GetFormat() const { return m_Format; }
    vk::Image GetImage() const { return m_Image; }
    vk::ImageView GetView() const { return *m_ImageView; }

private:
    void CreateDepthResources(vk::Extent2D extent, std::deque<std::function<void(VmaAllocator)>>& deletionQueue);

    vk::raii::Device* m_Device;
    VmaAllocator m_Allocator;

    vk::Format m_Format = vk::Format::eD32Sfloat;

    vk::Image m_Image{nullptr};
    vk::raii::ImageView m_ImageView{nullptr};

    // Raw handle & VMA allocation for manual VMA cleanup
    VkImage m_RawImage = VK_NULL_HANDLE;
    VmaAllocation m_Allocation = VK_NULL_HANDLE;
};

#endif // DEPTHIMAGEFACTORY_H
