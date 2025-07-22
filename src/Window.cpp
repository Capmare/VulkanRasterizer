#include "Window.h"

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "Factories/ShaderFactory.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <ranges>

#include "Buffer.h"
#include "PhysicalDevicePicker.h"
#include "glm/gtx/transform.hpp"


VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

#include "vmaFile.h"

VulkanWindow::VulkanWindow(vk::raii::Context& context)
// Factories
	: m_InstanceFactory(std::make_unique<InstanceFactory>())
	, m_DebugMessengerFactory(std::make_unique<DebugMessengerFactory>())
	, m_LogicalDeviceFactory(std::make_unique<LogicalDeviceFactory>())
	, m_SwapChainFactory(std::make_unique<SwapChainFactory>())
	, m_Renderer(std::make_unique<Renderer>())

	// Vulkan
	, m_Context(std::move(context))

{
}

VulkanWindow::~VulkanWindow()
= default;

void VulkanWindow::FramebufferResizeCallback(GLFWwindow* window, int , int )
{
	auto App = reinterpret_cast<VulkanWindow*>(glfwGetWindowUserPointer(window));
	App->m_bFrameBufferResized = true;
}

void VulkanWindow::MainLoop()
{
	while (!glfwWindowShouldClose(m_Window)) {
		glfwPollEvents();
		DrawFrame();

		m_GraphicsQueue->waitIdle();
	}
}

void VulkanWindow::Cleanup() {
	// Destroy depth image first (RAII)
	m_DepthImageFactory.reset();

	// Destroy other buffers allocated with VMA manually
	while (!m_VmaAllocatorsDeletionQueue.empty()) {
		m_VmaAllocatorsDeletionQueue.back()(m_VmaAllocator);
		m_VmaAllocatorsDeletionQueue.pop_back();
	}


	m_ImageResource.clear();
	m_SwapChainImageViews.clear();

	m_AllocationTracker->PrintAllocations();
	m_AllocationTracker->PrintImageViews();


	vmaDestroyAllocator(m_VmaAllocator);

	m_SwapChainFactory.reset();
	m_SwapChain->clear();

	vkDestroySurfaceKHR(**m_Instance, m_Surface, nullptr);
	glfwDestroyWindow(m_Window);
	glfwTerminate();
}

void VulkanWindow::UpdateUBO() {
	MVP ubo{};
	ubo.model = glm::mat4(1.0f);
	ubo.view = glm::lookAt(glm::vec3(20.f),
						   glm::vec3(0.0f,.0f, 0.0f),
						   glm::vec3(0.0f, 1.0f, 0.f));
	ubo.proj = glm::perspective(glm::radians(45.0f),
								1.0f,
								0.1f,
								1000.0f);
	ubo.proj[1][1] *= -1;

	Buffer::UploadData(m_UniformBufferInfo,&ubo,sizeof(ubo));

}

void VulkanWindow::CreateSurface() {
	VkSurfaceKHR m_TempSurface;
	glfwCreateWindowSurface(**m_Instance, m_Window, nullptr,&m_TempSurface);
	m_Surface = m_TempSurface;
}

void VulkanWindow::Run()
{
	InitWindow();
	InitVulkan();
	MainLoop();
	Cleanup();
}

void VulkanWindow::InitWindow() {
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	m_Window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
	glfwSetWindowUserPointer(m_Window, this);
	glfwSetFramebufferSizeCallback(m_Window, FramebufferResizeCallback);
}


void VulkanWindow::CreateVmaAllocator() {
	VmaAllocatorCreateInfo vmaCreateInfo{};
	vmaCreateInfo.device = **m_Device;
	vmaCreateInfo.instance = **m_Instance;
	vmaCreateInfo.physicalDevice = **m_PhysicalDevice;
	vmaCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
	vmaCreateInfo.pAllocationCallbacks = nullptr;
	vmaCreateAllocator(&vmaCreateInfo, &m_VmaAllocator);
}

