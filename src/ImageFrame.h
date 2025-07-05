#ifndef IMAGEFRAME_H
#define IMAGEFRAME_H

#include <Vulkan/vulkan.hpp>
#include "Factories/ImageFactory.h"
#include "Factories/MeshFactory.h"
#include "Factories/SwapChainFactory.h"
#include "vulkan/vulkan_raii.hpp"

class SwapChainFactory;

class ImageFrame {

public:
    ImageFrame(const std::vector<vk::Image>& images
               , SwapChainFactory* SwapChainFactory
               , vk::raii::CommandBuffer cmdBuffer
               , Mesh* TriangleMesh
               )
        : m_Images(images)
        , m_SwapChainFactory(SwapChainFactory)
        , m_CommandBuffer(std::move(cmdBuffer))
        , m_Mesh(TriangleMesh) {



    }

    void RecordCmdBuffer(uint32_t imageIndex, const vk::Extent2D& screenSize, const std::vector<vk::ShaderEXT>& shaders);

    void Build_ColorAttachment(uint32_t index) {
        m_ColorAttachment.setImageView(m_SwapChainFactory->m_ImageViews[index]);
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

    const std::vector<vk::Image>& m_Images;
    vk::raii::CommandBuffer m_CommandBuffer;

    SwapChainFactory* m_SwapChainFactory;

    vk::Pipeline m_Pipeline;

private:
    vk::RenderingAttachmentInfoKHR m_ColorAttachment = {};
    vk::RenderingInfoKHR m_RenderingInfo = {};

    std::unique_ptr<vk::raii::PipelineLayout> m_PipelineLayout;


    Mesh* m_Mesh;
};

#endif // IMAGEFRAME_H
