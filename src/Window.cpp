#include "Window.h"

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "Factories/ShaderFactory.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

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

void VulkanWindow::MainLoop() const
{
	while (!glfwWindowShouldClose(m_Window))
	{
		glfwPollEvents();
		DrawFrame();
	}

	//vkDeviceWaitIdle(LogicalDeviceFactory->m_Device);
}

void VulkanWindow::Cleanup() const {
	// clear all swapchains before destroying surface
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
	m_SwapChain = std::make_unique<vk::raii::SwapchainKHR>(m_SwapChainFactory->Build_SwapChain(*m_Device, *m_PhysicalDevice, m_Surface,WIDTH,HEIGHT));
	m_ImageAvailableSemaphore = std::make_unique<vk::raii::Semaphore>(*m_Device, vk::SemaphoreCreateInfo());
	m_RenderFinishedSemaphore = std::make_unique<vk::raii::Semaphore>(*m_Device, vk::SemaphoreCreateInfo());

	uint32_t QueueIdx = m_LogicalDeviceFactory->FindQueueFamilyIndex(*m_PhysicalDevice,m_Surface,vk::QueueFlagBits::eGraphics);

	VkQueue rawQueue = *m_Device->getQueue(QueueIdx, 0);

	m_GraphicsQueue = std::make_unique<vk::raii::Queue>(*m_Device,rawQueue);

	m_Images = m_SwapChain->getImages();


	for (auto & Img : m_Images) {
		m_ImageFrames.emplace_back(Img,*m_Device,m_SwapChainFactory->Format.format);
	}

	auto shaders = ShaderFactory::Build_Shader(*m_Device, "../shaders/vert.spv", "../shaders/frag.spv");
	for	(auto& shader : shaders) {
		m_Shaders.emplace_back(std::move(shader));
	}

	m_CmdPool = std::make_unique<vk::raii::CommandPool>(m_Renderer->CreateCommandPool(*m_Device, QueueIdx));

	std::vector<vk::ShaderEXT> rawShaders;
	rawShaders.reserve(m_Shaders.size());
	for (const auto& shader : m_Shaders) {
		rawShaders.push_back(*shader);
	}

	for (uint32_t i = 0; i < m_Images.size(); ++i) {
		m_ImageFrames[i].SetCmdBuffer(
			m_Renderer->CreateCommandBuffer(*m_Device, *m_CmdPool), rawShaders, m_SwapChainFactory->Extent);
	}


}

void VulkanWindow::DrawFrame() const {
	vk::AcquireNextImageInfoKHR acquireInfo{};
	acquireInfo.swapchain = **m_SwapChain;
	acquireInfo.timeout = UINT64_MAX;
	acquireInfo.semaphore = *m_ImageAvailableSemaphore;
	acquireInfo.fence = nullptr;

	auto acquireResult = m_Device->acquireNextImage2KHR(acquireInfo);

	if (acquireResult.first != vk::Result::eSuccess &&
		acquireResult.first != vk::Result::eSuboptimalKHR) {
		throw std::runtime_error("Failed to acquire swap chain image!");
		}

	uint32_t imageIndex = acquireResult.second;

	vk::SubmitInfo submitInfo{};
	vk::Semaphore waitSemaphores[] = { *m_ImageAvailableSemaphore };
	vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };

	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &**(m_ImageFrames[imageIndex].m_CommandBuffer);

	vk::Semaphore signalSemaphores[] = { *m_RenderFinishedSemaphore };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	m_GraphicsQueue->submit(submitInfo, nullptr);

	vk::PresentInfoKHR presentInfo{};
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &**m_SwapChain;
	presentInfo.pImageIndices = &imageIndex;

	auto result = m_GraphicsQueue->presentKHR(presentInfo);

	m_GraphicsQueue->waitIdle();

}