void VulkanWindow::InitVulkan() {

	m_Buffer = std::make_unique<Buffer>();
	m_AllocationTracker = std::make_unique<ResourceTracker>();

	m_Instance = std::make_unique<vk::raii::Instance>(m_InstanceFactory->Build_Instance(m_Context, instanceExtensions, validationLayers));
	m_DebugMessenger = std::make_unique<vk::raii::DebugUtilsMessengerEXT>(m_DebugMessengerFactory->Build_DebugMessenger(*m_Instance));
	CreateSurface();
	m_PhysicalDevice = std::make_unique<vk::raii::PhysicalDevice>(PhysicalDevicePicker::ChoosePhysicalDevice(*m_Instance));
	m_Device = std::make_unique<vk::raii::Device>(m_LogicalDeviceFactory->Build_Device(*m_PhysicalDevice, m_Surface));

	CreateVmaAllocator();

	m_DescriptorSetFactory = std::make_unique<DescriptorSetFactory>(*m_Device);
	m_FrameDescriptorSetLayout = std::make_unique<vk::raii::DescriptorSetLayout>(
		std::move(
			m_DescriptorSetFactory
				->AddBinding(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex)
				.Build()
		)
	);

	m_DescriptorSetFactory->ResetFactory();

	m_UniformBufferInfo = m_Buffer->CreateMapped(m_VmaAllocator,sizeof(MVP),
		vk::BufferUsageFlagBits::eUniformBuffer,
		VMA_MEMORY_USAGE_CPU_TO_GPU,
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, m_AllocationTracker.get(),"MVP");


	vk::SamplerCreateInfo samplerInfo = {
		{},
		vk::Filter::eLinear,
		vk::Filter::eLinear,
		vk::SamplerMipmapMode::eLinear,
		vk::SamplerAddressMode::eRepeat,
		vk::SamplerAddressMode::eRepeat,
		vk::SamplerAddressMode::eRepeat,
		0.0f,
		VK_TRUE,
		16.0f,
		VK_FALSE,
		vk::CompareOp::eNever,
		0.0f,
		0.0f,
		vk::BorderColor::eIntOpaqueBlack,
		VK_FALSE
	};


	m_Sampler = std::make_unique<vk::raii::Sampler>(*m_Device,samplerInfo);

	// Create Swapchain and Depth Image
	m_SwapChain = std::make_unique<vk::raii::SwapchainKHR>(
		m_SwapChainFactory->Build_SwapChain(*m_Device, *m_PhysicalDevice, m_Surface, WIDTH, HEIGHT)
	);

	m_DepthImageFactory = std::make_unique<DepthImageFactory>(
	*m_Device,
	m_VmaAllocator,
	m_SwapChainFactory->Extent,
	m_VmaAllocatorsDeletionQueue
	);

	CreateSemaphoreAndFences();

	uint32_t QueueIdx = m_LogicalDeviceFactory->FindQueueFamilyIndex(*m_PhysicalDevice, m_Surface, vk::QueueFlagBits::eGraphics);
	VkQueue rawQueue = *m_Device->getQueue(QueueIdx, 0);
	m_GraphicsQueue = std::make_unique<vk::raii::Queue>(*m_Device, rawQueue);

	m_CmdPool = std::make_unique<vk::raii::CommandPool>(m_Renderer->CreateCommandPool(*m_Device, QueueIdx));

	auto swapImg = m_SwapChain->getImages();
	m_SwapChainImages.resize(swapImg.size());

	for (size_t i = 0; i < swapImg.size(); i++) {
		m_SwapChainImages[i].image = swapImg[i];
		m_SwapChainImages[i].imageAspectFlags = vk::ImageAspectFlagBits::eColor;
	}


	CreateDescriptorPools();
	CreateFrameDescriptorSets();
	LoadMesh();


	m_GlobalDescriptorSetLayout = std::make_unique<vk::raii::DescriptorSetLayout>(
	std::move(
		m_DescriptorSetFactory
			->AddBinding(0,vk::DescriptorType::eSampler,vk::ShaderStageFlagBits::eFragment)
			.AddBinding(1,vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eFragment, static_cast<uint32_t>(m_ImageResource.size()))
			.Build()
		)
	);

	vk::Format depthFormat = m_DepthImageFactory->GetFormat();

	vk::ImageCreateInfo imageInfo{};
	imageInfo.imageType = vk::ImageType::e2D;
	imageInfo.extent = vk::Extent3D{ static_cast<uint32_t>(m_SwapChainFactory->Extent.width), static_cast<uint32_t>(m_SwapChainFactory->Extent.height), 1 };
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = depthFormat;
	imageInfo.tiling = vk::ImageTiling::eOptimal;
	imageInfo.initialLayout = vk::ImageLayout::eUndefined;
	imageInfo.usage = vk::ImageUsageFlagBits::eSampled;
	imageInfo.samples = vk::SampleCountFlagBits::e1;
	imageInfo.sharingMode = vk::SharingMode::eExclusive;

	VkImage img = VK_NULL_HANDLE;
	VkImageCreateInfo imgInfo{static_cast<VkImageCreateInfo>(imageInfo)};
	VmaAllocationCreateInfo vmaAllocationCreateInfo{};

	vmaCreateImage(m_VmaAllocator, &imgInfo, &vmaAllocationCreateInfo, &img, &m_DepthImage.allocation, nullptr);
	m_DepthImage.image = img;

	m_DepthImageView = ImageFactory::CreateImageView(
			*m_Device,m_DepthImage.image,
			depthFormat,vk::ImageAspectFlagBits::eDepth,
			m_AllocationTracker.get(), "DepthImageView"
			);

	m_DepthImage.imageAspectFlags = vk::ImageAspectFlagBits::eDepth;
	m_AllocationTracker->TrackAllocation(m_DepthImage.allocation, "DepthImage");

	m_VmaAllocatorsDeletionQueue.emplace_back([&](VmaAllocator) {
		m_AllocationTracker->UntrackImageView(m_DepthImageView);
		vkDestroyImageView(**m_Device,m_DepthImageView,nullptr);

		m_AllocationTracker->UntrackAllocation(m_DepthImage.allocation);
		vmaDestroyImage(m_VmaAllocator,m_DepthImage.image,m_DepthImage.allocation);

	});


	CreateShaderModules();
	CreateDescriptorSets();
	CreatePipelineLayout();
	CreateGraphicsPipeline();
	CreateCommandBuffers();


	/*for (auto& image : m_ImageResource) {
		auto allocation = image.allocation;
		m_VmaAllocatorsDeletionQueue.push_back([allocator, allocation](VmaAllocator) {
		vmaFreeMemory(allocator, allocation);
	});
	}*/

	m_VmaAllocatorsDeletionQueue.emplace_back([&](VmaAllocator) {
		Buffer::Destroy(m_VmaAllocator,m_UniformBufferInfo.m_Buffer,m_UniformBufferInfo.m_Allocation,m_AllocationTracker.get());
	});

	m_AllocationTracker->PrintAllocations();
}

