//
// Created by capma on 8/7/2025.
//

#include "ShadowPass.h"

#include "Factories/ImageFactory.h"
#include "Factories/ShaderFactory.h"

void ShadowPass::CreatePipeline(uint32_t shadowMapCount, vk::Format depthFormat) {
    // Depth test/write enabled, less or equal for shadow mapping
    vk::PipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = vk::CompareOp::eLessOrEqual;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // Optional: specialization constants if your shadow shader needs them
    uint32_t textureCount = shadowMapCount;
    vk::SpecializationMapEntry specializationEntry(0, 0, sizeof(uint32_t));
    vk::SpecializationInfo specializationInfo{
        1, &specializationEntry, sizeof(textureCount), &textureCount
    };

    // No color writes
    vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    colorBlendAttachment.blendEnable = VK_FALSE;

    // Multisampling
    vk::PipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

    // Rasterizer
    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = vk::CullModeFlagBits::eBack;
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable = VK_TRUE; // depth bias for shadow mapping
    rasterizer.depthBiasConstantFactor = 1.5f;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = 1.75f;

    // Input assembly
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Vertex input (match Vertex struct)
    vk::VertexInputBindingDescription bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    // Shaders
    auto shadowShaderModules = ShaderFactory::Build_ShaderModules(m_Device, "shaders/shadowvert.spv",
                                                                  "shaders/shadowfrag.spv");

    vk::PipelineShaderStageCreateInfo vertexStageInfo{};
    vertexStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
    vertexStageInfo.module = *shadowShaderModules[0];
    vertexStageInfo.pName = "main";

    vk::PipelineShaderStageCreateInfo fragmentStageInfo{};
    fragmentStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
    fragmentStageInfo.module = *shadowShaderModules[1];
    fragmentStageInfo.pName = "main";
    fragmentStageInfo.pSpecializationInfo = &specializationInfo;

    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = {vertexStageInfo, fragmentStageInfo};

    // Viewport state (dynamic)
    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr;
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr;

    // Build pipeline — depth-only, dynamic rendering style
    m_Pipeline = std::make_unique<vk::raii::Pipeline>(
        m_PipelineFactory
        ->SetShaderStages(shaderStages)
        .SetVertexInput(vertexInputInfo)
        .SetInputAssembly(inputAssembly)
        .SetRasterizer(rasterizer)
        .SetMultisampling(multisampling)
        .SetColorBlendAttachments({colorBlendAttachment}) // no color output
        .SetViewportState(viewportState)
        .SetDynamicStates({vk::DynamicState::eScissor, vk::DynamicState::eViewport})
        .SetDepthStencil(depthStencil)
        .SetLayout(m_PipelineLayout)
        .SetColorFormats({})
        .SetDepthFormat(depthFormat)
        .Build()
    );
}


