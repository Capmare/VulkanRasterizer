//
// Created by capma on 7/4/2025.
//

#ifndef PIPELINEFACTORY_H
#define PIPELINEFACTORY_H

#include <vulkan/vulkan.hpp>

#include "vulkan/vulkan_raii.hpp"


class GraphicsPipelineFactory {
public:
    GraphicsPipelineFactory(vk::raii::Device& device);

    GraphicsPipelineFactory& SetShaderStages(const std::vector<vk::PipelineShaderStageCreateInfo>& stages);
    GraphicsPipelineFactory& SetVertexInput(const vk::PipelineVertexInputStateCreateInfo& vertexInput);
    GraphicsPipelineFactory& SetInputAssembly(const vk::PipelineInputAssemblyStateCreateInfo& inputAssembly);
    GraphicsPipelineFactory& SetRasterizer(const vk::PipelineRasterizationStateCreateInfo& rasterizer);
    GraphicsPipelineFactory& SetMultisampling(const vk::PipelineMultisampleStateCreateInfo& multisampling);
    GraphicsPipelineFactory& SetColorBlendAttachment(const vk::PipelineColorBlendAttachmentState& attachment);
    GraphicsPipelineFactory& SetDynamicStates(const std::vector<vk::DynamicState>& dynamicStates);
    GraphicsPipelineFactory& SetLayout(vk::raii::PipelineLayout& layout);
    GraphicsPipelineFactory& SetColorFormat(vk::Format colorFormat);

    vk::raii::Pipeline Build();

private:
    vk::raii::Device& m_Device;

    std::vector<vk::PipelineShaderStageCreateInfo> m_ShaderStages;
    vk::PipelineVertexInputStateCreateInfo m_VertexInput{};
    vk::PipelineInputAssemblyStateCreateInfo m_InputAssembly{};
    vk::PipelineRasterizationStateCreateInfo m_Rasterizer{};
    vk::PipelineMultisampleStateCreateInfo m_Multisampling{};
    vk::PipelineColorBlendAttachmentState m_ColorBlendAttachment{};
    std::vector<vk::DynamicState> m_DynamicStates;
    vk::PipelineDynamicStateCreateInfo m_DynamicStateInfo{};
    vk::PipelineColorBlendStateCreateInfo m_ColorBlending{};
    vk::PipelineViewportStateCreateInfo m_ViewportState{};
    vk::raii::PipelineLayout* m_PipelineLayout = nullptr;
    vk::Format m_ColorFormat{};
};




#endif //PIPELINEFACTORY_H
