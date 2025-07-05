//
// Created by capma on 7/5/2025.
//

#include "DescriptorSetFactory.h"


DescriptorSetFactory & DescriptorSetFactory::AddBinding(uint32_t binding, vk::DescriptorType descriptorType,
                                                        vk::ShaderStageFlags stageFlags, uint32_t descriptorCount) {
    vk::DescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = binding;
    layoutBinding.descriptorType = descriptorType;
    layoutBinding.descriptorCount = descriptorCount;
    layoutBinding.stageFlags = stageFlags;
    layoutBinding.pImmutableSamplers = nullptr;

    m_Bindings.push_back(layoutBinding);
    return *this;
}

DescriptorSetFactory & DescriptorSetFactory::SetFlags(vk::DescriptorSetLayoutCreateFlags flags) {
    m_Flags = flags;
    return *this;
}


