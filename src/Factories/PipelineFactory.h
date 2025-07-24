#ifndef PIPELINEFACTORY_H
#define PIPELINEFACTORY_H

#include <vulkan/vulkan.hpp>
#include "vulkan/vulkan_raii.hpp"

class PipelineFactory {
public:
    PipelineFactory(vk::raii::Device& device);

    PipelineFactory& SetShaderStages(const std::vector<vk::PipelineShaderStageCreateInfo>& stages);
    PipelineFactory& SetVertexInput(const vk::PipelineVertexInputStateCreateInfo& vertexInput);
    PipelineFactory& SetInputAssembly(const vk::PipelineInputAssemblyStateCreateInfo& inputAssembly);
    PipelineFactory& SetRasterizer(const vk::PipelineRasterizationStateCreateInfo& rasterizer);
    PipelineFactory& SetMultisampling(const vk::PipelineMultisampleStateCreateInfo& multisampling);
    PipelineFactory& SetColorBlendAttachment(const vk::PipelineColorBlendAttachmentState& attachment);
    PipelineFactory& SetDynamicStates(const std::vector<vk::DynamicState>& dynamicStates);
    PipelineFactory& SetLayout(vk::raii::PipelineLayout& layout);
    PipelineFactory& SetColorFormat(vk::Format colorFormat);
    PipelineFactory& SetDepthFormat(vk::Format depthFormat);
    PipelineFactory& SetDepthStencil(const vk::PipelineDepthStencilStateCreateInfo& depthStencil);
    PipelineFactory& SetViewportState(const vk::PipelineViewportStateCreateInfo& viewportState);

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
    vk::PipelineDepthStencilStateCreateInfo m_DepthStencil{};
    vk::raii::PipelineLayout* m_PipelineLayout = nullptr;
    vk::Format m_ColorFormat{};
    vk::Format m_DepthFormat{};
};

#endif //PIPELINEFACTORY_H
