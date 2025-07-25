#include "Window.h"


#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "Factories/ShaderFactory.h"
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
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
	glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	SetupMouseCallback(m_Window);


	while (!glfwWindowShouldClose(m_Window)) {
		glfwPollEvents();

		const auto currentTime = glfwGetTime();
		const auto deltaTime = currentTime - lastFrameTime;
		lastFrameTime = currentTime;

		ProcessInput(m_Window, static_cast<float>(deltaTime));
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

    ubo.model = glm::translate(glm::mat4(1.0f), spawnPosition);
	ubo.view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
	float aspectRatio = static_cast<float>(m_SwapChainFactory->Extent.width) / m_SwapChainFactory->Extent.height;
    ubo.proj = m_Camera->GetProjectionMatrix(aspectRatio);
	ubo.cameraPos = m_Camera->position;

    Buffer::UploadData(m_UniformBufferInfo, &ubo, sizeof(ubo));

}

void VulkanWindow::CreateSurface() {
	VkSurfaceKHR m_TempSurface;
	glfwCreateWindowSurface(**m_Instance, m_Window, nullptr,&m_TempSurface);
	m_Surface = m_TempSurface;
}

void VulkanWindow::SetupMouseCallback(GLFWwindow* window) {
	glfwSetWindowUserPointer(window, this);

	glfwSetCursorPosCallback(window, [](GLFWwindow* w, double xpos, double ypos) {
		VulkanWindow* self = static_cast<VulkanWindow*>(glfwGetWindowUserPointer(w));
		if (self) {
			self->mouse_callback(xpos, ypos);
		}
	});
}

void VulkanWindow::mouse_callback(double xpos, double ypos) {
	if (firstMouse) {
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xoffset = static_cast<float>(xpos - lastX);
	float yoffset = static_cast<float>(lastY - ypos);  // reversed y

	lastX = xpos;
	lastY = ypos;

	float sensitivity = 0.1f;
	xoffset *= sensitivity;
	yoffset *= sensitivity;

	yaw += xoffset;
	pitch += yoffset;

	if (pitch > 89.0f) pitch = 89.0f;
	if (pitch < -89.0f) pitch = -89.0f;

	glm::vec3 front;
	front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	front.y = sin(glm::radians(pitch));
	front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	cameraFront = glm::normalize(front);
}


void VulkanWindow::ProcessInput(GLFWwindow* window, float deltaTime) {
	float velocity = cameraSpeed * deltaTime;


	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	}
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
		cameraPos += cameraFront * velocity;
	}
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
		cameraPos -= cameraFront * velocity;
	}
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
		glm::vec3 right = glm::normalize(glm::cross(cameraFront, cameraUp));
		cameraPos -= right * velocity;
	}
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
		glm::vec3 right = glm::normalize(glm::cross(cameraFront, cameraUp));
		cameraPos += right * velocity;
	}
	if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
		cameraPos += cameraUp * velocity;
	}
	if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
		cameraPos -= cameraUp * velocity;
	}

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
	m_Camera = std::make_unique<Camera>();

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
				->AddBinding(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
				.AddBinding(1,vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eFragment)
				.AddBinding(2,vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eFragment)
				.AddBinding(3,vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eFragment)
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


	CreateGBuffer();

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
	imageInfo.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc ;
	imageInfo.samples = vk::SampleCountFlagBits::e1;
	imageInfo.sharingMode = vk::SharingMode::eExclusive;


	ImageFactory::CreateImage(m_VmaAllocator,m_DepthImage,imageInfo);

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

	m_GraphicsPipelineFactory = std::make_unique<PipelineFactory>(*m_Device);
	m_DepthPipelineFactory = std::make_unique<PipelineFactory>(*m_Device);
	m_GBufferPipelineFactory = std::make_unique<PipelineFactory>(*m_Device);


	CreateDepthPrepassPipeline();
	CreateGBufferPipeline();
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

    HandleFramebufferResize(width, height);

    PrepareFrame();

    uint32_t imageIndex = AcquireSwapchainImage();

    BeginCommandBuffer(imageIndex);

    TransitionInitialLayouts(imageIndex);

    DepthPrepass(imageIndex, width, height);

    GBufferPass(imageIndex, width, height);

    PrepareGBufferForRead(imageIndex);

    FinalColorPass(imageIndex, width, height);

    TransitionForPresentation(imageIndex);

    EndCommandBuffer(imageIndex);

    SubmitFrame(imageIndex);

    PresentFrame(imageIndex);
}