void VulkanWindow::DrawFrame() {

	UpdateUBO();

	int width, height;
	glfwGetFramebufferSize(m_Window, &width, &height);

	if (m_bFrameBufferResized) {
		m_Device->waitIdle();

		m_SwapChain.reset();
		m_SwapChainFactory->m_ImageViews.clear();

		m_SwapChain = std::make_unique<vk::raii::SwapchainKHR>(
		m_SwapChainFactory->Build_SwapChain(*m_Device, *m_PhysicalDevice, m_Surface, width, height));

		m_SwapChainImages.clear();

		auto swapImg = m_SwapChain->getImages();
		m_SwapChainImages.resize(swapImg.size());

		for (size_t i = 0; i < swapImg.size(); i++) {
			m_SwapChainImages[i].image = swapImg[i];
			m_SwapChainImages[i].imageAspectFlags = vk::ImageAspectFlagBits::eColor;
		}

		m_bFrameBufferResized = false;
	}




	vk::Result waitResult = m_Device->waitForFences({*m_RenderFinishedFence}, false, UINT64_MAX);
	if (waitResult != vk::Result::eSuccess) {
		std::cout << "Waiting for render not succeded" << std::endl;
	}

	m_Device->resetFences({*m_RenderFinishedFence});

	vk::AcquireNextImageInfoKHR acquireInfo{};
	acquireInfo.swapchain = **m_SwapChain;
	acquireInfo.timeout = UINT64_MAX;
	acquireInfo.semaphore = *m_ImageAvailableSemaphore;
	//acquireInfo.fence = *m_RenderFinishedFence;
	acquireInfo.deviceMask = 1;

	auto acquireResult = m_Device->acquireNextImage2KHR(acquireInfo);

	if (acquireResult.first != vk::Result::eSuccess &&
		acquireResult.first != vk::Result::eSuboptimalKHR) {
		throw std::runtime_error("Failed to acquire swap chain image!");
		}

	uint32_t imageIndex = acquireResult.second;

	//m_ImageFrames[imageIndex].RecordCmdBuffer(imageIndex,m_SwapChainFactory->Extent,rawShaders);


	m_CommandBuffers[imageIndex]->reset();
	m_CommandBuffers[imageIndex]->begin(vk::CommandBufferBeginInfo());

	ImageFactory::ShiftImageLayout(
		*m_CommandBuffers[imageIndex],
		m_SwapChainImages[imageIndex],
		vk::ImageLayout::eColorAttachmentOptimal,
		vk::AccessFlagBits::eNone, vk::AccessFlagBits::eColorAttachmentWrite,
		vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput );

	ImageFactory::ShiftImageLayout(
		*m_CommandBuffers[imageIndex],
		m_DepthImage,
		vk::ImageLayout::eDepthAttachmentOptimal,
		vk::AccessFlagBits::eNone, vk::AccessFlagBits::eDepthStencilAttachmentWrite,
		vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eEarlyFragmentTests );

	vk::RenderingAttachmentInfo ColorAttachment;
	ColorAttachment.setImageView(m_SwapChainFactory->m_ImageViews[imageIndex]);
	ColorAttachment.setImageLayout(vk::ImageLayout::eAttachmentOptimal);
	ColorAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
	ColorAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
	ColorAttachment.setClearValue(vk::ClearValue({0.5f, 0.0f, 0.25f, 1.0f}));

    vk::RenderingInfo rendering_info;
	rendering_info.setFlags(vk::RenderingFlags());
	rendering_info.setRenderArea(vk::Rect2D({0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}));
	rendering_info.setLayerCount(1);
	rendering_info.setViewMask(0);
	rendering_info.setColorAttachmentCount(1);
	rendering_info.setPColorAttachments(&ColorAttachment);

	vk::RenderingAttachmentInfo depthAttachment;
	depthAttachment.setImageView(*m_SwapChainFactory->m_DepthImageView);
	depthAttachment.setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
	depthAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
	depthAttachment.setStoreOp(vk::AttachmentStoreOp::eDontCare);
	depthAttachment.setClearValue(vk::ClearValue().setDepthStencil({1.0f, 0}));

	if (*m_SwapChainFactory->m_DepthImageView && m_DepthImageFactory->GetFormat() != vk::Format::eUndefined) {
		rendering_info.setPDepthAttachment(&depthAttachment);
	} else {
		rendering_info.setPDepthAttachment(nullptr);
	}

	m_CommandBuffers[imageIndex]->beginRendering(rendering_info);
	m_CommandBuffers[imageIndex]->bindPipeline(vk::PipelineBindPoint::eGraphics, **m_Pipeline);

	vk::Viewport viewport{};
	viewport.setWidth(static_cast<float>(width));
	viewport.setHeight(static_cast<float>(height));
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	m_CommandBuffers[imageIndex]->setViewport(0,viewport);


	vk::Rect2D scissor{};
	scissor.offset = vk::Offset2D{ 0, 0 };
	scissor.extent = m_SwapChainFactory->Extent;


	m_CommandBuffers[imageIndex]->setScissor(0,scissor);


	std::vector<vk::DescriptorSet> rawDescriptorSets{};
	rawDescriptorSets.emplace_back(*(*m_FrameDescriptorSets)[imageIndex]);
	rawDescriptorSets.emplace_back(*(*m_GlobalDescriptorSets)[imageIndex]);

	m_CommandBuffers[imageIndex]->bindDescriptorSets(vk::PipelineBindPoint::eGraphics,*m_PipelineLayout,0,rawDescriptorSets,nullptr);

	for (const auto& mesh: m_Meshes) {
		m_CommandBuffers[imageIndex]->bindVertexBuffers(0,{mesh.m_VertexBufferInfo.m_Buffer}, mesh.m_VertexOffset);
		m_CommandBuffers[imageIndex]->bindIndexBuffer(mesh.m_IndexBufferInfo.m_Buffer,0,vk::IndexType::eUint32);
		m_CommandBuffers[imageIndex]->pushConstants(*m_PipelineLayout,vk::ShaderStageFlagBits::eFragment,0,vk::ArrayProxy<const uint32_t>{mesh.m_TextureIdx});
		m_CommandBuffers[imageIndex]->drawIndexed(mesh.m_IndexCount,1,0,0,0);


	}

	m_CommandBuffers[imageIndex]->endRendering();

	ImageFactory::ShiftImageLayout(
	*m_CommandBuffers[imageIndex],
	m_SwapChainImages[imageIndex],
	vk::ImageLayout::ePresentSrcKHR,
		vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eNone,
		vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eNone );

	ImageFactory::ShiftImageLayout(
		*m_CommandBuffers[imageIndex],
		m_DepthImage,
		vk::ImageLayout::eDepthAttachmentOptimal,
		vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eNone,
		vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eNone );

	m_CommandBuffers[imageIndex]->end();



	vk::SubmitInfo submitInfo{};
	vk::Semaphore waitSemaphores[] = { *m_ImageAvailableSemaphore };
	vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };

	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &**m_CommandBuffers[imageIndex];

	vk::Semaphore signalSemaphores[] = { *m_RenderFinishedSemaphore };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	m_GraphicsQueue->submit(submitInfo, **m_RenderFinishedFence);

	vk::PresentInfoKHR presentInfo{};
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &**m_SwapChain;
	presentInfo.pImageIndices = &imageIndex;

	auto result = m_GraphicsQueue->presentKHR(presentInfo);

	if (result != vk::Result::eSuccess) {
		std::cout << "Failed to present" << std::endl;
	}

}

