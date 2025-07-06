#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <deque>
#include <functional>

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <memory>

#include "ImageFrame.h"
#include "PhysicalDevicePicker.h"
#include "Renderer.h"
#include "Factories/DebugMessengerFactory.h"
#include "Factories/DepthImageFactory.h"
#include "Factories/DescriptorSetFactory.h"
#include "Factories/ImageFactory.h"
#include "Factories/InstanceFactory.h"
#include "Factories/LogicalDeviceFactory.h"
#include "Factories/PipelineFactory.h"
#include "Factories/SwapChainFactory.h"

#include "vma/vk_mem_alloc.h"


class VulkanWindow
{
public:

	VulkanWindow(vk::raii::Context &context);

	~VulkanWindow();

	static void FramebufferResizeCallback(GLFWwindow* window,  int width, int height);


	void Run();

	static constexpr uint32_t WIDTH = 800;
	static constexpr uint32_t HEIGHT = 600;

	static inline const std::vector<const char*> instanceExtensions = {
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
		VK_KHR_SURFACE_EXTENSION_NAME
	};

	static inline const std::vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

private:
	void InitWindow();
	void InitVulkan();

	void DrawFrame();

	void MainLoop();
	void Cleanup();


	void CreateSurface();
	GLFWwindow* m_Window{};

	std::unique_ptr<InstanceFactory> m_InstanceFactory{};
	std::unique_ptr<DebugMessengerFactory> m_DebugMessengerFactory{};
	std::unique_ptr<LogicalDeviceFactory> m_LogicalDeviceFactory{};
	std::unique_ptr<SwapChainFactory> m_SwapChainFactory{};
	std::unique_ptr<Renderer> m_Renderer{};
	std::unique_ptr<GraphicsPipelineFactory> m_GraphicsPipelineFactory{};
	std::unique_ptr<DescriptorSetFactory> m_DescriptorSetFactory{};
	std::unique_ptr<DepthImageFactory> m_DepthImageFactory;

	std::unique_ptr<vk::raii::Instance> m_Instance{};
	std::unique_ptr<vk::raii::DebugUtilsMessengerEXT> m_DebugMessenger;
	std::unique_ptr<vk::raii::PhysicalDevice> m_PhysicalDevice;
	std::unique_ptr<vk::raii::Device> m_Device;
	std::unique_ptr<vk::raii::SwapchainKHR> m_SwapChain;
	std::unique_ptr<vk::raii::Queue> m_GraphicsQueue;
	std::unique_ptr<vk::raii::Pipeline> m_Pipeline;

	std::unique_ptr<vk::raii::Semaphore> m_ImageAvailableSemaphore;
	std::unique_ptr<vk::raii::Semaphore> m_RenderFinishedSemaphore;
	std::unique_ptr<vk::raii::Fence> m_RenderFinishedFence;

	vk::SurfaceKHR m_Surface;
	std::vector<vk::Image> m_Images;

	vk::raii::Context m_Context;
	bool m_bFrameBufferResized{ false };


	std::vector<vk::raii::ShaderEXT> m_Shaders;
	std::vector<vk::ShaderEXT> rawShaders;
	std::vector<vk::raii::ShaderModule> m_ShaderModules;
	std::unique_ptr<vk::raii::Sampler> m_Sampler;

	std::unique_ptr<vk::raii::CommandPool> m_CmdPool;
	std::unique_ptr<vk::raii::CommandBuffer> m_AuxCmdBuffer;

	std::unique_ptr<vk::raii::DescriptorSetLayout> m_DescriptorSetLayout;
	std::unique_ptr<vk::raii::DescriptorPool> m_DescriptorPool;
	std::unique_ptr<vk::raii::PipelineLayout> m_PipelineLayout;

	std::vector<ImageFrame> m_ImageFrames;

	std::vector<vk::raii::ShaderModule> m_ShaderModule;

	std::deque<std::function<void(VmaAllocator)>> m_VmaAllocatorsDeletionQueue;

	VmaAllocator m_VmaAllocator{};

	std::unique_ptr<MeshFactory> m_MeshFactory;
	Mesh m_TriangleMesh;
	std::unique_ptr<ImageResource> m_ImageResource;
};
