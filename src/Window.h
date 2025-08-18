#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <deque>
#include <functional>

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vma/vk_mem_alloc.h>

#include <memory>

#include "Buffer.h"
#include "ResourceTracker.h"
#include "Renderer.h"
#include "Factories/DebugMessengerFactory.h"
#include "Factories/DepthImageFactory.h"
#include "Factories/DescriptorSetFactory.h"
#include "Factories/ImageFactory.h"
#include "Factories/InstanceFactory.h"
#include "Factories/LogicalDeviceFactory.h"
#include "Factories/MeshFactory.h"
#include "Factories/PipelineFactory.h"
#include "Factories/SwapChainFactory.h"

#include "Camera.h"
#include "DescriptorSets/DescriptorSets.h"
#include "Passes/ColorPass.h"
#include "Passes/DepthPass.h"
#include "Passes/GBufferPass.h"
#include "Passes/ShadowPass.h"


#include "Structs/Lights.h"

static constexpr uint32_t WIDTH = 800;
static constexpr uint32_t HEIGHT = 600;

static constexpr uint32_t ShadowResolutionMultiplier = 5;
static constexpr glm::vec2 m_ShadowResolution{
	1024 * ShadowResolutionMultiplier,1024 * ShadowResolutionMultiplier
};

class VulkanWindow
{
public:

	VulkanWindow(vk::raii::Context &context);

	~VulkanWindow();

	static void FramebufferResizeCallback(GLFWwindow* window,  int width, int height);

	void Run();

	static inline const std::vector<const char*> instanceExtensions = {
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
		VK_KHR_SURFACE_EXTENSION_NAME,
	};

	static inline const std::vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

private:
	void InitWindow();

	void CreateVmaAllocator();

	void InitVulkan();

	void PrepareFrame();

	[[nodiscard]] uint32_t AcquireSwapchainImage() const;

	void SubmitOffscreen() const;

	void BeginCommandBuffer() const;

	void TransitionInitialLayouts(uint32_t imageIndex);

	void TransitionForPresentation(uint32_t imageIndex);

	void EndCommandBuffer() const;

	void SubmitFrame() const;

	void PresentFrame(uint32_t imageIndex) const;

	void CreateSemaphoreAndFences();

	void LoadMesh();

	void CreatePipelineLayout();

	void CreateCommandBuffers();

	void DrawFrame();

	void HandleFramebufferResize(int width, int height);

	void MainLoop();

	void Cleanup();

	void UpdateUBO();

	void UpdateShadowUBO(uint32_t LightIdx);

	void CreateSurface();

	void SetupMouseCallback(GLFWwindow *window);

	void mouse_button_callback(GLFWwindow *window, int button, int action, int mods);

	void mouse_callback(double xpos, double ypos);

	void ProcessInput(GLFWwindow *window, float deltaTime);

	void RenderToCubemap(const std::vector<vk::ShaderModule> &Shader, ImageResource &inImage, const vk::ImageView &inImageView, vk::Sampler
	                     sampler, ImageResource &outImage, std::array<vk::ImageView, 6> &outImageViews, const vk::Extent2D &renderArea, int
	                     inLayerCount);

	GLFWwindow* m_Window{};

	std::unique_ptr<InstanceFactory> m_InstanceFactory{};
	std::unique_ptr<DebugMessengerFactory> m_DebugMessengerFactory{};
	std::unique_ptr<LogicalDeviceFactory> m_LogicalDeviceFactory{};
	std::unique_ptr<SwapChainFactory> m_SwapChainFactory{};
	std::unique_ptr<Renderer> m_Renderer{};

	std::unique_ptr<DescriptorSetFactory> m_DescriptorSetFactory{};
	std::unique_ptr<DepthImageFactory> m_DepthImageFactory{};

	std::unique_ptr<vk::raii::Instance> m_Instance{};
	std::unique_ptr<vk::raii::DebugUtilsMessengerEXT> m_DebugMessenger{};
	std::unique_ptr<vk::raii::PhysicalDevice> m_PhysicalDevice{};
	std::unique_ptr<vk::raii::Device> m_Device{};
	std::unique_ptr<vk::raii::SwapchainKHR> m_SwapChain{};
	std::unique_ptr<vk::raii::Queue> m_GraphicsQueue{};

