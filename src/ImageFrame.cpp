#include "ImageFrame.h"


void ImageFrame::RecordCmdBuffer(uint32_t ImageIndex, const vk::Extent2D &ScreenSize, const std::vector<vk::ShaderEXT> &Shaders) {
    m_CommandBuffer.reset();
    Build_ColorAttachment(ImageIndex);
    Build_RenderingInfo(ScreenSize);


    vk::CommandBufferBeginInfo beginInfo{};
    m_CommandBuffer.begin(beginInfo);

    ImageFactory::ShiftImageLayout(
        *m_CommandBuffer, m_Images[ImageIndex], vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal, vk::AccessFlagBits::eNone,
        vk::AccessFlagBits::eColorAttachmentWrite, vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eColorAttachmentOutput
    );

    m_CommandBuffer.beginRenderingKHR(m_RenderingInfo);

    vk::Viewport viewport(0.0f, 0.0f, static_cast<float>(ScreenSize.width), static_cast<float>(ScreenSize.height), 0.0f, 1.0f);
    m_CommandBuffer.setViewportWithCount(viewport);

    vk::Rect2D scissor({ 0, 0 }, ScreenSize);
    m_CommandBuffer.setScissorWithCount(scissor);

    m_CommandBuffer.setRasterizerDiscardEnable(VK_FALSE);
    m_CommandBuffer.setPolygonModeEXT(vk::PolygonMode::eFill);
    m_CommandBuffer.setRasterizationSamplesEXT(vk::SampleCountFlagBits::e1);
    uint32_t sampleMask = 1;
    m_CommandBuffer.setSampleMaskEXT(vk::SampleCountFlagBits::e1, sampleMask);
    m_CommandBuffer.setAlphaToCoverageEnableEXT(VK_FALSE);
    m_CommandBuffer.setCullMode(vk::CullModeFlagBits::eNone);
    m_CommandBuffer.setDepthTestEnable(VK_FALSE);
    m_CommandBuffer.setDepthWriteEnable(VK_FALSE);
    m_CommandBuffer.setDepthBiasEnable(VK_FALSE);
    m_CommandBuffer.setStencilTestEnable(VK_FALSE);
    m_CommandBuffer.setPrimitiveTopology(vk::PrimitiveTopology::eTriangleList);
    m_CommandBuffer.setPrimitiveRestartEnable(VK_FALSE);
    uint32_t colorBlendEnable = 0;
    m_CommandBuffer.setColorBlendEnableEXT(0, colorBlendEnable);
    vk::ColorBlendEquationEXT equation;
    equation.colorBlendOp = vk::BlendOp::eAdd;
    equation.dstColorBlendFactor = vk::BlendFactor::eZero;
    equation.srcColorBlendFactor = vk::BlendFactor::eOne;
    m_CommandBuffer.setColorBlendEquationEXT(0, equation);
    vk::ColorComponentFlags colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    m_CommandBuffer.setColorWriteMaskEXT(0, colorWriteMask);

    std::vector<vk::VertexInputBindingDescription2EXT> vertexBindings;
    std::vector<vk::VertexInputAttributeDescription2EXT> vertexAttributes;
    m_CommandBuffer.setVertexInputEXT(vertexBindings, vertexAttributes);

    vk::ShaderStageFlagBits stages[2] = { vk::ShaderStageFlagBits::eVertex, vk::ShaderStageFlagBits::eFragment };
    m_CommandBuffer.bindShadersEXT(stages, Shaders);
    m_CommandBuffer.draw(3, 1, 0, 0);

    m_CommandBuffer.endRenderingKHR();

    ImageFactory::ShiftImageLayout(
        *m_CommandBuffer, m_Images[ImageIndex],
        vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
        vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eNone,
        vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eBottomOfPipe
    );

    m_CommandBuffer.end();

}
