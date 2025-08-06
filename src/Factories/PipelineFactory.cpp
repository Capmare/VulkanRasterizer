#include "PipelineFactory.h"


PipelineFactory::PipelineFactory(const vk::raii::Device& device)
    : m_Device(device) {}

PipelineFactory& PipelineFactory::SetShaderStages(const std::vector<vk::PipelineShaderStageCreateInfo>& stages) {
    m_ShaderStages = stages;
    return *this;
}

PipelineFactory& PipelineFactory::SetVertexInput(const vk::PipelineVertexInputStateCreateInfo& vertexInput) {
    m_VertexInput = vertexInput;
    return *this;
}

PipelineFactory& PipelineFactory::SetInputAssembly(const vk::PipelineInputAssemblyStateCreateInfo& inputAssembly) {
    m_InputAssembly = inputAssembly;
    return *this;
}

PipelineFactory& PipelineFactory::SetRasterizer(const vk::PipelineRasterizationStateCreateInfo& rasterizer) {
    m_Rasterizer = rasterizer;
    return *this;
}

PipelineFactory& PipelineFactory::SetMultisampling(const vk::PipelineMultisampleStateCreateInfo& multisampling) {
    m_Multisampling = multisampling;
    return *this;
}

PipelineFactory& PipelineFactory::SetColorBlendAttachments(const std::vector<vk::PipelineColorBlendAttachmentState> &attachment) {
    m_ColorBlendAttachment = attachment;
    m_ColorBlending = vk::PipelineColorBlendStateCreateInfo{}
        .setLogicOpEnable(false)
        .setAttachmentCount(static_cast<uint32_t>(attachment.size()))
        .setPAttachments(m_ColorBlendAttachment.data());
    return *this;
}

PipelineFactory& PipelineFactory::SetDynamicStates(const std::vector<vk::DynamicState>& dynamicStates) {
    m_DynamicStates = dynamicStates;
    m_DynamicStateInfo = vk::PipelineDynamicStateCreateInfo{}
        .setDynamicStates(m_DynamicStates);
    return *this;
}

PipelineFactory& PipelineFactory::SetLayout(const vk::PipelineLayout& layout) {
    m_PipelineLayout = &layout;
    return *this;
}

PipelineFactory& PipelineFactory::SetColorFormats(const std::vector<vk::Format> &colorFormats) {
    m_ColorFormat = colorFormats;
    return *this;
}

PipelineFactory& PipelineFactory::SetDepthFormat(vk::Format depthFormat) {
    m_DepthFormat = depthFormat;
    return *this;
}

PipelineFactory & PipelineFactory::SetDepthStencil(
    const vk::PipelineDepthStencilStateCreateInfo &depthStencil) {
    m_DepthStencil = depthStencil;
    return *this;
}

PipelineFactory & PipelineFactory::SetViewportState(
    const vk::PipelineViewportStateCreateInfo &viewportState) {
    m_ViewportState = viewportState;
    return *this;
}

vk::raii::Pipeline PipelineFactory::Build() {
    vk::PipelineRenderingCreateInfoKHR renderingInfo{};
    renderingInfo.setColorAttachmentCount(static_cast<uint32_t>(m_ColorFormat.size()));
    renderingInfo.setPColorAttachmentFormats(m_ColorFormat.data());
    renderingInfo.setDepthAttachmentFormat(m_DepthFormat);

    vk::GraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.setPNext(&renderingInfo);
    pipelineInfo.setStages(m_ShaderStages);
    pipelineInfo.setPVertexInputState(&m_VertexInput);
    pipelineInfo.setPInputAssemblyState(&m_InputAssembly);
    pipelineInfo.setPViewportState(&m_ViewportState);
    pipelineInfo.setPRasterizationState(&m_Rasterizer);
    pipelineInfo.setPMultisampleState(&m_Multisampling);
    pipelineInfo.setPDepthStencilState(&m_DepthStencil);
    pipelineInfo.setPColorBlendState(&m_ColorBlending);
    pipelineInfo.setPDynamicState(&m_DynamicStateInfo);
    pipelineInfo.setLayout(*m_PipelineLayout);
    pipelineInfo.setRenderPass(nullptr); // dynamic rendering
    pipelineInfo.setSubpass(0);

    return {m_Device, nullptr, pipelineInfo};
}