void VulkanWindow::CreateSemaphoreAndFences() {
	m_ImageAvailableSemaphore = std::make_unique<vk::raii::Semaphore>(*m_Device, vk::SemaphoreCreateInfo());
	m_RenderFinishedSemaphore = std::make_unique<vk::raii::Semaphore>(*m_Device, vk::SemaphoreCreateInfo());
	m_RenderFinishedFence = std::make_unique<vk::raii::Fence>(*m_Device, vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));
}

void VulkanWindow::LoadMesh() {
	m_MeshFactory = std::make_unique<MeshFactory>();
	m_MeshCmdBuffer = std::make_unique<vk::raii::CommandBuffer>(std::move(m_Renderer->CreateCommandBuffer(*m_Device, *m_CmdPool)));

	m_Meshes = m_MeshFactory->LoadModelFromGLTF("../models/scene.gltf",
	                                            m_VmaAllocator,m_VmaAllocatorsDeletionQueue,
	                                            **m_MeshCmdBuffer,*m_GraphicsQueue,*m_Device,
	                                            *m_CmdPool,m_ImageResource,
	                                            m_SwapChainImageViews, m_AllocationTracker.get());
}

void VulkanWindow::CreateDescriptorPools() {
	vk::DescriptorPoolSize UboPoolSize{};
	UboPoolSize.type = vk::DescriptorType::eUniformBuffer;
	UboPoolSize.descriptorCount = 2;

	vk::DescriptorPoolSize SamplerPoolSize{};
	SamplerPoolSize.type = vk::DescriptorType::eSampler;
	SamplerPoolSize.descriptorCount = 2;

	vk::DescriptorPoolSize TexturesPoolSize{};
	TexturesPoolSize.type = vk::DescriptorType::eSampledImage;
	TexturesPoolSize.descriptorCount = 2;

	vk::DescriptorPoolSize PoolSizeArr[] = { UboPoolSize, SamplerPoolSize, TexturesPoolSize };

	vk::DescriptorPoolCreateInfo poolInfo{};
	poolInfo.maxSets = 4;
	poolInfo.poolSizeCount = 3;
	poolInfo.pPoolSizes = PoolSizeArr;
	poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

	m_DescriptorPool = std::make_unique<vk::raii::DescriptorPool>(std::move(m_Device->createDescriptorPool(poolInfo)));
}