void VulkanWindow::HandleFramebufferResize(int width, int height) {
    if (!m_bFrameBufferResized) return;

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

void VulkanWindow::PrepareFrame() {
    m_Device->waitForFences({*m_RenderFinishedFence}, VK_TRUE, UINT64_MAX);
    m_Device->resetFences({*m_RenderFinishedFence});
}

uint32_t VulkanWindow::AcquireSwapchainImage() const {
    vk::AcquireNextImageInfoKHR acquireInfo{};
    acquireInfo.swapchain = **m_SwapChain;
    acquireInfo.timeout = UINT64_MAX;
    acquireInfo.semaphore = *m_ImageAvailableSemaphore;
    acquireInfo.deviceMask = 1;

    auto result = m_Device->acquireNextImage2KHR(acquireInfo);
    if (result.first != vk::Result::eSuccess && result.first != vk::Result::eSuboptimalKHR)
        throw std::runtime_error("Failed to acquire swap chain image!");

    return result.second;
}

void VulkanWindow::BeginCommandBuffer(uint32_t imageIndex) const {
    m_CommandBuffers[imageIndex]->reset();
    m_CommandBuffers[imageIndex]->begin(vk::CommandBufferBeginInfo());
}

void VulkanWindow::TransitionInitialLayouts(uint32_t imageIndex) {
    ImageFactory::ShiftImageLayout(*m_CommandBuffers[imageIndex],
        m_SwapChainImages[imageIndex], vk::ImageLayout::eColorAttachmentOptimal,
        vk::AccessFlagBits::eNone, vk::AccessFlagBits::eColorAttachmentWrite,
        vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput);

    ImageFactory::ShiftImageLayout(*m_CommandBuffers[imageIndex],
        m_DepthImage, vk::ImageLayout::eDepthAttachmentOptimal,
        vk::AccessFlagBits::eNone, vk::AccessFlagBits::eDepthStencilAttachmentWrite,
        vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eEarlyFragmentTests);

}

void VulkanWindow::DepthPrepass(uint32_t imageIndex, int width, int height) {
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

    auto rawDescriptorSets = GetDescriptorSets(imageIndex);

    m_CommandBuffers[imageIndex]->beginRendering(renderInfo);
    m_CommandBuffers[imageIndex]->bindPipeline(vk::PipelineBindPoint::eGraphics, **m_DepthPrepassPipeline);
    m_CommandBuffers[imageIndex]->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *m_PipelineLayout, 0, rawDescriptorSets, {});
    m_CommandBuffers[imageIndex]->setViewport(0, viewport);
    m_CommandBuffers[imageIndex]->setScissor(0, scissor);
    DrawMeshes(imageIndex);
    m_CommandBuffers[imageIndex]->endRendering();

	ImageFactory::ShiftImageLayout(*m_CommandBuffers[imageIndex],
		m_DepthImage,
		vk::ImageLayout::eDepthAttachmentOptimal,
		vk::AccessFlagBits::eDepthStencilAttachmentWrite,
		vk::AccessFlagBits::eDepthStencilAttachmentRead,
		vk::PipelineStageFlagBits::eEarlyFragmentTests,
		vk::PipelineStageFlagBits::eEarlyFragmentTests);

}

