//
// Created by capma on 8/6/2025.
//

#include "GBufferPass.h"

#include <deque>
#include <functional>

#include "Factories/ImageFactory.h"
#include "Factories/PipelineFactory.h"
#include "Factories/ShaderFactory.h"

GBufferPass::GBufferPass(const vk::raii::Device &Device,
                         const std::vector<std::unique_ptr<vk::raii::CommandBuffer> > &CommandBuffer)
    : m_Device(Device)
      , m_CommandBuffer(CommandBuffer) {
    m_GBufferPipelineFactory = std::make_unique<PipelineFactory>(m_Device);
    CreateModules();
}

void GBufferPass::PrepareImagesForRead(uint32_t CurrentFrame) {
    ImageFactory::ShiftImageLayout(*m_CommandBuffer[CurrentFrame],
                                   m_GBufferDiffuse,
                                   vk::ImageLayout::eShaderReadOnlyOptimal,
                                   vk::AccessFlagBits::eColorAttachmentWrite,
                                   vk::AccessFlagBits::eShaderRead,
                                   vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                   vk::PipelineStageFlagBits::eFragmentShader);

    ImageFactory::ShiftImageLayout(*m_CommandBuffer[CurrentFrame],
                                   m_GBufferNormals,
                                   vk::ImageLayout::eShaderReadOnlyOptimal,
                                   vk::AccessFlagBits::eColorAttachmentWrite,
                                   vk::AccessFlagBits::eShaderRead,
                                   vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                   vk::PipelineStageFlagBits::eFragmentShader);


    ImageFactory::ShiftImageLayout(*m_CommandBuffer[CurrentFrame],
                                   m_GBufferMaterial,
                                   vk::ImageLayout::eShaderReadOnlyOptimal,
                                   vk::AccessFlagBits::eColorAttachmentWrite,
                                   vk::AccessFlagBits::eShaderRead,
                                   vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                   vk::PipelineStageFlagBits::eFragmentShader);
}


void GBufferPass::WindowResizeShiftLayout(const vk::raii::CommandBuffer &command_buffer) {
    using AF = vk::AccessFlagBits;
    using PS = vk::PipelineStageFlagBits;

    ImageFactory::ShiftImageLayout(
        *command_buffer,
        m_GBufferDiffuse,
        vk::ImageLayout::eColorAttachmentOptimal,
        AF::eNone,
        AF::eColorAttachmentWrite,
        PS::eTopOfPipe,
        PS::eColorAttachmentOutput
    );

    ImageFactory::ShiftImageLayout(
        *command_buffer,
        m_GBufferNormals,
        vk::ImageLayout::eColorAttachmentOptimal,
        AF::eNone,
        AF::eColorAttachmentWrite,
        PS::eTopOfPipe,
        PS::eColorAttachmentOutput
    );

    ImageFactory::ShiftImageLayout(
        *command_buffer,
        m_GBufferMaterial,
        vk::ImageLayout::eColorAttachmentOptimal,
        AF::eNone,
        AF::eColorAttachmentWrite,
        PS::eTopOfPipe,
        PS::eColorAttachmentOutput
    );
}


void GBufferPass::ShiftLayout(const vk::raii::CommandBuffer &command_buffer) {
    ImageFactory::ShiftImageLayout(
        command_buffer,
        m_GBufferDiffuse,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::AccessFlagBits::eNone,
        vk::AccessFlagBits::eColorAttachmentWrite,
        vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eColorAttachmentOutput
    );

    ImageFactory::ShiftImageLayout(
        command_buffer,
        m_GBufferDiffuse,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::AccessFlagBits::eNone,
        vk::AccessFlagBits::eColorAttachmentWrite,
        vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eColorAttachmentOutput
    );

    ImageFactory::ShiftImageLayout(
        command_buffer,
        m_GBufferNormals,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::AccessFlagBits::eNone,
        vk::AccessFlagBits::eColorAttachmentWrite,
        vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eColorAttachmentOutput
    );

    ImageFactory::ShiftImageLayout(
        command_buffer,
        m_GBufferMaterial,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::AccessFlagBits::eNone,
        vk::AccessFlagBits::eColorAttachmentWrite,
        vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eColorAttachmentOutput
    );
}