void VulkanWindow::CreateFrameDescriptorSets() {

	vk::DescriptorSetLayout FrameDescriptorSetLayoutArr[] = {**m_FrameDescriptorSetLayout, **m_FrameDescriptorSetLayout};

	// Allocate descriptor set
	vk::DescriptorSetAllocateInfo FrameAllocInfo{};
	FrameAllocInfo.descriptorPool = *m_DescriptorPool;
	FrameAllocInfo.descriptorSetCount = 2;
	FrameAllocInfo.pSetLayouts = FrameDescriptorSetLayoutArr;

	m_FrameDescriptorSets = std::make_unique<vk::raii::DescriptorSets>(*m_Device,FrameAllocInfo);

	// Descriptor buffer info (uniform buffer)
	vk::DescriptorBufferInfo bufferInfo{};
	bufferInfo.buffer = m_UniformBufferInfo.m_Buffer;
	bufferInfo.offset = 0;
	bufferInfo.range = sizeof(MVP);

	for (auto& ds : *m_FrameDescriptorSets) {
		vk::WriteDescriptorSet writeInfo{};

		writeInfo.dstSet          = ds;
		writeInfo.descriptorType  = vk::DescriptorType::eUniformBuffer;
		writeInfo.descriptorCount = static_cast<uint32_t>(1);
		writeInfo.pImageInfo      = nullptr;
		writeInfo.pBufferInfo     = &bufferInfo;
		m_Device->updateDescriptorSets(writeInfo, nullptr);

	}
}