void VulkanWindow::GBufferPass(uint32_t imageIndex, int width, int height) {

	ImageFactory::ShiftImageLayout(*m_CommandBuffers[imageIndex], m_GBufferDiffuse,
	vk::ImageLayout::eColorAttachmentOptimal,
	vk::AccessFlagBits::eNone, vk::AccessFlagBits::eColorAttachmentWrite,
	vk::PipelineStageFlagBits::eNone, vk::PipelineStageFlagBits::eColorAttachmentOutput);

	ImageFactory::ShiftImageLayout(*m_CommandBuffers[imageIndex], m_GBufferNormals,
		vk::ImageLayout::eColorAttachmentOptimal,
		vk::AccessFlagBits::eNone, vk::AccessFlagBits::eColorAttachmentWrite,
		vk::PipelineStageFlagBits::eNone, vk::PipelineStageFlagBits::eColorAttachmentOutput);

	ImageFactory::ShiftImageLayout(*m_CommandBuffers[imageIndex], m_GBufferMaterial,
		vk::ImageLayout::eColorAttachmentOptimal,
		vk::AccessFlagBits::eNone, vk::AccessFlagBits::eColorAttachmentWrite,
		vk::PipelineStageFlagBits::eNone, vk::PipelineStageFlagBits::eColorAttachmentOutput);


    vk::Viewport viewport{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
    vk::Rect2D scissor{{0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}};

    std::array<vk::RenderingAttachmentInfo, 3> gbufferAttachments{
        vk::RenderingAttachmentInfo().setImageView(m_GBufferDiffuseView).setImageLayout(vk::ImageLayout::eColorAttachmentOptimal).setLoadOp(vk::AttachmentLoadOp::eClear).setStoreOp(vk::AttachmentStoreOp::eStore).setClearValue(vk::ClearValue({0,0,0,1})),
        vk::RenderingAttachmentInfo().setImageView(m_GBufferNormalsView).setImageLayout(vk::ImageLayout::eColorAttachmentOptimal).setLoadOp(vk::AttachmentLoadOp::eClear).setStoreOp(vk::AttachmentStoreOp::eStore).setClearValue(vk::ClearValue({0,0,1,0})),
        vk::RenderingAttachmentInfo().setImageView(m_GBufferMaterialView).setImageLayout(vk::ImageLayout::eColorAttachmentOptimal).setLoadOp(vk::AttachmentLoadOp::eClear).setStoreOp(vk::AttachmentStoreOp::eStore).setClearValue(vk::ClearValue({0,0,0,0}))
    };

    vk::RenderingAttachmentInfo depthAttachment{};
    depthAttachment.setImageView(m_DepthImageView);
    depthAttachment.setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal);
    depthAttachment.setLoadOp(vk::AttachmentLoadOp::eLoad);
    depthAttachment.setStoreOp(vk::AttachmentStoreOp::eDontCare);

    vk::RenderingInfo renderInfo{};
    renderInfo.setRenderArea(scissor);
    renderInfo.setLayerCount(1);
    renderInfo.setColorAttachments(gbufferAttachments);
    renderInfo.setPDepthAttachment(&depthAttachment);

    auto rawDescriptorSets = GetDescriptorSets(imageIndex);

    m_CommandBuffers[imageIndex]->beginRendering(renderInfo);
    m_CommandBuffers[imageIndex]->bindPipeline(vk::PipelineBindPoint::eGraphics, **m_GBufferPipeline);
    m_CommandBuffers[imageIndex]->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *m_PipelineLayout, 0, rawDescriptorSets, {});
    m_CommandBuffers[imageIndex]->setViewport(0, viewport);
    m_CommandBuffers[imageIndex]->setScissor(0, scissor);
    DrawMeshes(imageIndex);
    m_CommandBuffers[imageIndex]->endRendering();
}