void GBufferPass::CreateGBuffer(VmaAllocator Allocator,
                                std::deque<std::function<void(VmaAllocator)> > &VmaAllocatorsDeletionQueue,
                                ResourceTracker *AllocationTracker, const uint32_t width, const uint32_t height) {
    vk::Extent3D extent = {
        width,
        height,
        1
    };

    // Diffuse
    {
        vk::ImageCreateInfo imageInfo{};
        imageInfo.imageType = vk::ImageType::e2D;
        imageInfo.extent = extent;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = vk::Format::eR8G8B8A8Srgb;
        imageInfo.tiling = vk::ImageTiling::eOptimal;
        imageInfo.initialLayout = vk::ImageLayout::eUndefined;
        imageInfo.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
        imageInfo.samples = vk::SampleCountFlagBits::e1;
        imageInfo.sharingMode = vk::SharingMode::eExclusive;


        ImageFactory::CreateImage(m_Device,Allocator, m_GBufferDiffuse, imageInfo,"GBuffer.Diffuse");

    }

    // Normals
    {
        vk::ImageCreateInfo imageInfo{};
        imageInfo.imageType = vk::ImageType::e2D;
        imageInfo.extent = extent;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = vk::Format::eR8G8B8A8Unorm;
        imageInfo.tiling = vk::ImageTiling::eOptimal;
        imageInfo.initialLayout = vk::ImageLayout::eUndefined;
        imageInfo.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
        imageInfo.samples = vk::SampleCountFlagBits::e1;
        imageInfo.sharingMode = vk::SharingMode::eExclusive;

        ImageFactory::CreateImage(m_Device, Allocator, m_GBufferNormals, imageInfo,"GBuffer.Normals");

    }

    // Material (Roughness + Metalness)
    {
        vk::ImageCreateInfo imageInfo{};
        imageInfo.imageType = vk::ImageType::e2D;
        imageInfo.extent = extent;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = vk::Format::eR8G8B8A8Srgb;
        imageInfo.tiling = vk::ImageTiling::eOptimal;
        imageInfo.initialLayout = vk::ImageLayout::eUndefined;
        imageInfo.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
        imageInfo.samples = vk::SampleCountFlagBits::e1;
        imageInfo.sharingMode = vk::SharingMode::eExclusive;

        ImageFactory::CreateImage(m_Device,Allocator, m_GBufferMaterial, imageInfo,"GBuffer.Material");

    }

    m_GBufferDiffuseView = ImageFactory::CreateImageView(m_Device, m_GBufferDiffuse.image, vk::Format::eR8G8B8A8Srgb,
                                                         vk::ImageAspectFlagBits::eColor, AllocationTracker,
                                                         "GBufferDiffuseView");
    m_GBufferNormalsView = ImageFactory::CreateImageView(m_Device, m_GBufferNormals.image, vk::Format::eR8G8B8A8Unorm,
                                                         vk::ImageAspectFlagBits::eColor, AllocationTracker,
                                                         "GBufferNormalsView");
    m_GBufferMaterialView = ImageFactory::CreateImageView(m_Device, m_GBufferMaterial.image, vk::Format::eR8G8B8A8Srgb,
                                                          vk::ImageAspectFlagBits::eColor, AllocationTracker,
                                                          "GBufferMaterialView");

    VmaAllocatorsDeletionQueue.emplace_back([=](VmaAllocator Alloc) {
        AllocationTracker->UntrackImageView(m_GBufferDiffuseView);
        AllocationTracker->UntrackImageView(m_GBufferNormalsView);
        AllocationTracker->UntrackImageView(m_GBufferMaterialView);

        vkDestroyImageView(*m_Device, m_GBufferDiffuseView, nullptr);
        vkDestroyImageView(*m_Device, m_GBufferNormalsView, nullptr);
        vkDestroyImageView(*m_Device, m_GBufferMaterialView, nullptr);

        AllocationTracker->UntrackAllocation(m_GBufferDiffuse.allocation);
        AllocationTracker->UntrackAllocation(m_GBufferNormals.allocation);
        AllocationTracker->UntrackAllocation(m_GBufferMaterial.allocation);

        vmaDestroyImage(Alloc, m_GBufferDiffuse.image, m_GBufferDiffuse.allocation);
        vmaDestroyImage(Alloc, m_GBufferNormals.image, m_GBufferNormals.allocation);
        vmaDestroyImage(Alloc, m_GBufferMaterial.image, m_GBufferMaterial.allocation);
    });
}

