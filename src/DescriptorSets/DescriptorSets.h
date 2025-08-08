//
// Created by capma on 8/7/2025.
//

#ifndef DESCRIPTORSETS_H
#define DESCRIPTORSETS_H
#include <complex.h>
#include <memory>
#include <vulkan/vulkan_raii.hpp>

#include "Buffer.h"
#include "Factories/ImageFactory.h"


class DescriptorSets {

    public:
    DescriptorSets(const vk::raii::Device& Device) : m_Device(Device) {
        CreateDescriptorPool();
    };
    virtual ~DescriptorSets() = default;

    DescriptorSets(const DescriptorSets&) = delete;
    DescriptorSets(DescriptorSets&&) noexcept = delete;
    DescriptorSets& operator=(const DescriptorSets&) = delete;
    DescriptorSets& operator=(DescriptorSets&&) noexcept = delete;

    void CreateDescriptorPool();

    void CreateFrameDescriptorSet(const ::vk::DescriptorSetLayout &FrameLayout,
                                  const std::tuple<vk::ImageView, vk::ImageView, vk::ImageView> & ColorImageViews, const vk::ImageView &DepthImageView,
                                  const BufferInfo &UniformBufferInfo, const BufferInfo &ShadowBufferInfo, const vk::ImageView &ShadowImageView);

    void CreateGlobalDescriptorSet(
        const vk::DescriptorSetLayout &GlobalLayout,
        const vk::Sampler &Sampler, const std::pair<BufferInfo, uint32_t> &PointLights,
        const std::pair<BufferInfo, uint32_t> &DirectionalLights,
        const std::vector<ImageResource> &ImageResources,
        const std::vector<vk::ImageView> &SwapchainImageViews, const vk::Sampler &ShadowSampler);

    // 0 descriptor pool, 1 descriptor set
    std::pair<vk::DescriptorPool, vk::DescriptorSet> GetFrameDescriptorSet(uint32_t CurrentFrame) const { return {*m_DescriptorPool,*(*m_FrameDescriptorSets)[CurrentFrame]}; };
    std::pair<vk::DescriptorPool, vk::DescriptorSet> GetGlobalDescriptorSet(uint32_t CurrentFrame) const { return {*m_DescriptorPool,*(*m_GlobalDescriptorSets)[CurrentFrame]}; };

    // 0 frame 1 global
    std::vector<vk::DescriptorSet> GetDescriptorSets(uint32_t CurrentFrame) const { return { *(*m_FrameDescriptorSets)[CurrentFrame], *(*m_GlobalDescriptorSets)[CurrentFrame] }; };
private:

    const vk::raii::Device& m_Device;
    std::unique_ptr<vk::raii::DescriptorSets> m_FrameDescriptorSets{};
    std::unique_ptr<vk::raii::DescriptorSets> m_GlobalDescriptorSets{};

	std::unique_ptr<vk::raii::DescriptorPool> m_DescriptorPool{};

};



#endif //DESCRIPTORSETS_H