void ShadowPass::DoPass(uint32_t CurrentFrame, uint32_t width, uint32_t height) {
    vk::Viewport viewport{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
    vk::Rect2D scissor{{0, 0}, {width, height}};

    // Depth attachment for shadow map
    vk::RenderingAttachmentInfo depthAttachment{};
    depthAttachment.setImageView(m_ShadowImageView);
    depthAttachment.setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal);
    depthAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
    depthAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
    depthAttachment.setClearValue(vk::ClearValue().setDepthStencil({1.0f, 0}));

    vk::RenderingInfo renderInfo{};
    renderInfo.setRenderArea(scissor);
    renderInfo.setLayerCount(1);
    renderInfo.setColorAttachmentCount(0);
    renderInfo.setPDepthAttachment(&depthAttachment);

    m_CommandBuffer[CurrentFrame]->beginRendering(renderInfo);
    m_CommandBuffer[CurrentFrame]->bindPipeline(vk::PipelineBindPoint::eGraphics, **m_Pipeline);
    m_CommandBuffer[CurrentFrame]->bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        m_PipelineLayout,
        0,
        m_DescriptorSets,
        {}
    );
    m_CommandBuffer[CurrentFrame]->setViewport(0, viewport);
    m_CommandBuffer[CurrentFrame]->setScissor(0, scissor);

    for (const auto &mesh: m_Meshes) {
        m_CommandBuffer[CurrentFrame]->bindVertexBuffers(0, {mesh.m_VertexBufferInfo.m_Buffer}, mesh.m_VertexOffset);
        m_CommandBuffer[CurrentFrame]->bindIndexBuffer(mesh.m_IndexBufferInfo.m_Buffer, 0, vk::IndexType::eUint32);
        m_CommandBuffer[CurrentFrame]->pushConstants(
            m_PipelineLayout,
            vk::ShaderStageFlagBits::eFragment,
            0,
            vk::ArrayProxy<const Material>{mesh.m_Material}
        );
        m_CommandBuffer[CurrentFrame]->drawIndexed(mesh.m_IndexCount, 1, 0, 0, 0);
    }

    m_CommandBuffer[CurrentFrame]->endRendering();

    //  transition shadow map image for sampling
    ImageFactory::ShiftImageLayout(
        *m_CommandBuffer[CurrentFrame],
        m_ShadowImageResource,
        vk::ImageLayout::eDepthAttachmentOptimal,
        vk::AccessFlagBits::eDepthStencilAttachmentWrite,
        vk::AccessFlagBits::eShaderRead,
        vk::PipelineStageFlagBits::eEarlyFragmentTests,
        vk::PipelineStageFlagBits::eFragmentShader
    );

}


void ShadowPass::CreateShadowResources(
    VmaAllocator allocator,
    std::deque<std::function<void(VmaAllocator)> > &deletionQueue,
    ResourceTracker *tracker,
    uint32_t width,
    uint32_t height
) {
    vk::ImageCreateInfo imageInfo{};
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = vk::Format::eD32Sfloat;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.initialLayout = vk::ImageLayout::eUndefined;
    imageInfo.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment |
                      vk::ImageUsageFlagBits::eSampled;
    imageInfo.samples = vk::SampleCountFlagBits::e1;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;

    ImageFactory::CreateImage(m_Device,allocator, m_ShadowImageResource, imageInfo, "Shadow Image");


    if (tracker) tracker->TrackAllocation(m_ShadowImageResource.allocation, "ShadowMap");

    // Create image view
    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.image = m_ShadowImageResource.image;
    viewInfo.viewType = vk::ImageViewType::e2D;
    viewInfo.format = vk::Format::eD32Sfloat;
    viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    // Create sampler
    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter = vk::Filter::eLinear;
    samplerInfo.minFilter = vk::Filter::eLinear;
    samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.compareEnable = VK_TRUE;
    samplerInfo.compareOp = vk::CompareOp::eLess;
    samplerInfo.borderColor = vk::BorderColor::eFloatOpaqueWhite;
    samplerInfo.maxAnisotropy = 1.0f;

    m_ShadowSampler = std::make_unique<vk::raii::Sampler>(m_Device, samplerInfo);

    m_ShadowImageView = ImageFactory::CreateImageView(
            m_Device,
            m_ShadowImageResource.image,
            vk::Format::eD32Sfloat,
            vk::ImageAspectFlagBits::eDepth,
            tracker,
            "ShadowMap"
        );


    m_ShadowImageResource.imageAspectFlags = vk::ImageAspectFlagBits::eDepth;
    m_ShadowImageResource.imageLayout = vk::ImageLayout::eUndefined;

    deletionQueue.emplace_back([=](VmaAllocator alloc) {
        tracker->UntrackAllocation(m_ShadowImageResource.allocation);
        vmaDestroyImage(alloc, m_ShadowImageResource.image, m_ShadowImageResource.allocation);

        tracker->UntrackImageView(m_ShadowImageView);
        vmaDestroyImage(alloc, m_ShadowImageResource.image, m_ShadowImageResource.allocation);
    });

}
