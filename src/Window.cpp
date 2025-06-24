#include "Window.h"

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

VulkanWindow::VulkanWindow(vk::raii::Context& context)
// Factories
	: m_InstanceFactory(std::make_unique<InstanceFactory>())
	, m_DebugMessengerFactory(std::make_unique<DebugMessengerFactory>())
	, m_LogicalDeviceFactory(std::make_unique<LogicalDeviceFactory>())
	, m_SwapChainFactory(std::make_unique<SwapChainFactory>())
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
		//DrawFrame();
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
	m_Images = m_SwapChain->getImages();

	for (const vk::Image& Image : m_Images) {
		m_ImageViews.emplace_back(ImageFactory::CreateImageView(*m_Device,Image,m_SwapChainFactory->Format.format));
	}
}