void VulkanWindow::CreateShaderModules() {
	auto ShaderModules = ShaderFactory::Build_ShaderModules(*m_Device, "../shaders/vert.spv", "../shaders/frag.spv");
	for (auto& shader : ShaderModules) {
		m_ShaderModule.emplace_back(std::move(shader));
	}
}

void VulkanWindow::CreateDescriptorSets() {


	// global descriptor set


	vk::DescriptorSetLayout GlobalDescriptorSetLayoutArr[] = {**m_GlobalDescriptorSetLayout, **m_GlobalDescriptorSetLayout};

	vk::DescriptorSetAllocateInfo GlobalAllocInfo{};
	GlobalAllocInfo.descriptorPool = *m_DescriptorPool;
	GlobalAllocInfo.descriptorSetCount = 2;
	GlobalAllocInfo.pSetLayouts = GlobalDescriptorSetLayoutArr;

	m_GlobalDescriptorSets = std::make_unique<vk::raii::DescriptorSets>(*m_Device,GlobalAllocInfo);

	vk::DescriptorImageInfo SamplerInfo{};
	SamplerInfo.imageLayout = vk::ImageLayout::eUndefined;
	SamplerInfo.imageView = VK_NULL_HANDLE;
	SamplerInfo.sampler = *m_Sampler;

	for (auto& ds: *m_GlobalDescriptorSets) {
		std::vector<vk::WriteDescriptorSet> descriptorWrites{};
		descriptorWrites.emplace_back();

		descriptorWrites[0].dstSet = ds;
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = vk::DescriptorType::eSampler;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pImageInfo = &SamplerInfo;

		std::vector<vk::DescriptorImageInfo> DescriptorImgInfos{};
		for (auto [i, img] : m_ImageResource | std::ranges::views::enumerate) {
			vk::DescriptorImageInfo descriptorImageInfo{};
			descriptorImageInfo.imageLayout = img.imageLayout;
			descriptorImageInfo.imageView = m_SwapChainImageViews[i];

			DescriptorImgInfos.emplace_back(descriptorImageInfo);
		}

		descriptorWrites.emplace_back();
		descriptorWrites[1].dstSet = ds;
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = vk::DescriptorType::eSampledImage;
		descriptorWrites[1].descriptorCount = static_cast<uint32_t>(DescriptorImgInfos.size());
		descriptorWrites[1].pImageInfo = DescriptorImgInfos.data();

		m_Device->updateDescriptorSets(descriptorWrites, nullptr);
	}
}

