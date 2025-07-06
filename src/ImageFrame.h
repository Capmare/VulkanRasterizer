#ifndef IMAGE_FRAME_COMMAND_BUILDER_H
#define IMAGE_FRAME_COMMAND_BUILDER_H

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
#include "Factories/ImageFactory.h"
#include "Factories/MeshFactory.h"

class SwapChainFactory;

class ImageFrame {
public:
    ImageFrame(const std::vector<vk::Image>& images,
               SwapChainFactory* swapChainFactory,
               vk::raii::CommandBuffer commandBuffer,
               Mesh* triangleMesh)
        : m_Images(images)
        , m_SwapChainFactory(swapChainFactory)
        , m_CommandBuffer(std::move(commandBuffer))
        , m_Mesh(triangleMesh) {}

    void RecordCmdBuffer(uint32_t imageIndex, const vk::Extent2D& screenSize, const std::vector<vk::ShaderEXT>& shaders);

    void SetPipeline(vk::Pipeline pipeline) { m_Pipeline = pipeline; }
    void SetPipelineLayout(vk::PipelineLayout layout) { m_PipelineLayout = layout; }
    void SetSwapChainFactory(SwapChainFactory* factory) { m_SwapChainFactory = factory; }
    void SetCommandBuffer(vk::raii::CommandBuffer cmdBuffer) { m_CommandBuffer = std::move(cmdBuffer); }
    void SetDepthImage(vk::ImageView depthView, vk::Format format) {
        m_DepthImageView = depthView;
        m_DepthFormat = format;

        m_DepthAttachment.setImageView(m_DepthImageView);
        m_DepthAttachment.setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
        m_DepthAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
        m_DepthAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
        m_DepthAttachment.setClearValue(vk::ClearValue().setDepthStencil({1.0f, 0}));
    }
    const vk::CommandBuffer& GetCommandBuffer() const { return m_CommandBuffer; }

private:
    void Build_ColorAttachment(uint32_t index);
    void Build_RenderingInfo(const vk::Extent2D& screenSize);

    const std::vector<vk::Image>& m_Images;
    vk::ImageView m_DepthImageView = nullptr;
    vk::Format m_DepthFormat = vk::Format::eUndefined;

    SwapChainFactory* m_SwapChainFactory;
    vk::raii::CommandBuffer m_CommandBuffer;
    Mesh* m_Mesh;

    vk::Pipeline m_Pipeline;
    vk::PipelineLayout m_PipelineLayout;

    vk::RenderingAttachmentInfoKHR m_ColorAttachment;
    vk::RenderingAttachmentInfoKHR m_DepthAttachment;

    vk::RenderingInfoKHR m_RenderingInfo;
};

class ImageFrameCommandFactory {
public:
    ImageFrameCommandFactory(vk::raii::CommandBuffer& commandBuffer)
        : m_CommandBuffer(commandBuffer) {}

    ImageFrameCommandFactory& Begin(const vk::RenderingInfoKHR& renderingInfo, vk::Image image, const vk::Extent2D& screenSize);
    ImageFrameCommandFactory& SetViewport(const vk::Extent2D& screenSize);
    ImageFrameCommandFactory& SetShaders(const std::vector<vk::ShaderEXT>& shaders);
    ImageFrameCommandFactory& BindPipeline(vk::Pipeline pipeline, vk::PipelineLayout layout);
    ImageFrameCommandFactory& DrawMesh(const Mesh& mesh);
    ImageFrameCommandFactory& End(vk::Image image);

private:
    vk::raii::CommandBuffer& m_CommandBuffer;
    vk::PipelineLayout m_PipelineLayout{};
};

#endif // IMAGE_FRAME_COMMAND_BUILDER_H
