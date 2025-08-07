//
// Created by capma on 8/7/2025.
//

#include "ShadowPass.h"

#include "Factories/ImageFactory.h"
#include "Factories/ShaderFactory.h"


void ShadowPass::CreatePipeline(uint32_t shadowMapCount, vk::Format format) {
    // formats.first  = (unused) color format
    // formats.second = depth format (e.g., vk::Format::eD32Sfloat)

    auto m_DepthFormat = format;


    // 1. Load shadow vertex shader (no fragment shader!)
    auto shadowVert = ShaderFactory::Build_ShaderModules(m_Device, "shaders/shadowvert.spv","shaders/shadowfrag.spv");

    vk::PipelineShaderStageCreateInfo vertStageInfo{};
    vertStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
    vertStageInfo.module = *shadowVert[0];
    vertStageInfo.pName = "main";

    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertStageInfo };

    // 2. Vertex input (position only for shadows)
    vk::VertexInputBindingDescription bindingDesc{ 0, sizeof(Vertex), vk::VertexInputRate::eVertex };
    std::vector<vk::VertexInputAttributeDescription> attributes = {
        { 0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos) }
    };

    vk::PipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
    vertexInput.pVertexAttributeDescriptions = attributes.data();

    // 3. Input assembly
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // 4. Viewport state (dynamic)
    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // 5. Rasterizer
    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = vk::CullModeFlagBits::eBack;
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable = VK_TRUE;
    rasterizer.depthBiasConstantFactor = 1.25f;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = 1.75f;

    // 6. Multisampling (off)
    vk::PipelineMultisampleStateCreateInfo multisampling{};
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

    // 7. Depth stencil
    vk::PipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = vk::CompareOp::eLessOrEqual;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // 8. Dynamic states
    std::vector<vk::DynamicState> dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor
    };

    // 9. No color blend attachments
    std::vector<vk::PipelineColorBlendAttachmentState> colorBlendAttachments; // empty

    // 10. Build pipeline using factory
    m_Pipeline = std::make_unique<vk::raii::Pipeline>(m_PipelineFactory
        ->SetShaderStages(shaderStages)
        .SetVertexInput(vertexInput)
        .SetInputAssembly(inputAssembly)
        .SetRasterizer(rasterizer)
        .SetMultisampling(multisampling)
        .SetColorBlendAttachments(colorBlendAttachments) // none
        .SetDepthFormat(m_DepthFormat)
        .SetDepthStencil(depthStencil)
        .SetDynamicStates(dynamicStates)
        .SetViewportState(viewportState)
        .SetLayout(m_PipelineLayout)
        .Build());
}



void ShadowPass::DoPass(uint32_t CurrentFrame, uint32_t width, uint32_t height) const {


    // 1. Begin render pass
    vk::ClearValue clearValue;
    clearValue.depthStencil = vk::ClearDepthStencilValue{1.0f, 0};

    vk::RenderPassBeginInfo renderPassInfo{};
    renderPassInfo.renderPass = **m_RenderPass;
    renderPassInfo.framebuffer = **m_Framebuffer;
    renderPassInfo.renderArea.offset = vk::Offset2D{0, 0};
    renderPassInfo.renderArea.extent = vk::Extent2D{width, height};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearValue;

    m_CommandBuffer[CurrentFrame]->beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

    // 2. Set viewport and scissor
    vk::Viewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(width);
    viewport.height = static_cast<float>(height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    m_CommandBuffer[CurrentFrame]->setViewport(0, viewport);

    vk::Rect2D scissor{};
    scissor.offset = vk::Offset2D{0, 0};
    scissor.extent = vk::Extent2D{width, height};
    m_CommandBuffer[CurrentFrame]->setScissor(0, scissor);

    // 3. Bind pipeline
    m_CommandBuffer[CurrentFrame]->bindPipeline(vk::PipelineBindPoint::eGraphics, *m_Pipeline);

    // 4. Bind descriptor sets (e.g. light-space matrix UBO)
    m_CommandBuffer[CurrentFrame]->bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        m_PipelineLayout,
        0,
        m_DescriptorSets[CurrentFrame],
        nullptr
    );

    // 5. Draw each mesh
    for (const auto& mesh : m_Meshes) {
        m_CommandBuffer[CurrentFrame]->bindVertexBuffers(0, {mesh.m_VertexBufferInfo.m_Buffer}, mesh.m_VertexOffset);
        m_CommandBuffer[CurrentFrame]->bindIndexBuffer(mesh.m_IndexBufferInfo.m_Buffer, 0, vk::IndexType::eUint32);
        m_CommandBuffer[CurrentFrame]->pushConstants(m_PipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, vk::ArrayProxy<const Material>{mesh.m_Material});
        m_CommandBuffer[CurrentFrame]->drawIndexed(mesh.m_IndexCount, 1, 0, 0, 0);
    }

    // 6. End render pass
    m_CommandBuffer[CurrentFrame]->endRenderPass();
}


void ShadowPass::CreateShadowResources(
    VmaAllocator allocator,
    std::deque<std::function<void(VmaAllocator)>>& deletionQueue,
    ResourceTracker* tracker,
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

    m_ShadowImageResource = std::make_unique<ImageResource>();
    ImageFactory::CreateImage(allocator, *m_ShadowImageResource, imageInfo);

    deletionQueue.emplace_back([resource = m_ShadowImageResource.get(), tracker = tracker](VmaAllocator alloc) {
        tracker->UntrackAllocation(resource->allocation);
        vmaDestroyImage(alloc, resource->image, resource->allocation);
    });

    if (tracker) tracker->TrackAllocation(m_ShadowImageResource->allocation, "ShadowMap");

    // Create image view
    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.image = m_ShadowImageResource->image;
    viewInfo.viewType = vk::ImageViewType::e2D;
    viewInfo.format = vk::Format::eD32Sfloat;
    viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    m_ShadowImageView = std::make_unique<vk::raii::ImageView>(m_Device, viewInfo);

    // Create render pass
    vk::AttachmentDescription depthAttachment{};
    depthAttachment.format = vk::Format::eD32Sfloat;
    depthAttachment.samples = vk::SampleCountFlagBits::e1;
    depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    depthAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    depthAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    depthAttachment.initialLayout = vk::ImageLayout::eUndefined;
    depthAttachment.finalLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;

    vk::AttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 0;
    depthAttachmentRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    vk::SubpassDescription subpass{};
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    vk::RenderPassCreateInfo renderPassInfo{};
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &depthAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    m_RenderPass = std::make_unique<vk::raii::RenderPass>(m_Device, renderPassInfo);

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
}