void VulkanWindow::PrepareGBufferForRead(uint32_t imageIndex) {
	ImageFactory::ShiftImageLayout(*m_CommandBuffers[imageIndex],
			m_GBufferDiffuse,
			vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::AccessFlagBits::eColorAttachmentWrite,
			vk::AccessFlagBits::eShaderRead,
			vk::PipelineStageFlagBits::eColorAttachmentOutput,
			vk::PipelineStageFlagBits::eFragmentShader);

	ImageFactory::ShiftImageLayout(*m_CommandBuffers[imageIndex],
		m_GBufferNormals,
		vk::ImageLayout::eShaderReadOnlyOptimal,
		vk::AccessFlagBits::eColorAttachmentWrite,
		vk::AccessFlagBits::eShaderRead,
		vk::PipelineStageFlagBits::eColorAttachmentOutput,
		vk::PipelineStageFlagBits::eFragmentShader);

	ImageFactory::ShiftImageLayout(*m_CommandBuffers[imageIndex],
		m_GBufferMaterial,
		vk::ImageLayout::eShaderReadOnlyOptimal,
		vk::AccessFlagBits::eColorAttachmentWrite,
		vk::AccessFlagBits::eShaderRead,
		vk::PipelineStageFlagBits::eColorAttachmentOutput,
		vk::PipelineStageFlagBits::eFragmentShader);
}

void VulkanWindow::FinalColorPass(uint32_t imageIndex, int width, int height) const {
    vk::Viewport viewport{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
    vk::Rect2D scissor{{0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}};

    vk::RenderingAttachmentInfo colorAttachment{};
    colorAttachment.setImageView(m_SwapChainFactory->m_ImageViews[imageIndex]);
    colorAttachment.setImageLayout(vk::ImageLayout::eColorAttachmentOptimal);
    colorAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
    colorAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
    colorAttachment.setClearValue(vk::ClearValue({0.5f, 0.0f, 0.25f, 1.0f}));

    vk::RenderingInfo renderInfo{};
    renderInfo.setRenderArea(scissor);
    renderInfo.setLayerCount(1);
    renderInfo.setColorAttachments(colorAttachment);

    auto rawDescriptorSets = GetDescriptorSets(imageIndex);

    m_CommandBuffers[imageIndex]->beginRendering(renderInfo);
    m_CommandBuffers[imageIndex]->bindPipeline(vk::PipelineBindPoint::eGraphics, **m_GraphicsPipeline);
    m_CommandBuffers[imageIndex]->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *m_PipelineLayout, 0, rawDescriptorSets, {});
    m_CommandBuffers[imageIndex]->setViewport(0, viewport);
    m_CommandBuffers[imageIndex]->setScissor(0, scissor);
    m_CommandBuffers[imageIndex]->draw(3, 1, 0, 0);
    m_CommandBuffers[imageIndex]->endRendering();
}

void VulkanWindow::TransitionForPresentation(uint32_t imageIndex) {
    ImageFactory::ShiftImageLayout(*m_CommandBuffers[imageIndex],
        m_SwapChainImages[imageIndex], vk::ImageLayout::ePresentSrcKHR,
        vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eNone,
        vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eBottomOfPipe);
}

void VulkanWindow::EndCommandBuffer(uint32_t imageIndex) const {
    m_CommandBuffers[imageIndex]->end();
}

void VulkanWindow::SubmitFrame(uint32_t imageIndex) const {
    vk::SubmitInfo submitInfo{};
    vk::Semaphore waitSemaphores[] = {*m_ImageAvailableSemaphore};
    vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
    vk::Semaphore signalSemaphores[] = {*m_RenderFinishedSemaphore};

    vk::CommandBuffer cmd = *m_CommandBuffers[imageIndex];
    submitInfo.setWaitSemaphores(waitSemaphores);
    submitInfo.setWaitDstStageMask(waitStages);
    submitInfo.setCommandBuffers(cmd);
    submitInfo.setSignalSemaphores(signalSemaphores);

    m_GraphicsQueue->submit(submitInfo, **m_RenderFinishedFence);
}