void VulkanWindow::CreatePipelineLayout() {
	std::vector<vk::DescriptorSetLayout> DescriptorSetLayouts{*m_FrameDescriptorSetLayout,*m_GlobalDescriptorSetLayout};

	vk::PushConstantRange pushConstantRange{};
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(uint32_t);
	pushConstantRange.stageFlags = vk::ShaderStageFlagBits::eFragment;

	vk::PipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.setLayoutCount = static_cast<uint32_t>(DescriptorSetLayouts.size());
	layoutInfo.pSetLayouts = DescriptorSetLayouts.data();
	layoutInfo.pushConstantRangeCount = 1;
	layoutInfo.pPushConstantRanges = &pushConstantRange;

	m_PipelineLayout = std::make_unique<vk::raii::PipelineLayout>(*m_Device, layoutInfo);

}

void VulkanWindow::CreateGraphicsPipeline() {
	m_GraphicsPipelineFactory = std::make_unique<GraphicsPipelineFactory>(*m_Device);

	// depth stencil
	vk::PipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = vk::CompareOp::eLess;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	// pipeline color attachment
	vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask =
		vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
		vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	colorBlendAttachment.blendEnable = VK_FALSE;

	// sampling
	vk::PipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

	// rasterizer
	vk::PipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = vk::PolygonMode::eFill;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = vk::CullModeFlagBits::eNone;
	rasterizer.frontFace = vk::FrontFace::eClockwise;
	rasterizer.depthBiasEnable = VK_FALSE;

	// input assembly
	vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	// vertex input info
	vk::VertexInputBindingDescription bindingDescription = Vertex::getBindingDescription();
	auto attributeDescriptions = Vertex::getAttributeDescriptions();

	vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	// shader stages
	vk::PipelineShaderStageCreateInfo vertexStageInfo{};
	vertexStageInfo.setStage(vk::ShaderStageFlagBits::eVertex);
	vertexStageInfo.setModule(*m_ShaderModule[0]);
	vertexStageInfo.setPName("main");

	vk::PipelineShaderStageCreateInfo fragmentStageInfo{};
	fragmentStageInfo.setStage(vk::ShaderStageFlagBits::eFragment);
	fragmentStageInfo.setModule(*m_ShaderModule[1]);
	fragmentStageInfo.setPName("main");

	std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertexStageInfo, fragmentStageInfo };

	// viewport state
	vk::PipelineViewportStateCreateInfo viewportState{};
	viewportState.viewportCount = 1;
	viewportState.pViewports = nullptr;
	viewportState.scissorCount = 1;
	viewportState.pScissors = nullptr;

	// color and depth format
	vk::Format colorFormat = m_SwapChainFactory->Format.format;
	vk::Format depthFormat = m_DepthImageFactory->GetFormat();

	m_Pipeline = std::make_unique<vk::raii::Pipeline>(m_GraphicsPipelineFactory
		->SetShaderStages(shaderStages)
		.SetVertexInput(vertexInputInfo)
		.SetInputAssembly(inputAssembly)
		.SetRasterizer(rasterizer)
		.SetMultisampling(multisampling)
		.SetColorBlendAttachment(colorBlendAttachment)
		.SetViewportState(viewportState)
		.SetDynamicStates({ vk::DynamicState::eScissor, vk::DynamicState::eViewport })
		.SetDepthStencil(depthStencil)
		.SetLayout(*m_PipelineLayout)
		.SetColorFormat(colorFormat)
		.SetDepthFormat(depthFormat)
		.Build());

}

void VulkanWindow::CreateCommandBuffers() {
	m_CommandBuffers.emplace_back(std::make_unique<vk::raii::CommandBuffer>(m_Renderer->CreateCommandBuffer(*m_Device, *m_CmdPool)));
	m_CommandBuffers.emplace_back(std::make_unique<vk::raii::CommandBuffer>(m_Renderer->CreateCommandBuffer(*m_Device, *m_CmdPool)));
}