void GBufferPass::DoPass(const vk::ImageView DepthImageView, uint32_t CurrentFrame, uint32_t width, uint32_t height) {
    ImageFactory::ShiftImageLayout(*m_CommandBuffer[CurrentFrame], m_GBufferDiffuse,
                                   vk::ImageLayout::eColorAttachmentOptimal,
                                   vk::AccessFlagBits::eNone, vk::AccessFlagBits::eColorAttachmentWrite,
                                   vk::PipelineStageFlagBits::eNone, vk::PipelineStageFlagBits::eColorAttachmentOutput);

    ImageFactory::ShiftImageLayout(*m_CommandBuffer[CurrentFrame], m_GBufferNormals,
                                   vk::ImageLayout::eColorAttachmentOptimal,
                                   vk::AccessFlagBits::eNone, vk::AccessFlagBits::eColorAttachmentWrite,
                                   vk::PipelineStageFlagBits::eNone, vk::PipelineStageFlagBits::eColorAttachmentOutput);

    ImageFactory::ShiftImageLayout(*m_CommandBuffer[CurrentFrame], m_GBufferMaterial,
                                   vk::ImageLayout::eColorAttachmentOptimal,
                                   vk::AccessFlagBits::eNone, vk::AccessFlagBits::eColorAttachmentWrite,
                                   vk::PipelineStageFlagBits::eNone, vk::PipelineStageFlagBits::eColorAttachmentOutput);


    vk::Viewport viewport{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
    vk::Rect2D scissor{{0, 0}, {width, height}};

    std::array<vk::RenderingAttachmentInfo, 3> gbufferAttachments{
        vk::RenderingAttachmentInfo().setImageView(m_GBufferDiffuseView).
        setImageLayout(vk::ImageLayout::eColorAttachmentOptimal).setLoadOp(vk::AttachmentLoadOp::eClear).
        setStoreOp(vk::AttachmentStoreOp::eStore).setClearValue(vk::ClearValue({0, 0, 0, 1})),
        vk::RenderingAttachmentInfo().setImageView(m_GBufferNormalsView).
        setImageLayout(vk::ImageLayout::eColorAttachmentOptimal).setLoadOp(vk::AttachmentLoadOp::eClear).
        setStoreOp(vk::AttachmentStoreOp::eStore).setClearValue(vk::ClearValue({0, 0, 1, 0})),
        vk::RenderingAttachmentInfo().setImageView(m_GBufferMaterialView).
        setImageLayout(vk::ImageLayout::eColorAttachmentOptimal).setLoadOp(vk::AttachmentLoadOp::eClear).
        setStoreOp(vk::AttachmentStoreOp::eStore).setClearValue(vk::ClearValue({0, 0, 0, 0}))
    };

    vk::RenderingAttachmentInfo depthAttachment{};
    depthAttachment.setImageView(DepthImageView);
    depthAttachment.setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal);
    depthAttachment.setLoadOp(vk::AttachmentLoadOp::eLoad);
    depthAttachment.setStoreOp(vk::AttachmentStoreOp::eDontCare);

    vk::RenderingInfo renderInfo{};
    renderInfo.setRenderArea(scissor);
    renderInfo.setLayerCount(1);
    renderInfo.setColorAttachments(gbufferAttachments);
    renderInfo.setPDepthAttachment(&depthAttachment);

    m_CommandBuffer[CurrentFrame]->beginRendering(renderInfo);
    m_CommandBuffer[CurrentFrame]->bindPipeline(vk::PipelineBindPoint::eGraphics, **m_GBufferPipeline);
    m_CommandBuffer[CurrentFrame]->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PipelineLayout, 0,
                                                      m_DescriptorSets, {});
    m_CommandBuffer[CurrentFrame]->setViewport(0, viewport);
    m_CommandBuffer[CurrentFrame]->setScissor(0, scissor);

    for (const auto &mesh: m_Meshes) {
        m_CommandBuffer[CurrentFrame]->bindVertexBuffers(0, {mesh.m_VertexBufferInfo.m_Buffer}, mesh.m_VertexOffset);
        m_CommandBuffer[CurrentFrame]->bindIndexBuffer(mesh.m_IndexBufferInfo.m_Buffer, 0, vk::IndexType::eUint32);
        m_CommandBuffer[CurrentFrame]->pushConstants(m_PipelineLayout, vk::ShaderStageFlagBits::eFragment, 0,
                                                     vk::ArrayProxy<const Material>{mesh.m_Material});
        m_CommandBuffer[CurrentFrame]->drawIndexed(mesh.m_IndexCount, 1, 0, 0, 0);
    }
    m_CommandBuffer[CurrentFrame]->endRendering();
}

