#include "ImageFrame.h"
#include "Factories/MeshFactory.h"


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
    m_CommandBuffer.setViewport(0, viewport);

    vk::Rect2D scissor({0, 0}, ScreenSize);
    m_CommandBuffer.setScissor(0, scissor);


    vk::ShaderStageFlagBits stages[2] = { vk::ShaderStageFlagBits::eVertex, vk::ShaderStageFlagBits::eFragment };
    m_CommandBuffer.bindShadersEXT(stages, Shaders);

    m_CommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_Pipeline);
    m_CommandBuffer.bindVertexBuffers2(0, m_Mesh->m_Buffer, m_Mesh->m_Offset);
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

