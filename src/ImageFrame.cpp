// ImageFrame.cpp
#include "ImageFrame.h"

#include <chrono>

#include "Factories/SwapChainFactory.h"
#include "stb_image.h"

ImageFrameCommandFactory& ImageFrameCommandFactory::Begin(const vk::RenderingInfoKHR& renderingInfo, ImageResource& image) {
    m_CommandBuffer.reset();

    m_CommandBuffer.begin(vk::CommandBufferBeginInfo{});

    ImageFactory::ShiftImageLayout(
        *m_CommandBuffer, image,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::AccessFlagBits::eNone, vk::AccessFlagBits::eColorAttachmentWrite,
        vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput
    );

    m_CommandBuffer.beginRenderingKHR(renderingInfo);

    return *this;
}

ImageFrameCommandFactory& ImageFrameCommandFactory::SetViewport(const vk::Extent2D& screenSize) {
    std::vector<vk::Viewport> viewports = {
        vk::Viewport{0.0f, 0.0f, float(screenSize.width), float(screenSize.height), 0.0f, 1.0f}
    };
    m_CommandBuffer.setViewportWithCountEXT(viewports);

    std::vector<vk::Rect2D> scissors = {
        vk::Rect2D{{0, 0}, screenSize}
    };
    m_CommandBuffer.setScissorWithCountEXT(scissors);

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

ImageFrameCommandFactory& ImageFrameCommandFactory::DrawMesh(const Mesh& mesh, const vk::raii::DescriptorSets& descriptorSets) {
    m_CommandBuffer.bindVertexBuffers2(0, {mesh.m_VertexBuffer}, mesh.m_VertexOffset);
    m_CommandBuffer.bindIndexBuffer(mesh.m_IndexBuffer, 0, vk::IndexType::eUint16);

    std::vector<vk::DescriptorSet> rawDescriptorSets{};
    for (auto& ds: descriptorSets) {
        rawDescriptorSets.emplace_back(ds);
    }

    m_CommandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PipelineLayout, 0, rawDescriptorSets, nullptr);
    m_CommandBuffer.drawIndexed(mesh.m_IndexCount, 1, 0, 0, 0);
    return *this;
}

ImageFrameCommandFactory& ImageFrameCommandFactory::End(ImageResource& image) {
    m_CommandBuffer.endRenderingKHR();

    ImageFactory::ShiftImageLayout(
        *m_CommandBuffer, image,
        vk::ImageLayout::ePresentSrcKHR,
        vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eNone,
        vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eBottomOfPipe
    );

    m_CommandBuffer.end();
    return *this;
}

void ImageFrame::RecordCmdBuffer(uint32_t imageIndex, const vk::Extent2D& screenSize, const std::vector<vk::ShaderEXT>& shaders) {
    //Build_ColorAttachment(imageIndex);
    //Build_RenderingInfo(screenSize);
    //
    //ImageFrameCommandFactory builder(m_CommandBuffer);
    //builder.Begin(m_RenderingInfo, m_Images[imageIndex])
    //       .SetViewport(screenSize)
    //       .SetShaders(shaders)
    //       .BindPipeline(m_Pipeline, m_PipelineLayout)
    //       .DrawMesh(*m_Mesh, TODO)
    //       .End(m_Images[imageIndex]);
}

void ImageFrame::SetDepthImage(vk::ImageView depthView, vk::Format format) {
    m_DepthImageView = depthView;
    m_DepthFormat = format;

    m_DepthAttachment.setImageView(m_DepthImageView);
    m_DepthAttachment.setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
    m_DepthAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
    m_DepthAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
    m_DepthAttachment.setClearValue(vk::ClearValue().setDepthStencil({1.0f, 0}));
}

void ImageFrame::Build_ColorAttachment(uint32_t index) {
    m_ColorAttachment.setImageView(m_SwapChainFactory->m_ImageViews[index]);
    m_ColorAttachment.setImageLayout(vk::ImageLayout::eAttachmentOptimal);
    m_ColorAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
    m_ColorAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
    m_ColorAttachment.setClearValue(vk::ClearValue({0.5f, 0.0f, 0.25f, 1.0f}));
}

void ImageFrame::Build_RenderingInfo(const vk::Extent2D& screenSize) {
    m_RenderingInfo.setFlags(vk::RenderingFlagsKHR());
    m_RenderingInfo.setRenderArea(vk::Rect2D({0, 0}, screenSize));
    m_RenderingInfo.setLayerCount(1);
    m_RenderingInfo.setViewMask(0);

    m_RenderingInfo.setColorAttachmentCount(1);
    m_RenderingInfo.setPColorAttachments(&m_ColorAttachment);

    if (m_DepthImageView && m_DepthFormat != vk::Format::eUndefined) {
        m_RenderingInfo.setPDepthAttachment(&m_DepthAttachment);
    } else {
        m_RenderingInfo.setPDepthAttachment(nullptr);
    }
}
