//
// Created by capma on 8/6/2025.
//

#include "DepthPass.h"

#include "Factories/ImageFactory.h"
#include "Factories/ShaderFactory.h"

DepthPass::DepthPass(const vk::raii::Device &Device,
	std::vector<std::unique_ptr<vk::raii::CommandBuffer>> &CommandBuffer): m_Device(Device), m_CommandBuffer(CommandBuffer) {
	m_DepthPipelineFactory = std::make_unique<PipelineFactory>(Device);
	CreateModules();
}


void DepthPass::WindowResizeShiftLayout(const vk::raii::CommandBuffer& command_buffer) {
	using AF = vk::AccessFlagBits;
	using PS = vk::PipelineStageFlagBits;

	ImageFactory::ShiftImageLayout(
		*command_buffer,
		m_DepthImage,
		vk::ImageLayout::eDepthStencilReadOnlyOptimal,
		AF::eNone,
		AF::eShaderRead,
		PS::eTopOfPipe,
		PS::eFragmentShader
	);
}



void DepthPass::ShiftLayout(const vk::raii::CommandBuffer &command_buffer) {
	ImageFactory::ShiftImageLayout(
	command_buffer,
	m_DepthImage,
	vk::ImageLayout::eDepthAttachmentOptimal,
	vk::AccessFlagBits::eNone,
	vk::AccessFlagBits::eDepthStencilAttachmentWrite,
	vk::PipelineStageFlagBits::eTopOfPipe,
	vk::PipelineStageFlagBits::eEarlyFragmentTests
);
}

void DepthPass::DoPass(uint32_t CurrentFrame, uint32_t width, uint32_t height) {
    vk::Viewport viewport{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
    vk::Rect2D scissor{{0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}};

    vk::RenderingAttachmentInfo depthOnlyAttachment{};
    depthOnlyAttachment.setImageView(m_DepthImageView);
    depthOnlyAttachment.setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal);
    depthOnlyAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
    depthOnlyAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
    depthOnlyAttachment.setClearValue(vk::ClearValue().setDepthStencil({1.0f, 0}));

    vk::RenderingInfo renderInfo{};
    renderInfo.setRenderArea(scissor);
    renderInfo.setLayerCount(1);
    renderInfo.setColorAttachmentCount(0);
    renderInfo.setPDepthAttachment(&depthOnlyAttachment);


    m_CommandBuffer[CurrentFrame]->beginRendering(renderInfo);
    m_CommandBuffer[CurrentFrame]->bindPipeline(vk::PipelineBindPoint::eGraphics, **m_DepthPrepassPipeline);
    m_CommandBuffer[CurrentFrame]->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PipelineLayout, 0, m_DescriptorSets, {});
    m_CommandBuffer[CurrentFrame]->setViewport(0, viewport);
    m_CommandBuffer[CurrentFrame]->setScissor(0, scissor);

    for (const auto& mesh : m_Meshes) {
         m_CommandBuffer[CurrentFrame]->bindVertexBuffers(0, {mesh.m_VertexBufferInfo.m_Buffer}, mesh.m_VertexOffset);
         m_CommandBuffer[CurrentFrame]->bindIndexBuffer(mesh.m_IndexBufferInfo.m_Buffer, 0, vk::IndexType::eUint32);
         m_CommandBuffer[CurrentFrame]->pushConstants(m_PipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, vk::ArrayProxy<const Material>{mesh.m_Material});
         m_CommandBuffer[CurrentFrame]->drawIndexed(mesh.m_IndexCount, 1, 0, 0, 0);
    }

    m_CommandBuffer[CurrentFrame]->endRendering();

    ImageFactory::ShiftImageLayout(*m_CommandBuffer[CurrentFrame],
        m_DepthImage,
        vk::ImageLayout::eDepthAttachmentOptimal,
        vk::AccessFlagBits::eDepthStencilAttachmentWrite,
        vk::AccessFlagBits::eDepthStencilAttachmentRead,
        vk::PipelineStageFlagBits::eEarlyFragmentTests,
        vk::PipelineStageFlagBits::eEarlyFragmentTests);
}