	std::unique_ptr<vk::raii::Semaphore> m_ImageAvailableSemaphore{};
	std::unique_ptr<vk::raii::Semaphore> m_RenderFinishedSemaphore{};
	std::unique_ptr<vk::raii::Fence> m_RenderFinishedFence{};

	vk::SurfaceKHR m_Surface{};
	std::vector<ImageResource> m_SwapChainImages{};

	vk::raii::Context m_Context{};
	bool m_bFrameBufferResized{ false };

	std::vector<vk::ShaderEXT> rawShaders{};

	std::unique_ptr<vk::raii::Sampler> m_Sampler{};

	std::unique_ptr<vk::raii::CommandPool> m_CmdPool{};
	std::unique_ptr<vk::raii::CommandBuffer> m_MeshCmdBuffer{};

	std::unique_ptr<vk::raii::DescriptorSetLayout> m_FrameDescriptorSetLayout{};
	std::unique_ptr<vk::raii::DescriptorSetLayout> m_GlobalDescriptorSetLayout{};

	std::unique_ptr<vk::raii::PipelineLayout> m_PipelineLayout{};

	std::vector<vk::raii::ShaderModule> m_ShaderModule{};

	std::deque<std::function<void(VmaAllocator)>> m_VmaAllocatorsDeletionQueue{};

	VmaAllocator m_VmaAllocator{};

	std::unique_ptr<MeshFactory> m_MeshFactory{};
	std::vector<ImageResource> m_ImageResource{};

	BufferInfo m_UniformBufferInfo{};
	BufferInfo m_PointLightBufferInfo{};
	BufferInfo m_DirectionalLightBufferInfo{};
	BufferInfo m_ShadowUBOBufferInfo{};

	std::vector<std::unique_ptr<vk::raii::CommandBuffer>> m_CommandBuffers{};

	std::vector<Mesh> m_Meshes{};

	std::unique_ptr<ResourceTracker> m_AllocationTracker{};

	std::unique_ptr<Buffer> m_Buffer{};

	std::vector<vk::ImageView> m_SwapChainImageViews{};
	// Refactoring
	std::unique_ptr<Camera> m_Camera{};

	std::unique_ptr<ColorPass> m_ColorPass{};
	std::unique_ptr<GBufferPass> m_GBufferPass{};
	std::unique_ptr<DepthPass> m_DepthPass{};
	std::unique_ptr<ShadowPass> m_ShadowPass{};

	std::unique_ptr<DescriptorSets> m_DescriptorSets{};


    ImageResource m_CubemapImage;
	vk::ImageView m_CubemapImageView{};

    ImageResource m_IrradianceImage;
	vk::ImageView m_IrradianceImageView{};


	float cameraSpeed = 10.0f;
	double lastFrameTime = 0.f;

	double lastX = 0, lastY = 0;
	bool firstMouse = true;

	const std::vector<PointLight> m_PointLights{
				{
					{0,1,0,0},
					{1,0,0.f,7.f}
				},
				{
					{2,1,0,0},
					{0.f,1,.0f,7.f}
				},
				{
					{4,1,0,0},
					{0.f,0,1.f,7.f}
				}
	};

	const std::vector<DirectionalLight> m_DirectionalLights =
	{
		{
			glm::vec4(-.5f, -.2f, -0.f, 0.0f),
			glm::vec4(1.0f, 0.95f, 0.9f, 20.0f)
		},
		{
			glm::vec4(-.5f, -.5f, -0.f, 0.0f),
			glm::vec4(1.0f, 0.95f, 0.9f, 10.0f)
		}
	};


	glm::vec3 spawnPosition = glm::vec3(0.0f, 0.0f, 0.0f);

	size_t m_FramesInFlight{ 2 };
	uint32_t m_CurrentFrame{ 0 };

	glm::vec2 m_CurrentScreenSize{};



};
