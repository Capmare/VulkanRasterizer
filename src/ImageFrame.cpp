#include "ImageFrame.h"

#include "Factories/SwapChainFactory.h"

ImageFrameCommandFactory& ImageFrameCommandFactory::Begin(const vk::RenderingInfoKHR& renderingInfo, vk::Image image, const vk::Extent2D& screenSize) {
    m_CommandBuffer.reset();

    m_CommandBuffer.begin(vk::CommandBufferBeginInfo{});

    ImageFactory::ShiftImageLayout(
    *m_CommandBuffer, image,
    vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
    vk::AccessFlagBits::eNone, vk::AccessFlagBits::eColorAttachmentWrite,
    vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput
    );

    m_CommandBuffer.beginRenderingKHR(renderingInfo);

    return *this;
}

ImageFrameCommandFactory& ImageFrameCommandFactory::SetViewport(const vk::Extent2D& screenSize) {
    vk::Viewport viewport{0.0f, 0.0f, static_cast<float>(screenSize.width), static_cast<float>(screenSize.height), 0.0f, 1.0f};
    m_CommandBuffer.setViewport(0, viewport);

    vk::Rect2D scissor{{0, 0}, screenSize};
    m_CommandBuffer.setScissor(0, scissor);

    return *this;
}

ImageFrameCommandFactory& ImageFrameCommandFactory::SetShaders(const std::vector<vk::ShaderEXT>& shaders) {
    vk::ShaderStageFlagBits stages[] = {vk::ShaderStageFlagBits::eVertex, vk::ShaderStageFlagBits::eFragment};
    m_CommandBuffer.bindShadersEXT(stages, shaders);
    return *this;
}

ImageFrameCommandFactory& ImageFrameCommandFactory::BindPipeline(vk::Pipeline pipeline, vk::PipelineLayout layout) {
    m_PipelineLayout = layout;
    m_CommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
    return *this;
}

ImageFrameCommandFactory& ImageFrameCommandFactory::DrawMesh(const Mesh& mesh) {
    m_CommandBuffer.bindVertexBuffers2(0, {mesh.m_VertexBuffer}, mesh.m_VertexOffset);
    m_CommandBuffer.bindIndexBuffer(mesh.m_IndexBuffer, 0, vk::IndexType::eUint16);
    m_CommandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PipelineLayout, 0, {**mesh.DescriptorSet}, nullptr);
    m_CommandBuffer.drawIndexed(mesh.m_IndexCount, 1, 0, 0, 0);
    return *this;
}

ImageFrameCommandFactory& ImageFrameCommandFactory::End(vk::Image image) {
    m_CommandBuffer.endRenderingKHR();

    ImageFactory::ShiftImageLayout(
        *m_CommandBuffer, image,
        vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
        vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eNone,
        vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eBottomOfPipe
    );

    m_CommandBuffer.end();
    return *this;
}

void ImageFrame::RecordCmdBuffer(uint32_t imageIndex, const vk::Extent2D& screenSize, const std::vector<vk::ShaderEXT>& shaders) {
    Build_ColorAttachment(imageIndex);
    Build_RenderingInfo(screenSize);

    ImageFrameCommandFactory builder(m_CommandBuffer);
    builder.Begin(m_RenderingInfo, m_Images[imageIndex], screenSize)
           .SetViewport(screenSize)
           .SetShaders(shaders)
           .BindPipeline(m_Pipeline, m_PipelineLayout)
           .DrawMesh(*m_Mesh)
           .End(m_Images[imageIndex]);
}

void ImageFrame::Build_ColorAttachment(uint32_t index) {
    m_ColorAttachment.setImageView(m_SwapChainFactory->m_ImageViews[index]);
    m_ColorAttachment.setImageLayout(vk::ImageLayout::eAttachmentOptimal);
    m_ColorAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
    m_ColorAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
    m_ColorAttachment.setClearValue(vk::ClearValue({ 0.5f, 0.0f, 0.25f, 1.0f }));
}

void ImageFrame::Build_RenderingInfo(const vk::Extent2D &screenSize) {
    m_RenderingInfo.setFlags(vk::RenderingFlagsKHR());
    m_RenderingInfo.setRenderArea(vk::Rect2D({ 0, 0 }, screenSize));
    m_RenderingInfo.setLayerCount(1);
    m_RenderingInfo.setViewMask(0);
    m_RenderingInfo.setColorAttachmentCount(1);
    m_RenderingInfo.setColorAttachments(m_ColorAttachment);
}
