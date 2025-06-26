//
// Created by capma on 6/24/2025.
//

#ifndef IMAGEFRAME_H
#define IMAGEFRAME_H
#include <Vulkan/vulkan.hpp>

#include "Factories/ImageFactory.h"
#include "vulkan/vulkan_raii.hpp"


struct ImageFrame {

    ImageFrame(vk::Image Image, const vk::raii::Device& LogicalDevice, vk::Format Format) : m_Image(Image), m_ImageView(ImageFactory::CreateImageView(LogicalDevice, Image, Format)) {


    }

    void SetCmdBuffer(vk::raii::CommandBuffer CmdBuffer, const std::vector<vk::ShaderEXT>& Shaders, vk::Extent2D ScreenSize);

    void Build_ColorAttachment() {
        m_ColorAttachment.setImageView(*m_ImageView);
        m_ColorAttachment.setImageLayout(vk::ImageLayout::eAttachmentOptimal);
        m_ColorAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
        m_ColorAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
        m_ColorAttachment.setClearValue(vk::ClearValue({ 0.5f, 0.0f, 0.25f, 1.0f }));
    }

    void Build_RenderingInfo(const vk::Extent2D& ScreenSize) {
        m_RenderingInfo.setFlags(vk::RenderingFlagsKHR());
        m_RenderingInfo.setRenderArea(vk::Rect2D({ 0,0 }, ScreenSize));
        m_RenderingInfo.setLayerCount(1);
        m_RenderingInfo.setViewMask(0);
        m_RenderingInfo.setColorAttachmentCount(1);
        m_RenderingInfo.setColorAttachments(m_ColorAttachment);
    }



    vk::Image m_Image{};
    vk::raii::ImageView m_ImageView;

    std::unique_ptr<vk::raii::CommandBuffer> m_CommandBuffer{};

private:
    vk::RenderingAttachmentInfoKHR m_ColorAttachment = {};
    vk::RenderingInfoKHR m_RenderingInfo = {};


};


inline void ImageFrame::SetCmdBuffer(vk::raii::CommandBuffer CmdBuffer, const std::vector<vk::ShaderEXT> &Shaders,
    vk::Extent2D ScreenSize)
{
    m_CommandBuffer = std::make_unique<vk::raii::CommandBuffer>(std::move(CmdBuffer));

    Build_ColorAttachment();
    Build_RenderingInfo(ScreenSize);

    vk::CommandBufferBeginInfo beginInfo{};
    m_CommandBuffer->begin(beginInfo);

    ImageFactory::ShiftImageLayout(
        *m_CommandBuffer, m_Image, vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal, vk::AccessFlagBits::eNone,
        vk::AccessFlagBits::eColorAttachmentWrite, vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eColorAttachmentOutput
    );

    m_CommandBuffer->beginRenderingKHR(m_RenderingInfo);

    vk::Viewport viewport(0.0f, 0.0f, static_cast<float>(ScreenSize.width), static_cast<float>(ScreenSize.height), 0.0f, 1.0f);
    m_CommandBuffer->setViewportWithCount(viewport);

    vk::Rect2D scissor({ 0, 0 }, ScreenSize);
    m_CommandBuffer->setScissorWithCount(scissor);

    m_CommandBuffer->setRasterizerDiscardEnable(VK_FALSE);
    m_CommandBuffer->setPolygonModeEXT(vk::PolygonMode::eFill);
    m_CommandBuffer->setRasterizationSamplesEXT(vk::SampleCountFlagBits::e1);
    uint32_t sampleMask = 1;
    m_CommandBuffer->setSampleMaskEXT(vk::SampleCountFlagBits::e1, sampleMask);
    m_CommandBuffer->setAlphaToCoverageEnableEXT(VK_FALSE);
    m_CommandBuffer->setCullMode(vk::CullModeFlagBits::eNone);
    m_CommandBuffer->setDepthTestEnable(VK_FALSE);
    m_CommandBuffer->setDepthWriteEnable(VK_FALSE);
    m_CommandBuffer->setDepthBiasEnable(VK_FALSE);
    m_CommandBuffer->setStencilTestEnable(VK_FALSE);
    m_CommandBuffer->setPrimitiveTopology(vk::PrimitiveTopology::eTriangleList);
    m_CommandBuffer->setPrimitiveRestartEnable(VK_FALSE);
    uint32_t colorBlendEnable = 0;
    m_CommandBuffer->setColorBlendEnableEXT(0, colorBlendEnable);
    vk::ColorBlendEquationEXT equation;
    equation.colorBlendOp = vk::BlendOp::eAdd;
    equation.dstColorBlendFactor = vk::BlendFactor::eZero;
    equation.srcColorBlendFactor = vk::BlendFactor::eOne;
    m_CommandBuffer->setColorBlendEquationEXT(0, equation);
    vk::ColorComponentFlags colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    m_CommandBuffer->setColorWriteMaskEXT(0, colorWriteMask);

    std::vector<vk::VertexInputBindingDescription2EXT> vertexBindings;
    std::vector<vk::VertexInputAttributeDescription2EXT> vertexAttributes;
    m_CommandBuffer->setVertexInputEXT(vertexBindings, vertexAttributes);

    vk::ShaderStageFlagBits stages[2] = { vk::ShaderStageFlagBits::eVertex, vk::ShaderStageFlagBits::eFragment };
    m_CommandBuffer->bindShadersEXT(stages, Shaders);
    m_CommandBuffer->draw(3, 1, 0, 0);

    m_CommandBuffer->endRenderingKHR();

    ImageFactory::ShiftImageLayout(
        *m_CommandBuffer, m_Image,
        vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
        vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eNone,
        vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eBottomOfPipe
    );

    m_CommandBuffer->end();
}




#endif //IMAGEFRAME_H