void DepthPass::CreatePipeline(uint32_t ImageResourceSize, const std::pair<vk::Format,vk::Format> &ColorAndDepthFormat) {
        vk::PipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = vk::CompareOp::eLess;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;


	uint32_t textureCount = static_cast<uint32_t>(ImageResourceSize);

	vk::SpecializationMapEntry specializationEntry(0, 0, sizeof(uint32_t));

	// Data buffer holding the value
	vk::SpecializationInfo specializationInfo;
	specializationInfo.mapEntryCount = 1;
	specializationInfo.pMapEntries = &specializationEntry;
	specializationInfo.dataSize = sizeof(textureCount);
	specializationInfo.pData = &textureCount;

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask =
		vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
		vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    colorBlendAttachment.blendEnable = VK_FALSE;

    // Multisampling
    vk::PipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

    // Rasterizer (same as usual)
    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = vk::CullModeFlagBits::eBack;
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Input assembly
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

    vk::PipelineShaderStageCreateInfo vertexStageInfo{};
    vertexStageInfo.setStage(vk::ShaderStageFlagBits::eVertex);
    vertexStageInfo.setModule(*m_DepthShaderModules[0]); // vertex shader module
    vertexStageInfo.setPName("main");

	vk::PipelineShaderStageCreateInfo fragmentStageInfo{};
	fragmentStageInfo.setStage(vk::ShaderStageFlagBits::eFragment);
	fragmentStageInfo.setModule(*m_DepthShaderModules[1]); // vertex shader module
	fragmentStageInfo.setPName("main");
	fragmentStageInfo.pSpecializationInfo = &specializationInfo;

    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertexStageInfo, fragmentStageInfo };

    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr;
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr;

    vk::Format ColorFormat = std::get<0>(ColorAndDepthFormat);
    vk::Format DepthFormat = std::get<1>(ColorAndDepthFormat);

	m_Format = ColorAndDepthFormat;

    m_DepthPrepassPipeline = std::make_unique<vk::raii::Pipeline>(m_DepthPipelineFactory
        ->SetShaderStages(shaderStages)
        .SetVertexInput(vertexInputInfo)
        .SetInputAssembly(inputAssembly)
        .SetRasterizer(rasterizer)
        .SetMultisampling(multisampling)
        .SetColorBlendAttachments({colorBlendAttachment})
        .SetViewportState(viewportState)
        .SetDynamicStates({ vk::DynamicState::eScissor, vk::DynamicState::eViewport })
        .SetDepthStencil(depthStencil)
        .SetLayout(m_PipelineLayout)
        .SetColorFormats({ColorFormat})
        .SetDepthFormat(DepthFormat)
        .Build());

}

void DepthPass::CreateImage(VmaAllocator Allocator,std::deque<std::function<void(VmaAllocator)>>& VmaAllocatorsDeletionQueue, ResourceTracker* AllocationTracker,const vk::Format& DepthFormat, uint32_t width, uint32_t height) {

	vk::ImageCreateInfo imageInfo{};
	imageInfo.imageType = vk::ImageType::e2D;
	imageInfo.extent = vk::Extent3D{ width, height, 1 };
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = DepthFormat;
	imageInfo.tiling = vk::ImageTiling::eOptimal;
	imageInfo.initialLayout = vk::ImageLayout::eUndefined;
	imageInfo.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc ;
	imageInfo.samples = vk::SampleCountFlagBits::e1;
	imageInfo.sharingMode = vk::SharingMode::eExclusive;

	ImageFactory::CreateImage(Allocator,m_DepthImage,imageInfo);

	m_DepthImageView = ImageFactory::CreateImageView(
			m_Device,m_DepthImage.image,
			DepthFormat,vk::ImageAspectFlagBits::eDepth,
			AllocationTracker, "DepthImageView"
			);

	m_DepthImage.imageAspectFlags = vk::ImageAspectFlagBits::eDepth;
	m_DepthImage.imageLayout = vk::ImageLayout::eUndefined;

	AllocationTracker->TrackAllocation(m_DepthImage.allocation, "DepthImage");

	VmaAllocatorsDeletionQueue.emplace_back([=](VmaAllocator) {
		AllocationTracker->UntrackImageView(m_DepthImageView);
		vkDestroyImageView(*m_Device,m_DepthImageView,nullptr);

		AllocationTracker->UntrackAllocation(m_DepthImage.allocation);
		vmaDestroyImage(Allocator,m_DepthImage.image,m_DepthImage.allocation);

	});
}

void DepthPass::RecreateImage(VmaAllocator Allocator,std::deque<std::function<void(VmaAllocator)>>& VmaAllocatorsDeletionQueue, ResourceTracker* AllocationTracker,const vk::Format& DepthFormat, uint32_t width, uint32_t height) {
	// Destroy old depth image resources
	AllocationTracker->UntrackImageView(m_DepthImageView);
	vkDestroyImageView(*m_Device, m_DepthImageView, nullptr);

	AllocationTracker->UntrackAllocation(m_DepthImage.allocation);
	vmaDestroyImage(Allocator, m_DepthImage.image, m_DepthImage.allocation);

	CreateImage(Allocator,VmaAllocatorsDeletionQueue,AllocationTracker, DepthFormat, width, height);

}

void DepthPass::CreateModules() {
    auto DepthShaderModules = ShaderFactory::Build_ShaderModules(m_Device, "shaders/Depthvert.spv", "shaders/Depthfrag.spv");
    for (auto& shader : DepthShaderModules) {
        m_DepthShaderModules.emplace_back(std::move(shader));
    }
}
