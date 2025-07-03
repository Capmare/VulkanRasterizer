#ifndef IMAGEFRAME_H
#define IMAGEFRAME_H

#include <Vulkan/vulkan.hpp>
#include "Factories/ImageFactory.h"
#include "vulkan/vulkan_raii.hpp"

class ImageFrame {

public:
    ImageFrame(const std::vector<vk::Image>& images,
               std::vector<vk::raii::ImageView>& imageViews,
               vk::raii::CommandBuffer cmdBuffer)
        : m_Images(images)
        , m_ImageViews(imageViews)
        , m_CommandBuffer(std::move(cmdBuffer))
    {}

    void RecordCmdBuffer(uint32_t imageIndex, const vk::Extent2D& screenSize, const std::vector<vk::ShaderEXT>& shaders);

    void Build_ColorAttachment(uint32_t index) {
        m_ColorAttachment.setImageView(*m_ImageViews[index]);
        m_ColorAttachment.setImageLayout(vk::ImageLayout::eAttachmentOptimal);
        m_ColorAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
        m_ColorAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
        m_ColorAttachment.setClearValue(vk::ClearValue({ 0.5f, 0.0f, 0.25f, 1.0f }));
    }

    void Build_RenderingInfo(const vk::Extent2D& screenSize) {
        m_RenderingInfo.setFlags(vk::RenderingFlagsKHR());
        m_RenderingInfo.setRenderArea(vk::Rect2D({ 0, 0 }, screenSize));
        m_RenderingInfo.setLayerCount(1);
        m_RenderingInfo.setViewMask(0);
        m_RenderingInfo.setColorAttachmentCount(1);
        m_RenderingInfo.setColorAttachments(m_ColorAttachment);
    }

    std::vector<vk::raii::ImageView>& m_ImageViews;
    const std::vector<vk::Image>& m_Images;
    vk::raii::CommandBuffer m_CommandBuffer;

private:
    vk::RenderingAttachmentInfoKHR m_ColorAttachment = {};
    vk::RenderingInfoKHR m_RenderingInfo = {};
};

#endif // IMAGEFRAME_H