void VulkanWindow::PresentFrame(uint32_t imageIndex) const {
    vk::PresentInfoKHR presentInfo{};
    presentInfo.setWaitSemaphores(**m_RenderFinishedSemaphore);
    presentInfo.setSwapchains(**m_SwapChain);
    presentInfo.setImageIndices(imageIndex);
    m_GraphicsQueue->presentKHR(presentInfo);
}

std::vector<vk::DescriptorSet> VulkanWindow::GetDescriptorSets(uint32_t imageIndex) const {
    return { *(*m_FrameDescriptorSets)[imageIndex], *(*m_GlobalDescriptorSets)[imageIndex] };
}

void VulkanWindow::DrawMeshes(uint32_t imageIndex) const {
    for (const auto& mesh : m_Meshes) {
        m_CommandBuffers[imageIndex]->bindVertexBuffers(0, {mesh.m_VertexBufferInfo.m_Buffer}, mesh.m_VertexOffset);
        m_CommandBuffers[imageIndex]->bindIndexBuffer(mesh.m_IndexBufferInfo.m_Buffer, 0, vk::IndexType::eUint32);
        m_CommandBuffers[imageIndex]->pushConstants(*m_PipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, vk::ArrayProxy<const Material>{mesh.m_Material});
        m_CommandBuffers[imageIndex]->drawIndexed(mesh.m_IndexCount, 1, 0, 0, 0);
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

	m_Meshes = m_MeshFactory->LoadModelFromGLTF("models/sponza/Sponza.gltf",
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
	TexturesPoolSize.descriptorCount = 8;

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

	vk::DescriptorImageInfo DiffuseImageInfo{};
	DiffuseImageInfo.imageLayout = vk::ImageLayout::eReadOnlyOptimal;
	DiffuseImageInfo.imageView = m_GBufferDiffuseView;
	DiffuseImageInfo.sampler = nullptr;

	vk::DescriptorImageInfo NormalImageInfo{};
	NormalImageInfo.imageLayout = vk::ImageLayout::eReadOnlyOptimal;
	NormalImageInfo.imageView = m_GBufferNormalsView;
	NormalImageInfo.sampler = nullptr;

	vk::DescriptorImageInfo MaterialImageInfo{};
	MaterialImageInfo.imageLayout = vk::ImageLayout::eReadOnlyOptimal;
	MaterialImageInfo.imageView = m_GBufferMaterialView;
	MaterialImageInfo.sampler = nullptr;


	for (auto& ds : *m_FrameDescriptorSets) {
		vk::WriteDescriptorSet writeInfo{};

		writeInfo.dstSet          = ds;
		writeInfo.descriptorType  = vk::DescriptorType::eUniformBuffer;
		writeInfo.descriptorCount = static_cast<uint32_t>(1);
		writeInfo.pImageInfo      = nullptr;
		writeInfo.pBufferInfo     = &bufferInfo;

		vk::WriteDescriptorSet textureDiffuse{};
		textureDiffuse.dstSet = ds;
		textureDiffuse.descriptorType = vk::DescriptorType::eSampledImage;
		textureDiffuse.descriptorCount = static_cast<uint32_t>(1);
		textureDiffuse.pImageInfo      = &DiffuseImageInfo;
		textureDiffuse.dstBinding	  = 1;
		textureDiffuse.pBufferInfo     = nullptr;

		vk::WriteDescriptorSet textureNormal{};
		textureNormal.dstSet = ds;
		textureNormal.descriptorType = vk::DescriptorType::eSampledImage;
		textureNormal.descriptorCount = static_cast<uint32_t>(1);
		textureNormal.pImageInfo      = &NormalImageInfo;
		textureNormal.dstBinding	  = 2;
		textureNormal.pBufferInfo     = nullptr;

		vk::WriteDescriptorSet textureMaterial{};
		textureMaterial.dstSet = ds;
		textureMaterial.descriptorType = vk::DescriptorType::eSampledImage;
		textureMaterial.descriptorCount = static_cast<uint32_t>(1);
		textureMaterial.pImageInfo      = &MaterialImageInfo;
		textureMaterial.dstBinding	  = 3;
		textureMaterial.pBufferInfo     = nullptr;


		m_Device->updateDescriptorSets({writeInfo,textureDiffuse,textureNormal,textureMaterial}, nullptr);


	}
}

void VulkanWindow::CreateShaderModules() {
	auto ShaderModules = ShaderFactory::Build_ShaderModules(*m_Device, "shaders/vert.spv", "shaders/frag.spv");
	for (auto& shader : ShaderModules) {
		m_ShaderModule.emplace_back(std::move(shader));
	}

	auto DepthShaderModules = ShaderFactory::Build_ShaderModules(*m_Device, "shaders/DepthVert.spv", "shaders/DepthFrag.spv");
	for (auto& shader : DepthShaderModules) {
		m_DepthShaderModules.emplace_back(std::move(shader));
	}

	auto GbufferShaderModules = ShaderFactory::Build_ShaderModules(*m_Device, "shaders/GbufferVert.spv", "shaders/GbufferFrag.spv");
	for (auto& shader : GbufferShaderModules) {
		m_GBufferShaderModules.emplace_back(std::move(shader));
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
	pushConstantRange.size = sizeof(Material);
	pushConstantRange.stageFlags = vk::ShaderStageFlagBits::eFragment;

	vk::PipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.setLayoutCount = static_cast<uint32_t>(DescriptorSetLayouts.size());
	layoutInfo.pSetLayouts = DescriptorSetLayouts.data();
	layoutInfo.pushConstantRangeCount = 1;
	layoutInfo.pPushConstantRanges = &pushConstantRange;

	m_PipelineLayout = std::make_unique<vk::raii::PipelineLayout>(*m_Device, layoutInfo);

}


void VulkanWindow::CreateDepthPrepassPipeline() {

    vk::PipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = vk::CompareOp::eLess;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;


	uint32_t textureCount = static_cast<uint32_t>(m_ImageResource.size());

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

    vk::Format depthFormat = m_DepthImageFactory->GetFormat();

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
        .SetLayout(*m_PipelineLayout)
        .SetColorFormats({m_SwapChainFactory->Format.format})
        .SetDepthFormat(depthFormat)
        .Build());
}

void VulkanWindow::CreateGraphicsPipeline() {

	// depth stencil
	vk::PipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.depthTestEnable = VK_FALSE;
	depthStencil.depthWriteEnable = VK_FALSE;
	depthStencil.depthCompareOp = vk::CompareOp::eEqual;
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
	rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
	rasterizer.depthBiasEnable = VK_FALSE;

	// input assembly
	vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	// vertex input info
	vk::VertexInputBindingDescription bindingDescription = Vertex::getBindingDescription();

	vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.vertexBindingDescriptionCount = 0;

	uint32_t textureCount = static_cast<uint32_t>(m_ImageResource.size());

	vk::SpecializationMapEntry specializationEntry(0, 0, sizeof(uint32_t));

	// Data buffer holding the value
	vk::SpecializationInfo specializationInfo;
	specializationInfo.mapEntryCount = 1;
	specializationInfo.pMapEntries = &specializationEntry;
	specializationInfo.dataSize = sizeof(textureCount);
	specializationInfo.pData = &textureCount;

	// shader stages
	vk::PipelineShaderStageCreateInfo vertexStageInfo{};
	vertexStageInfo.setStage(vk::ShaderStageFlagBits::eVertex);
	vertexStageInfo.setModule(*m_ShaderModule[0]);
	vertexStageInfo.setPName("main");

	vk::PipelineShaderStageCreateInfo fragmentStageInfo{};
	fragmentStageInfo.setStage(vk::ShaderStageFlagBits::eFragment);
	fragmentStageInfo.setModule(*m_ShaderModule[1]);
	fragmentStageInfo.setPName("main");
	fragmentStageInfo.pSpecializationInfo = &specializationInfo;

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

	m_GraphicsPipeline = std::make_unique<vk::raii::Pipeline>(m_GraphicsPipelineFactory
		->SetShaderStages(shaderStages)
		.SetVertexInput(vertexInputInfo)
		.SetInputAssembly(inputAssembly)
		.SetRasterizer(rasterizer)
		.SetMultisampling(multisampling)
		.SetColorBlendAttachments({colorBlendAttachment})
		.SetViewportState(viewportState)
		.SetDynamicStates({ vk::DynamicState::eScissor, vk::DynamicState::eViewport })
		.SetDepthStencil(depthStencil)
		.SetLayout(*m_PipelineLayout)
		.SetColorFormats({colorFormat})
		.SetDepthFormat(depthFormat)
		.Build());

}

void VulkanWindow::CreateGBufferPipeline() {
    vk::PipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthCompareOp = vk::CompareOp::eEqual;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

	std::vector<vk::PipelineColorBlendAttachmentState> blendAttachments(3);
	for (auto& blend : blendAttachments) {
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

    uint32_t textureCount = static_cast<uint32_t>(m_ImageResource.size());
    vk::SpecializationMapEntry specializationEntry(0, 0, sizeof(uint32_t));
    vk::SpecializationInfo specializationInfo;
    specializationInfo.mapEntryCount = 1;
    specializationInfo.pMapEntries = &specializationEntry;
    specializationInfo.dataSize = sizeof(textureCount);
    specializationInfo.pData = &textureCount;

    vk::PipelineShaderStageCreateInfo vertexStageInfo{};
    vertexStageInfo.setStage(vk::ShaderStageFlagBits::eVertex);
    vertexStageInfo.setModule(*m_GBufferShaderModules[0]);
    vertexStageInfo.setPName("main");

    vk::PipelineShaderStageCreateInfo fragmentStageInfo{};
    fragmentStageInfo.setStage(vk::ShaderStageFlagBits::eFragment);
    fragmentStageInfo.setModule(*m_GBufferShaderModules[1]);
    fragmentStageInfo.setPName("main");
    fragmentStageInfo.pSpecializationInfo = &specializationInfo;

    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertexStageInfo, fragmentStageInfo };

    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr;
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr;

    std::vector<vk::Format> gbufferFormats = {
        vk::Format::eR8G8B8A8Srgb,   // Diffuse
        vk::Format::eR8G8B8A8Snorm,  // Normals
        vk::Format::eR8G8B8A8Srgb    // Material
    };

    vk::Format depthFormat = m_DepthImageFactory->GetFormat();

    m_GBufferPipeline = std::make_unique<vk::raii::Pipeline>(m_GBufferPipelineFactory
        ->SetShaderStages(shaderStages)
        .SetVertexInput(vertexInputInfo)
        .SetInputAssembly(inputAssembly)
        .SetRasterizer(rasterizer)
        .SetMultisampling(multisampling)
        .SetColorBlendAttachments(blendAttachments)
        .SetViewportState(viewportState)
        .SetDynamicStates({ vk::DynamicState::eScissor, vk::DynamicState::eViewport })
        .SetDepthStencil(depthStencil)
        .SetLayout(*m_PipelineLayout)
        .SetColorFormats(gbufferFormats)
        .SetDepthFormat(depthFormat)
        .Build());
}



void VulkanWindow::CreateGBuffer() {


	vk::Extent3D extent = {
		WIDTH,
		HEIGHT,
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

        ImageFactory::CreateImage(m_VmaAllocator, m_GBufferDiffuse, imageInfo);
		vk::DebugUtilsObjectNameInfoEXT nameInfo{};
		nameInfo.pObjectName = "GBuffer.Diffuse";
		nameInfo.objectType = vk::ObjectType::eImage;
		nameInfo.objectHandle = uint64_t(&*m_GBufferDiffuse.image);

		m_Device->setDebugUtilsObjectNameEXT(nameInfo);

    }

    // Normals
    {
        vk::ImageCreateInfo imageInfo{};
        imageInfo.imageType = vk::ImageType::e2D;
        imageInfo.extent = extent;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = vk::Format::eR8G8B8A8Snorm;
        imageInfo.tiling = vk::ImageTiling::eOptimal;
        imageInfo.initialLayout = vk::ImageLayout::eUndefined;
        imageInfo.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
        imageInfo.samples = vk::SampleCountFlagBits::e1;
        imageInfo.sharingMode = vk::SharingMode::eExclusive;

        ImageFactory::CreateImage(m_VmaAllocator, m_GBufferNormals, imageInfo);
		vk::DebugUtilsObjectNameInfoEXT nameInfo{};
		nameInfo.pObjectName = "GBuffer.Normals";
		nameInfo.objectType = vk::ObjectType::eImage;
		nameInfo.objectHandle = uint64_t(&*m_GBufferNormals.image);
		m_Device->setDebugUtilsObjectNameEXT(nameInfo);
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

        ImageFactory::CreateImage(m_VmaAllocator, m_GBufferMaterial, imageInfo);
		vk::DebugUtilsObjectNameInfoEXT nameInfo{};
		nameInfo.pObjectName = "GBuffer.Material";
		nameInfo.objectType = vk::ObjectType::eImage;
		nameInfo.objectHandle = uint64_t(&*m_GBufferMaterial.image);
		m_Device->setDebugUtilsObjectNameEXT(nameInfo);

    }

	m_GBufferDiffuseView = ImageFactory::CreateImageView(*m_Device, m_GBufferDiffuse.image, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor, m_AllocationTracker.get(), "GBufferDiffuseView");
	m_GBufferNormalsView = ImageFactory::CreateImageView(*m_Device, m_GBufferNormals.image, vk::Format::eR8G8B8A8Snorm, vk::ImageAspectFlagBits::eColor, m_AllocationTracker.get(), "GBufferNormalsView");
	m_GBufferMaterialView = ImageFactory::CreateImageView(*m_Device, m_GBufferMaterial.image, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor, m_AllocationTracker.get(), "GBufferMaterialView");

	m_VmaAllocatorsDeletionQueue.emplace_back([=](VmaAllocator Alloc) {
		m_AllocationTracker->UntrackImageView(m_GBufferDiffuseView);
   		m_AllocationTracker->UntrackImageView(m_GBufferNormalsView);
   		m_AllocationTracker->UntrackImageView(m_GBufferMaterialView);

   		vkDestroyImageView(**m_Device, m_GBufferDiffuseView, nullptr);
   		vkDestroyImageView(**m_Device, m_GBufferNormalsView, nullptr);
   		vkDestroyImageView(**m_Device, m_GBufferMaterialView, nullptr);

   		m_AllocationTracker->UntrackAllocation(m_GBufferDiffuse.allocation);
   		m_AllocationTracker->UntrackAllocation(m_GBufferNormals.allocation);
   		m_AllocationTracker->UntrackAllocation(m_GBufferMaterial.allocation);

   		vmaDestroyImage(Alloc, m_GBufferDiffuse.image, m_GBufferDiffuse.allocation);
   		vmaDestroyImage(Alloc, m_GBufferNormals.image, m_GBufferNormals.allocation);
   		vmaDestroyImage(Alloc, m_GBufferMaterial.image, m_GBufferMaterial.allocation);
	});

}

void VulkanWindow::CreateCommandBuffers() {
	m_CommandBuffers.emplace_back(std::make_unique<vk::raii::CommandBuffer>(m_Renderer->CreateCommandBuffer(*m_Device, *m_CmdPool)));
	m_CommandBuffers.emplace_back(std::make_unique<vk::raii::CommandBuffer>(m_Renderer->CreateCommandBuffer(*m_Device, *m_CmdPool)));
}







