#include "PipelineFactory.h"


GraphicsPipelineFactory::GraphicsPipelineFactory(vk::raii::Device& device)
    : m_Device(device) {}

GraphicsPipelineFactory& GraphicsPipelineFactory::SetShaderStages(const std::vector<vk::PipelineShaderStageCreateInfo>& stages) {
    m_ShaderStages = stages;
    return *this;
}

GraphicsPipelineFactory& GraphicsPipelineFactory::SetVertexInput(const vk::PipelineVertexInputStateCreateInfo& vertexInput) {
    m_VertexInput = vertexInput;
    return *this;
}

GraphicsPipelineFactory& GraphicsPipelineFactory::SetInputAssembly(const vk::PipelineInputAssemblyStateCreateInfo& inputAssembly) {
    m_InputAssembly = inputAssembly;
    return *this;
}

GraphicsPipelineFactory& GraphicsPipelineFactory::SetRasterizer(const vk::PipelineRasterizationStateCreateInfo& rasterizer) {
    m_Rasterizer = rasterizer;
    return *this;
}

GraphicsPipelineFactory& GraphicsPipelineFactory::SetMultisampling(const vk::PipelineMultisampleStateCreateInfo& multisampling) {
    m_Multisampling = multisampling;
    return *this;
}

GraphicsPipelineFactory& GraphicsPipelineFactory::SetColorBlendAttachment(const vk::PipelineColorBlendAttachmentState& attachment) {
    m_ColorBlendAttachment = attachment;
    m_ColorBlending = vk::PipelineColorBlendStateCreateInfo{}
        .setLogicOpEnable(false)
        .setAttachmentCount(1)
        .setPAttachments(&m_ColorBlendAttachment);
    return *this;
}

GraphicsPipelineFactory& GraphicsPipelineFactory::SetDynamicStates(const std::vector<vk::DynamicState>& dynamicStates) {
    m_DynamicStates = dynamicStates;
    m_DynamicStateInfo = vk::PipelineDynamicStateCreateInfo{}
        .setDynamicStates(m_DynamicStates);
    return *this;
}

GraphicsPipelineFactory& GraphicsPipelineFactory::SetLayout(vk::raii::PipelineLayout& layout) {
    m_PipelineLayout = &layout;
    return *this;
}

GraphicsPipelineFactory& GraphicsPipelineFactory::SetColorFormat(vk::Format colorFormat) {
    m_ColorFormat = colorFormat;
    return *this;
}

GraphicsPipelineFactory& GraphicsPipelineFactory::SetDepthFormat(vk::Format depthFormat) {
    m_DepthFormat = depthFormat;

    m_DepthStencil = vk::PipelineDepthStencilStateCreateInfo{}
        .setDepthTestEnable(true)
        .setDepthWriteEnable(true)
        .setDepthCompareOp(vk::CompareOp::eLess)
        .setDepthBoundsTestEnable(false)
        .setStencilTestEnable(false);

    return *this;
}

GraphicsPipelineFactory & GraphicsPipelineFactory::SetDepthStencil(
    const vk::PipelineDepthStencilStateCreateInfo &depthStencil) {
    m_DepthStencil = depthStencil;
    return *this;
}

GraphicsPipelineFactory & GraphicsPipelineFactory::SetViewportState(
    const vk::PipelineViewportStateCreateInfo &viewportState) {
    m_ViewportState = viewportState;
    return *this;
}

vk::raii::Pipeline GraphicsPipelineFactory::Build() {
    vk::PipelineRenderingCreateInfoKHR renderingInfo{};
    renderingInfo.setColorAttachmentCount(1);
    renderingInfo.setPColorAttachmentFormats(&m_ColorFormat);
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
