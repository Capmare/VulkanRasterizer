#include "Window.h"

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "Factories/ShaderFactory.h"
#define GLM_ENABLE_EXPERIMENTAL
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

	// clean up allocators
	while (m_VmaAllocatorsDeletionQueue.size() > 0) {
		m_VmaAllocatorsDeletionQueue.back()(m_VmaAllocator);
		m_VmaAllocatorsDeletionQueue.pop_back();

	}

	vmaDestroyAllocator(m_VmaAllocator);

	// clear all swapchains before destroying surface
	m_SwapChainFactory.reset();
	m_SwapChain->clear();

	vkDestroySurfaceKHR(**m_Instance, m_Surface, nullptr);
	glfwDestroyWindow(m_Window);
	glfwTerminate();
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

void VulkanWindow::InitWindow()
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	m_Window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
	glfwSetWindowUserPointer(m_Window, this);
	glfwSetFramebufferSizeCallback(m_Window, FramebufferResizeCallback);



}

void VulkanWindow::InitVulkan() {
	m_Instance = std::make_unique<vk::raii::Instance>(m_InstanceFactory->Build_Instance(m_Context, instanceExtensions, validationLayers));
	m_DebugMessenger = std::make_unique<vk::raii::DebugUtilsMessengerEXT>(m_DebugMessengerFactory->Build_DebugMessenger(*m_Instance));
	CreateSurface();
	m_PhysicalDevice = std::make_unique<vk::raii::PhysicalDevice>(PhysicalDevicePicker::ChoosePhysicalDevice(*m_Instance));
	m_Device = std::make_unique<vk::raii::Device>(m_LogicalDeviceFactory->Build_Device(*m_PhysicalDevice, m_Surface));

	m_DescriptorSetFactory = std::make_unique<DescriptorSetFactory>(*m_Device);
	m_DescriptorSetLayout = std::make_unique<vk::raii::DescriptorSetLayout>(
		std::move(
			m_DescriptorSetFactory
				->AddBinding(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex)
				.Build()
		)
	);

	VmaAllocatorCreateInfo vmaCreateInfo;
	vmaCreateInfo.device = **m_Device;
	vmaCreateInfo.instance = **m_Instance;
	vmaCreateInfo.physicalDevice = **m_PhysicalDevice;
	vmaCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
	vmaCreateAllocator(&vmaCreateInfo, &m_VmaAllocator);

	m_SwapChain = std::make_unique<vk::raii::SwapchainKHR>(m_SwapChainFactory->Build_SwapChain(*m_Device, *m_PhysicalDevice, m_Surface,WIDTH,HEIGHT));
	m_ImageAvailableSemaphore = std::make_unique<vk::raii::Semaphore>(*m_Device, vk::SemaphoreCreateInfo());
	m_RenderFinishedSemaphore = std::make_unique<vk::raii::Semaphore>(*m_Device, vk::SemaphoreCreateInfo());
	m_RenderFinishedFence = std::make_unique<vk::raii::Fence>(*m_Device, vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));

	uint32_t QueueIdx = m_LogicalDeviceFactory->FindQueueFamilyIndex(*m_PhysicalDevice,m_Surface,vk::QueueFlagBits::eGraphics);

	VkQueue rawQueue = *m_Device->getQueue(QueueIdx, 0);

	m_GraphicsQueue = std::make_unique<vk::raii::Queue>(*m_Device,rawQueue);

	auto shaders = ShaderFactory::Build_Shader(*m_Device, "../shaders/vert.spv", "../shaders/frag.spv");
	for	(auto& shader : shaders) {
		m_Shaders.emplace_back(std::move(shader));
	}

	m_CmdPool = std::make_unique<vk::raii::CommandPool>(m_Renderer->CreateCommandPool(*m_Device, QueueIdx));

	rawShaders.reserve(m_Shaders.size());
	for (const auto& shader : m_Shaders) {
		rawShaders.push_back(*shader);
	}
	m_Images = m_SwapChain->getImages();

	m_MeshFactory = std::make_unique<MeshFactory>();

	vk::DescriptorPoolSize poolSize{};
	poolSize.type = vk::DescriptorType::eUniformBuffer;
	poolSize.descriptorCount = 2;

	vk::DescriptorPoolCreateInfo poolInfo{};
	poolInfo.maxSets = 2;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

	m_DescriptorPool = std::make_unique<vk::raii::DescriptorPool>(std::move(m_Device->createDescriptorPool(poolInfo)));



	m_AuxCmdBuffer = std::make_unique<vk::raii::CommandBuffer>(std::move(m_Renderer->CreateCommandBuffer(*m_Device, *m_CmdPool)));
	m_TriangleMesh = m_MeshFactory->Build_Triangle(m_VmaAllocator,m_VmaAllocatorsDeletionQueue,**m_AuxCmdBuffer,*m_GraphicsQueue,*m_Device,*m_DescriptorPool,*m_DescriptorSetLayout);

	auto ShaderModules = ShaderFactory::Build_ShaderModules(*m_Device, "../shaders/vert.spv", "../shaders/frag.spv");
	for	(auto& shader : ShaderModules) {
		m_ShaderModule.emplace_back(std::move(shader));
	}

	vk::PipelineShaderStageCreateInfo vertexStageInfo{};
	vertexStageInfo.setStage(vk::ShaderStageFlagBits::eVertex);
	vertexStageInfo.setModule(*m_ShaderModule[0]);
	vertexStageInfo.setPName("main");

	vk::PipelineShaderStageCreateInfo fragmentStageInfo{};
	fragmentStageInfo.setStage(vk::ShaderStageFlagBits::eFragment);
	fragmentStageInfo.setModule(*m_ShaderModule[1]);
	fragmentStageInfo.setPName("main");

	std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = {vertexStageInfo, fragmentStageInfo};

	vk::VertexInputBindingDescription bindingDescription = Vertex::getBindingDescription();
	auto attributeDescriptions = Vertex::getAttributeDescriptions();

	vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	vk::PipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = vk::PolygonMode::eFill;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = vk::CullModeFlagBits::eNone;
	rasterizer.frontFace = vk::FrontFace::eClockwise;
	rasterizer.depthBiasEnable = VK_FALSE;

	vk::PipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

	vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask =
		vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
		vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	colorBlendAttachment.blendEnable = VK_FALSE;

	vk::PipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pSetLayouts = &**m_DescriptorSetLayout;
	layoutInfo.pushConstantRangeCount = 0;
	layoutInfo.pPushConstantRanges = nullptr;

	m_PipelineLayout = std::make_unique<vk::raii::PipelineLayout>(*m_Device, layoutInfo);

	vk::Format colorFormat = m_SwapChainFactory->Format.format;
	m_GraphicsPipelineFactory = std::make_unique<GraphicsPipelineFactory>(*m_Device);


	m_Pipeline = std::make_unique<vk::raii::Pipeline>(m_GraphicsPipelineFactory
		->SetShaderStages(shaderStages)
		.SetVertexInput(vertexInputInfo)
		.SetInputAssembly(inputAssembly)
		.SetRasterizer(rasterizer)
		.SetMultisampling(multisampling)
		.SetColorBlendAttachment(colorBlendAttachment)
		.SetDynamicStates({ vk::DynamicState::eViewport, vk::DynamicState::eScissor })
		.SetLayout(*m_PipelineLayout)
		.SetColorFormat(colorFormat)
		.Build());



	for (uint32_t i = 0; i < m_Images.size(); ++i) {
		m_ImageFrames.emplace_back(m_Images, m_SwapChainFactory.get(), m_Renderer->CreateCommandBuffer(*m_Device, *m_CmdPool), &m_TriangleMesh);
		m_ImageFrames.back().SetPipeline(**m_Pipeline);
		m_ImageFrames.back().SetPipelineLayout(**m_PipelineLayout);
	}
}

void VulkanWindow::DrawFrame() {


	int width, height;
	glfwGetFramebufferSize(m_Window, &width, &height);

	if (m_bFrameBufferResized) {
		m_Device->waitIdle();

		m_SwapChain.reset();
		m_SwapChainFactory->m_ImageViews.clear();

		m_SwapChain = std::make_unique<vk::raii::SwapchainKHR>(
			m_SwapChainFactory->Build_SwapChain(*m_Device, *m_PhysicalDevice, m_Surface, width, height));

		for (auto& frame : m_ImageFrames) {
			frame.SetSwapChainFactory(m_SwapChainFactory.get());
		}

		m_Images = m_SwapChain->getImages();

		m_bFrameBufferResized = false;
	}

	vk::Result waitResult = m_Device->waitForFences({*m_RenderFinishedFence}, false, UINT64_MAX);
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

	m_ImageFrames[imageIndex].RecordCmdBuffer(imageIndex,m_SwapChainFactory->Extent,rawShaders);
	vk::SubmitInfo submitInfo{};
	vk::Semaphore waitSemaphores[] = { *m_ImageAvailableSemaphore };
	vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };

	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_ImageFrames[imageIndex].GetCommandBuffer();

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


}




