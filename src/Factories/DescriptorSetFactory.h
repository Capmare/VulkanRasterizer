//
// Created by capma on 7/5/2025.
//

#ifndef DESCRIPTORSETFACTORY_H
#define DESCRIPTORSETFACTORY_H


#include <vulkan/vulkan.hpp>
#include <vector>

#include "vulkan/vulkan_raii.hpp"


class DescriptorSetFactory {
public:

    DescriptorSetFactory(vk::raii::Device& device) : m_Device(device) {};
    virtual ~DescriptorSetFactory() = default;

    DescriptorSetFactory(const DescriptorSetFactory&) = delete;
    DescriptorSetFactory(DescriptorSetFactory&&) noexcept = delete;
    DescriptorSetFactory& operator=(const DescriptorSetFactory&) = delete;
    DescriptorSetFactory& operator=(DescriptorSetFactory&&) noexcept = delete;


    DescriptorSetFactory& AddBinding(
            uint32_t binding,
            vk::DescriptorType descriptorType,
            vk::ShaderStageFlags stageFlags,
            uint32_t descriptorCount = 1,
            const vk::Sampler* immutableSamplers = nullptr
        );

    DescriptorSetFactory& SetFlags(vk::DescriptorSetLayoutCreateFlags flags);

    vk::raii::DescriptorSetLayout Build() const {
        vk::DescriptorSetLayoutCreateInfo layoutInfo{
            m_Flags,
            static_cast<uint32_t>(m_Bindings.size()),
            m_Bindings.data()
        };

        return vk::raii::DescriptorSetLayout(m_Device, layoutInfo);
    }

    void ResetFactory() {
        m_Bindings.clear();
        m_Flags = {};
    }
private:
    vk::raii::Device& m_Device;
    std::vector<vk::DescriptorSetLayoutBinding> m_Bindings;
    vk::DescriptorSetLayoutCreateFlags m_Flags = {};
};



#endif //DESCRIPTORSETFACTORY_H