void GBufferPass::CreatePipeline(uint32_t ImageResourceSize, const vk::Format &DepthFormat) {
    vk::PipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthCompareOp = vk::CompareOp::eEqual;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    std::vector<vk::PipelineColorBlendAttachmentState> blendAttachments(3);
    for (auto &blend: blendAttachments) {
        blend.colorWriteMask =
                vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
        blend.blendEnable = VK_FALSE;
    }

    vk::PipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = vk::CullModeFlagBits::eBack;
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable = VK_FALSE;

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    vk::VertexInputBindingDescription bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    uint32_t textureCount = ImageResourceSize;
    vk::SpecializationMapEntry specializationEntry(0, 0, sizeof(uint32_t));
    vk::SpecializationInfo specializationInfo;
    specializationInfo.mapEntryCount = 1;
    specializationInfo.pMapEntries = &specializationEntry;
    specializationInfo.dataSize = sizeof(textureCount);
    specializationInfo.pData = &textureCount;

    vk::PipelineShaderStageCreateInfo vertexStageInfo{};
    vertexStageInfo.setStage(vk::ShaderStageFlagBits::eVertex);
    vertexStageInfo.setModule(m_GBufferShaderModules[0]);
    vertexStageInfo.setPName("main");

    vk::PipelineShaderStageCreateInfo fragmentStageInfo{};
    fragmentStageInfo.setStage(vk::ShaderStageFlagBits::eFragment);
    fragmentStageInfo.setModule(m_GBufferShaderModules[1]);
    fragmentStageInfo.setPName("main");
    fragmentStageInfo.pSpecializationInfo = &specializationInfo;

    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = {vertexStageInfo, fragmentStageInfo};

    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr;
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr;

    std::vector<vk::Format> gbufferFormats = {
        vk::Format::eR8G8B8A8Srgb, // Diffuse
        vk::Format::eR8G8B8A8Unorm, // Normals
        vk::Format::eR8G8B8A8Srgb // Material
    };


    m_GBufferPipeline = std::make_unique<vk::raii::Pipeline>(m_GBufferPipelineFactory
        ->SetShaderStages(shaderStages)
        .SetVertexInput(vertexInputInfo)
        .SetInputAssembly(inputAssembly)
        .SetRasterizer(rasterizer)
        .SetMultisampling(multisampling)
        .SetColorBlendAttachments(blendAttachments)
        .SetViewportState(viewportState)
        .SetDynamicStates({vk::DynamicState::eScissor, vk::DynamicState::eViewport})
        .SetDepthStencil(depthStencil)
        .SetLayout(m_PipelineLayout)
        .SetColorFormats(gbufferFormats)
        .SetDepthFormat(DepthFormat)
        .Build());
}

void GBufferPass::RecreateGBuffer(VmaAllocator Allocator,
                                std::deque<std::function<void(VmaAllocator)> > &VmaAllocatorsDeletionQueue,
                                ResourceTracker *AllocationTracker, const uint32_t width, const uint32_t height) {
    // Destroy old GBuffer resources
    AllocationTracker->UntrackImageView(m_GBufferDiffuseView);
    AllocationTracker->UntrackImageView(m_GBufferNormalsView);
    AllocationTracker->UntrackImageView(m_GBufferMaterialView);

    vkDestroyImageView(*m_Device, m_GBufferDiffuseView, nullptr);
    vkDestroyImageView(*m_Device, m_GBufferNormalsView, nullptr);
    vkDestroyImageView(*m_Device, m_GBufferMaterialView, nullptr);

    AllocationTracker->UntrackAllocation(m_GBufferDiffuse.allocation);
    AllocationTracker->UntrackAllocation(m_GBufferNormals.allocation);
    AllocationTracker->UntrackAllocation(m_GBufferMaterial.allocation);

    vmaDestroyImage(Allocator, m_GBufferDiffuse.image, m_GBufferDiffuse.allocation);
    vmaDestroyImage(Allocator, m_GBufferNormals.image, m_GBufferNormals.allocation);
    vmaDestroyImage(Allocator, m_GBufferMaterial.image, m_GBufferMaterial.allocation);

    vk::Extent3D extent = {width, height, 1};

    CreateGBuffer(Allocator,VmaAllocatorsDeletionQueue,AllocationTracker, width, height);
}

std::tuple<VkImageView, VkImageView, VkImageView> GBufferPass::GetImageViews() {
    return {m_GBufferDiffuseView, m_GBufferNormalsView, m_GBufferMaterialView};
}

void GBufferPass::CreateModules() {
    auto GbufferShaderModules = ShaderFactory::Build_ShaderModules(m_Device, "shaders/Gbuffervert.spv",
                                                                   "shaders/Gbufferfrag.spv");
    for (auto &shader: GbufferShaderModules) {
        m_GBufferShaderModules.emplace_back(std::move(shader));
    }
}
