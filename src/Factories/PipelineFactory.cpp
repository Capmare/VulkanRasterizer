//
// Created by capma on 7/4/2025.
//

#include "PipelineFactory.h"


GraphicsPipelineFactory::GraphicsPipelineFactory(vk::raii::Device &device) : m_Device(device) {
}

GraphicsPipelineFactory & GraphicsPipelineFactory::SetShaderStages(
    const std::vector<vk::PipelineShaderStageCreateInfo> &stages) {
    m_ShaderStages = stages;
    return *this;
}

GraphicsPipelineFactory& GraphicsPipelineFactory::SetVertexInput(
    const vk::PipelineVertexInputStateCreateInfo& vertexInput) {
    m_VertexInput = vertexInput;
    return *this;
}

GraphicsPipelineFactory& GraphicsPipelineFactory::SetInputAssembly(
    const vk::PipelineInputAssemblyStateCreateInfo& inputAssembly) {
    m_InputAssembly = inputAssembly;
    return *this;
}

GraphicsPipelineFactory& GraphicsPipelineFactory::SetRasterizer(
    const vk::PipelineRasterizationStateCreateInfo& rasterizer) {
    m_Rasterizer = rasterizer;
    return *this;
}

GraphicsPipelineFactory& GraphicsPipelineFactory::SetMultisampling(
    const vk::PipelineMultisampleStateCreateInfo& multisampling) {
    m_Multisampling = multisampling;
    return *this;
}

GraphicsPipelineFactory& GraphicsPipelineFactory::SetColorBlendAttachment(
    const vk::PipelineColorBlendAttachmentState& attachment) {
    m_ColorBlendAttachment = attachment;
    return *this;
}

GraphicsPipelineFactory& GraphicsPipelineFactory::SetDynamicStates(
    const std::vector<vk::DynamicState>& dynamicStates) {
    m_DynamicStates = dynamicStates;
    return *this;
}

GraphicsPipelineFactory& GraphicsPipelineFactory::SetLayout(
    vk::raii::PipelineLayout& layout) {
    m_PipelineLayout = &layout;
    return *this;
}

GraphicsPipelineFactory& GraphicsPipelineFactory::SetColorFormat(
    vk::Format colorFormat) {
    m_ColorFormat = colorFormat;
    return *this;
}


vk::raii::Pipeline GraphicsPipelineFactory::Build() {
    m_DynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(m_DynamicStates.size());
    m_DynamicStateInfo.pDynamicStates = m_DynamicStates.data();

    m_ColorBlending.attachmentCount = 1;
    m_ColorBlending.pAttachments = &m_ColorBlendAttachment;

    m_ViewportState.viewportCount = 1;
    m_ViewportState.scissorCount = 1;

    vk::PipelineRenderingCreateInfoKHR renderingInfo{};
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &m_ColorFormat;

    vk::GraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.pNext = &renderingInfo;
    pipelineInfo.stageCount = static_cast<uint32_t>(m_ShaderStages.size());
    pipelineInfo.pStages = m_ShaderStages.data();
    pipelineInfo.pVertexInputState = &m_VertexInput;
    pipelineInfo.pInputAssemblyState = &m_InputAssembly;
    pipelineInfo.pViewportState = &m_ViewportState;
    pipelineInfo.pRasterizationState = &m_Rasterizer;
    pipelineInfo.pMultisampleState = &m_Multisampling;
    pipelineInfo.pColorBlendState = &m_ColorBlending;
    pipelineInfo.pDynamicState = &m_DynamicStateInfo;
    pipelineInfo.layout = *m_PipelineLayout;
    pipelineInfo.renderPass = nullptr;

    return m_Device.createGraphicsPipeline(nullptr, pipelineInfo);
}
