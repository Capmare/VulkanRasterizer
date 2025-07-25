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

#include "vma/vk_mem_alloc.h"
#include "Camera.h"

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

	void CreateVmaAllocator();

	void InitVulkan();

	void PrepareFrame();

	uint32_t AcquireSwapchainImage() const;

	void BeginCommandBuffer(uint32_t imageIndex) const;

	void TransitionInitialLayouts(uint32_t imageIndex);

	void DepthPrepass(uint32_t imageIndex, int width, int height) const;

	void GBufferPass(uint32_t imageIndex, int width, int height);

	void GBufferPass(uint32_t imageIndex, int width, int height) const;

	void PrepareGBufferForRead(uint32_t imageIndex);

	void FinalColorPass(uint32_t imageIndex, int width, int height) const;

	void TransitionForPresentation(uint32_t imageIndex);

	void EndCommandBuffer(uint32_t imageIndex) const;

	void SubmitFrame(uint32_t imageIndex) const;

	void PresentFrame(uint32_t imageIndex) const;

	std::vector<vk::DescriptorSet> GetDescriptorSets(uint32_t imageIndex) const;

	void DrawMeshes(uint32_t imageIndex) const;

	void CreateSemaphoreAndFences();

	void LoadMesh();

	void CreateDescriptorPools();

	void CreateFrameDescriptorSets();

	void CreateShaderModules();

	void CreateDescriptorSets();

	void CreatePipelineLayout();

	void CreateDepthPrepassPipeline();

	void CreateGraphicsPipeline();

	void CreateGBufferPipeline();

	void CreateGBuffer();


	void CreateCommandBuffers();

	void DrawFrame();

	void HandleFramebufferResize(int width, int height);

	void MainLoop();

	void Cleanup();

	void UpdateUBO();

	void CreateSurface();

	void SetupMouseCallback(GLFWwindow *window);

	void mouse_callback(double xpos, double ypos);

	void ProcessInput(GLFWwindow *window, float deltaTime);


	GLFWwindow* m_Window{};

	std::unique_ptr<InstanceFactory> m_InstanceFactory{};
	std::unique_ptr<DebugMessengerFactory> m_DebugMessengerFactory{};
	std::unique_ptr<LogicalDeviceFactory> m_LogicalDeviceFactory{};
	std::unique_ptr<SwapChainFactory> m_SwapChainFactory{};
	std::unique_ptr<Renderer> m_Renderer{};

	std::unique_ptr<PipelineFactory> m_GraphicsPipelineFactory{};
	std::unique_ptr<PipelineFactory> m_DepthPipelineFactory{};
	std::unique_ptr<PipelineFactory> m_GBufferPipelineFactory{};

	std::unique_ptr<DescriptorSetFactory> m_DescriptorSetFactory{};
	std::unique_ptr<DepthImageFactory> m_DepthImageFactory{};

	std::unique_ptr<vk::raii::Instance> m_Instance{};
	std::unique_ptr<vk::raii::DebugUtilsMessengerEXT> m_DebugMessenger{};
	std::unique_ptr<vk::raii::PhysicalDevice> m_PhysicalDevice{};
	std::unique_ptr<vk::raii::Device> m_Device{};
	std::unique_ptr<vk::raii::SwapchainKHR> m_SwapChain{};
	std::unique_ptr<vk::raii::Queue> m_GraphicsQueue{};


	std::unique_ptr<vk::raii::Pipeline> m_GraphicsPipeline{};
	std::unique_ptr<vk::raii::Pipeline> m_DepthPrepassPipeline{};
	std::unique_ptr<vk::raii::Pipeline> m_GBufferPipeline{};

	std::unique_ptr<vk::raii::Semaphore> m_ImageAvailableSemaphore{};
	std::unique_ptr<vk::raii::Semaphore> m_RenderFinishedSemaphore{};
	std::unique_ptr<vk::raii::Fence> m_RenderFinishedFence{};

	vk::SurfaceKHR m_Surface{};
	std::vector<ImageResource> m_SwapChainImages{};
	ImageResource m_DepthImage{};

	vk::raii::Context m_Context{};
	bool m_bFrameBufferResized{ false };


	std::vector<vk::ShaderEXT> rawShaders{};
	std::vector<vk::raii::ShaderModule> m_ShaderModules{};
	std::vector<vk::raii::ShaderModule> m_DepthShaderModules{};
	std::vector<vk::raii::ShaderModule> m_GBufferShaderModules{};

	std::unique_ptr<vk::raii::Sampler> m_Sampler{};

	std::unique_ptr<vk::raii::CommandPool> m_CmdPool{};
	std::unique_ptr<vk::raii::CommandBuffer> m_MeshCmdBuffer{};

	std::unique_ptr<vk::raii::DescriptorSetLayout> m_FrameDescriptorSetLayout{};
	std::unique_ptr<vk::raii::DescriptorSetLayout> m_GlobalDescriptorSetLayout{};

	std::unique_ptr<vk::raii::DescriptorPool> m_DescriptorPool{};
	std::unique_ptr<vk::raii::PipelineLayout> m_PipelineLayout{};


	std::vector<vk::raii::ShaderModule> m_ShaderModule{};

	std::deque<std::function<void(VmaAllocator)>> m_VmaAllocatorsDeletionQueue{};

	VmaAllocator m_VmaAllocator{};

	std::unique_ptr<MeshFactory> m_MeshFactory{};
	std::vector<ImageResource> m_ImageResource{};

	// G-buffer images
	ImageResource m_GBufferDiffuse;
	ImageResource m_GBufferNormals;
	ImageResource m_GBufferMaterial;

	VkImageView m_GBufferDiffuseView{};
	VkImageView m_GBufferNormalsView{};
	VkImageView m_GBufferMaterialView{};

	BufferInfo m_UniformBufferInfo{};


	std::vector<std::unique_ptr<vk::raii::CommandBuffer>> m_CommandBuffers{};

	std::unique_ptr<vk::raii::DescriptorSets> m_FrameDescriptorSets{};
    std::unique_ptr<vk::raii::DescriptorSets> m_GlobalDescriptorSets{};

	std::vector<Mesh> m_Meshes{};

	std::unique_ptr<ResourceTracker> m_AllocationTracker{};

	std::unique_ptr<Buffer> m_Buffer{};


	vk::ImageView m_DepthImageView{};
	std::vector<vk::ImageView> m_SwapChainImageViews{};

	std::unique_ptr<Camera> m_Camera{};


	glm::vec3 cameraPos = glm::vec3(20.f, 0.f, 0.f);
	glm::vec3 cameraFront = glm::vec3(-1.f, 0.f, 0.f);
	glm::vec3 cameraUp = glm::vec3(0.f, 1.f, 0.f);
	glm::vec3 cameraRight = glm::vec3(0.f, 0.f, 1.f);

	float cameraSpeed = 10.0f;
	double lastFrameTime = 0.f;


	double lastX = 400, lastY = 300;
	bool firstMouse = true;
	float yaw = -90.0f;  // facing -Z
	float pitch = 0.0f;


	glm::vec3 spawnPosition = glm::vec3(10.0f, -5.0f, 0.0f);


};
