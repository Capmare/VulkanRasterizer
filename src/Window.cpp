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
#include "../cmake-build-release-visual-studio/_deps/assimp-src/code/AssetLib/OpenGEX/OpenGEXStructs.h"
#include "glm/gtx/transform.hpp"
#include "Structs/UBOStructs.h"


VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

#include "vmaFile.h"


static const glm::mat4 kClipBias = glm::mat4(
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, -1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.5f, 0.0f,
    0.0f, 0.0f, 0.5f, 1.0f
);


VulkanWindow::VulkanWindow(vk::raii::Context &context)
// Factories
    : m_InstanceFactory(std::make_unique<InstanceFactory>())
      , m_DebugMessengerFactory(std::make_unique<DebugMessengerFactory>())
      , m_LogicalDeviceFactory(std::make_unique<LogicalDeviceFactory>())
      , m_SwapChainFactory(std::make_unique<SwapChainFactory>())
      , m_Renderer(std::make_unique<Renderer>())

      // Vulkan
      , m_Context(std::move(context)) {
}

VulkanWindow::~VulkanWindow()
= default;

void VulkanWindow::FramebufferResizeCallback(GLFWwindow *window, int, int) {
    auto App = reinterpret_cast<VulkanWindow *>(glfwGetWindowUserPointer(window));
    App->m_bFrameBufferResized = true;
}

void VulkanWindow::MainLoop() {
    glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
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
    ubo.view = m_Camera->GetViewMatrix();
    const float aspectRatio = m_CurrentScreenSize.x / m_CurrentScreenSize.y;
    ubo.proj = m_Camera->GetProjectionMatrix(aspectRatio);
    ubo.cameraPos = m_Camera->position;

    Buffer::UploadData(m_UniformBufferInfo, &ubo, sizeof(ubo));
}

void VulkanWindow::UpdateShadowUBO() {
    // use the first directional light in your array
    glm::vec3 lightDir = glm::normalize(glm::vec3(m_DirectionalLights[0].Direction));

    glm::vec3 sceneMin, sceneMax;
    std::tie(sceneMin, sceneMax) = VulkanMath::ComputeSceneAABB(m_Meshes);
    glm::vec3 sceneCenter = 0.5f * (sceneMin + sceneMax);

    auto corners = VulkanMath::GetAABBCorners(sceneMin, sceneMax);

    float minProj = std::numeric_limits<float>::infinity();
    float maxProj = -std::numeric_limits<float>::infinity();
    for (auto &c: corners) {
        float p = glm::dot(c, lightDir);
        minProj = std::min(minProj, p);
        maxProj = std::max(maxProj, p);
    }

    float distanceToCenter = maxProj - glm::dot(sceneCenter, lightDir);
    glm::vec3 lightPos = sceneCenter - lightDir * distanceToCenter;

    glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    glm::vec3 up = (std::abs(glm::dot(lightDir, worldUp)) > 0.99f)
                       ? glm::vec3(0.0f, 0.0f, 1.0f)
                       : worldUp;

    glm::mat4 lightView = glm::lookAt(lightPos, sceneCenter, up);

    glm::vec3 minLS(std::numeric_limits<float>::infinity());
    glm::vec3 maxLS(-std::numeric_limits<float>::infinity());
    for (auto &c: corners) {
        glm::vec3 ls = glm::vec3(lightView * glm::vec4(c, 1.0f));
        minLS = glm::min(minLS, ls);
        maxLS = glm::max(maxLS, ls);
    }

    float nearPlane = -maxLS.z;
    float farPlane = -minLS.z;

    glm::mat4 lightProj = kClipBias * glm::ortho(
                              minLS.x, maxLS.x,
                              minLS.y, maxLS.y,
                              nearPlane, farPlane
                          );


    ShadowMVP smvp{};

    smvp.view = lightView;
    smvp.proj = lightProj;
    smvp.vp = lightProj * lightView;
    smvp.center = sceneCenter;
    smvp.NearFarPlanes = glm::vec2(nearPlane, farPlane);

    Buffer::UploadData(m_ShadowUBOBufferInfo, &smvp, sizeof(smvp));
}


void VulkanWindow::CreateSurface() {
    VkSurfaceKHR m_TempSurface;
    glfwCreateWindowSurface(**m_Instance, m_Window, nullptr, &m_TempSurface);
    m_Surface = m_TempSurface;
}

void VulkanWindow::SetupMouseCallback(GLFWwindow *window) {
    glfwSetWindowUserPointer(window, this);

    glfwSetCursorPosCallback(window, [](GLFWwindow *w, double xpos, double ypos) {
        if (const auto self = static_cast<VulkanWindow *>(glfwGetWindowUserPointer(w))) {
            self->mouse_callback(xpos, ypos);
        }
    });

    glfwSetMouseButtonCallback(m_Window, [](GLFWwindow *window, int button, int action, int mods) {
        if (const auto self = static_cast<VulkanWindow *>(glfwGetWindowUserPointer(window))) {
            self->mouse_button_callback(window, button, action, mods);
        }
    });
}

void VulkanWindow::mouse_button_callback(GLFWwindow *, int button, int action, int) {
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE) {
        firstMouse = true;
    }
}

void VulkanWindow::mouse_callback(double xpos, double ypos) {
    if (glfwGetMouseButton(m_Window, GLFW_MOUSE_BUTTON_RIGHT) != GLFW_PRESS)
        return;

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = static_cast<float>(xpos - lastX);
    float yoffset = static_cast<float>(lastY - ypos); // reversed

    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    m_Camera->yaw += xoffset;
    m_Camera->pitch += yoffset;

    m_Camera->pitch = glm::clamp(m_Camera->pitch, -89.0f, 89.0f);
    m_Camera->UpdateTarget();
}


void VulkanWindow::ProcessInput(GLFWwindow *window, float deltaTime) {
    float velocity = cameraSpeed * deltaTime;

    glm::vec3 front = glm::normalize(m_Camera->target);
    glm::vec3 right = glm::normalize(glm::cross(front, m_Camera->up));
    glm::vec3 up = m_Camera->up;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        m_Camera->position += front * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        m_Camera->position -= front * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        m_Camera->position -= right * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        m_Camera->position += right * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        m_Camera->position += up * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        m_Camera->position -= up * velocity;
    }
}

void VulkanWindow::Run() {
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
    m_CurrentScreenSize = {WIDTH, HEIGHT};

    m_Buffer = std::make_unique<Buffer>();
    m_Camera = std::make_unique<Camera>();
    m_Camera->UpdateTarget();

    m_AllocationTracker = std::make_unique<ResourceTracker>();

    m_Instance = std::make_unique<vk::raii::Instance>(
        m_InstanceFactory->Build_Instance(m_Context, instanceExtensions, validationLayers));
    m_DebugMessenger = std::make_unique<vk::raii::DebugUtilsMessengerEXT>(
        m_DebugMessengerFactory->Build_DebugMessenger(*m_Instance));
    CreateSurface();
    m_PhysicalDevice = std::make_unique<vk::raii::PhysicalDevice>(
        PhysicalDevicePicker::ChoosePhysicalDevice(*m_Instance));
    m_Device = std::make_unique<vk::raii::Device>(m_LogicalDeviceFactory->Build_Device(*m_PhysicalDevice, m_Surface));

    CreateVmaAllocator();

    m_DescriptorSetFactory = std::make_unique<DescriptorSetFactory>(*m_Device);
    m_FrameDescriptorSetLayout = std::make_unique<vk::raii::DescriptorSetLayout>(
        std::move(
            m_DescriptorSetFactory
            ->AddBinding(0, vk::DescriptorType::eUniformBuffer,
                         vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
            .AddBinding(1, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eFragment)
            .AddBinding(2, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eFragment)
            .AddBinding(3, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eFragment)
            .AddBinding(4, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eFragment)
            .AddBinding(5, vk::DescriptorType::eUniformBuffer,
                        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
            .AddBinding(6, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eFragment)

            .Build()
        )
    );

    m_DescriptorSetFactory->ResetFactory();

    m_UniformBufferInfo = m_Buffer->CreateMapped(m_VmaAllocator, sizeof(MVP),
                                                 vk::BufferUsageFlagBits::eUniformBuffer,
                                                 VMA_MEMORY_USAGE_CPU_TO_GPU,
                                                 VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                                                 m_AllocationTracker.get(), "MVP");

    m_ShadowUBOBufferInfo = m_Buffer->CreateMapped(m_VmaAllocator, sizeof(ShadowMVP),
                                                   vk::BufferUsageFlagBits::eUniformBuffer,
                                                   VMA_MEMORY_USAGE_CPU_TO_GPU,
                                                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                                                   m_AllocationTracker.get(), "ShadowMVP");

    m_PointLightBufferInfo = m_Buffer->CreateMapped(
        m_VmaAllocator,
        sizeof(PointLight) * m_PointLights.size(),
        vk::BufferUsageFlagBits::eStorageBuffer,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        m_AllocationTracker.get(),
        "PointLights"
    );

    m_DirectionalLightBufferInfo = m_Buffer->CreateMapped(
        m_VmaAllocator,
        sizeof(DirectionalLight) * m_DirectionalLights.size(),
        vk::BufferUsageFlagBits::eStorageBuffer,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        m_AllocationTracker.get(),
        "DirectionalLights"
    );


    Buffer::UploadData(m_PointLightBufferInfo, m_PointLights.data(), sizeof(PointLight) * m_PointLights.size());
    Buffer::UploadData(m_DirectionalLightBufferInfo, m_DirectionalLights.data(),
                       sizeof(DirectionalLight) * m_DirectionalLights.size());


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


    m_Sampler = std::make_unique<vk::raii::Sampler>(*m_Device, samplerInfo);

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

    uint32_t QueueIdx = m_LogicalDeviceFactory->FindQueueFamilyIndex(*m_PhysicalDevice, m_Surface,
                                                                     vk::QueueFlagBits::eGraphics);
    VkQueue rawQueue = *m_Device->getQueue(QueueIdx, 0);
    m_GraphicsQueue = std::make_unique<vk::raii::Queue>(*m_Device, rawQueue);

    m_CmdPool = std::make_unique<vk::raii::CommandPool>(m_Renderer->CreateCommandPool(*m_Device, QueueIdx));

    auto swapImg = m_SwapChain->getImages();
    m_SwapChainImages.resize(swapImg.size());

    for (size_t i = 0; i < swapImg.size(); i++) {
        m_SwapChainImages[i].image = swapImg[i];
        m_SwapChainImages[i].imageAspectFlags = vk::ImageAspectFlagBits::eColor;
    }

    m_DepthPass = std::make_unique<DepthPass>(*m_Device, m_CommandBuffers);

    m_DepthPass->CreateImage(m_VmaAllocator, m_VmaAllocatorsDeletionQueue, m_AllocationTracker.get(),
                             m_DepthImageFactory->GetFormat(), m_SwapChainFactory->Extent.width,
                             m_SwapChainFactory->Extent.height);

    m_GBufferPass = std::make_unique<GBufferPass>(*m_Device, m_CommandBuffers);
    m_GBufferPass->CreateGBuffer(m_VmaAllocator, m_VmaAllocatorsDeletionQueue, m_AllocationTracker.get(),
                                 m_SwapChainFactory->Extent.width, m_SwapChainFactory->Extent.height);

    m_ShadowPass = std::make_unique<ShadowPass>(*m_Device, m_CommandBuffers);
    m_ShadowPass->CreateShadowResources(m_VmaAllocator, m_VmaAllocatorsDeletionQueue, m_AllocationTracker.get(), m_ShadowResolution.x, m_ShadowResolution.y);

    LoadMesh();

    m_GlobalDescriptorSetLayout = std::make_unique<vk::raii::DescriptorSetLayout>(
        std::move(
            m_DescriptorSetFactory
            ->AddBinding(0, vk::DescriptorType::eSampler, vk::ShaderStageFlagBits::eFragment)
            .AddBinding(1, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eFragment,
                        static_cast<uint32_t>(m_ImageResource.size()))
            .AddBinding(2, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eFragment, 1)
            .AddBinding(3, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eFragment, 1)
            .AddBinding(4, vk::DescriptorType::eSampler, vk::ShaderStageFlagBits::eFragment)

            .Build()
        )
    );

    m_DescriptorSets = std::make_unique<DescriptorSets>(*m_Device);
    m_DescriptorSets->CreateGlobalDescriptorSet(
        **m_GlobalDescriptorSetLayout, *m_Sampler,
        std::make_pair(m_PointLightBufferInfo, static_cast<uint32_t>(m_PointLights.size())),
        std::make_pair(m_DirectionalLightBufferInfo, static_cast<uint32_t>(m_DirectionalLights.size())),
        m_ImageResource,
        m_SwapChainImageViews, m_ShadowPass->GetSampler()
    );


    m_DescriptorSets->CreateFrameDescriptorSet(**m_FrameDescriptorSetLayout, m_GBufferPass->GetImageViews(),
                                               m_DepthPass->GetImageView(), m_UniformBufferInfo, m_ShadowUBOBufferInfo,
                                               m_ShadowPass->GetImageView());

    m_GBufferPass->m_DescriptorSets = m_DescriptorSets->GetDescriptorSets(m_CurrentFrame);
    m_DepthPass->m_DescriptorSets = m_DescriptorSets->GetDescriptorSets(m_CurrentFrame);
    m_ShadowPass->m_DescriptorSets = m_DescriptorSets->GetDescriptorSets(m_CurrentFrame);

    CreatePipelineLayout();
    m_GBufferPass->m_PipelineLayout = **m_PipelineLayout;
    m_DepthPass->m_PipelineLayout = **m_PipelineLayout;
    m_ShadowPass->m_PipelineLayout = **m_PipelineLayout;

    m_GBufferPass->SetMeshes(m_Meshes);
    m_DepthPass->SetMeshes(m_Meshes);
    m_ShadowPass->SetMeshes(m_Meshes);

    std::pair Format = {m_SwapChainFactory->Format.format, m_DepthImageFactory->GetFormat()};

    m_DepthPass->CreatePipeline(static_cast<uint32_t>(m_ImageResource.size()), Format);
    m_GBufferPass->CreatePipeline(static_cast<uint32_t>(m_ImageResource.size()), m_DepthImageFactory->GetFormat());
    m_ShadowPass->CreatePipeline(static_cast<uint32_t>(m_ImageResource.size()), m_DepthImageFactory->GetFormat());

    // create color pass
    std::pair lights = {m_DirectionalLights, m_PointLights};
    m_ColorPass = std::make_unique<ColorPass>(*m_Device, **m_PipelineLayout, m_CommandBuffers, lights, Format);
    m_ColorPass->m_DescriptorSets = m_DescriptorSets->GetDescriptorSets(m_CurrentFrame);
    m_ShadowPass->m_DescriptorSets = m_DescriptorSets->GetDescriptorSets(m_CurrentFrame);


    CreateCommandBuffers();


    PrepareFrame();

    BeginCommandBuffer();
    UpdateShadowUBO();
    m_ShadowPass->DoPass(m_CurrentFrame, m_ShadowResolution.x, m_ShadowResolution.y);
    EndCommandBuffer();
    SubmitFrame();
    uint32_t imageIndex = AcquireSwapchainImage();
    PresentFrame(imageIndex);


    m_VmaAllocatorsDeletionQueue.emplace_back([&](VmaAllocator) {
        Buffer::Destroy(m_VmaAllocator, m_UniformBufferInfo.m_Buffer, m_UniformBufferInfo.m_Allocation,
                        m_AllocationTracker.get());
        Buffer::Destroy(m_VmaAllocator, m_PointLightBufferInfo.m_Buffer, m_PointLightBufferInfo.m_Allocation,
                        m_AllocationTracker.get());
        Buffer::Destroy(m_VmaAllocator, m_DirectionalLightBufferInfo.m_Buffer,
                        m_DirectionalLightBufferInfo.m_Allocation, m_AllocationTracker.get());
        Buffer::Destroy(m_VmaAllocator, m_ShadowUBOBufferInfo.m_Buffer, m_ShadowUBOBufferInfo.m_Allocation,
                        m_AllocationTracker.get());
    });

    m_AllocationTracker->PrintAllocations();
}

void VulkanWindow::DrawFrame() {
    UpdateUBO();
    int width, height;
    glfwGetFramebufferSize(m_Window, &width, &height);
    m_CurrentScreenSize = glm::vec2(width, height);

    HandleFramebufferResize(width, height);

    PrepareFrame();
    uint32_t imageIndex = AcquireSwapchainImage();

    BeginCommandBuffer();

    TransitionInitialLayouts(imageIndex);

    ImageFactory::ShiftImageLayout(
        *m_CommandBuffers[m_CurrentFrame],
        m_DepthPass->GetImage(),
        vk::ImageLayout::eDepthStencilAttachmentOptimal,
        vk::AccessFlagBits::eNone,
        vk::AccessFlagBits::eDepthStencilAttachmentWrite,
        vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eEarlyFragmentTests
    );

    m_DepthPass->DoPass(m_CurrentFrame, width, height);

    ImageFactory::ShiftImageLayout(
        *m_CommandBuffers[m_CurrentFrame],
        m_DepthPass->GetImage(),
        vk::ImageLayout::eDepthReadOnlyOptimal,
        vk::AccessFlagBits::eDepthStencilAttachmentWrite,
        vk::AccessFlagBits::eShaderRead,
        vk::PipelineStageFlagBits::eLateFragmentTests,
        vk::PipelineStageFlagBits::eFragmentShader
    );


    m_GBufferPass->DoPass(m_DepthPass->GetImageView(), m_CurrentFrame, width, height);
    m_GBufferPass->PrepareImagesForRead(m_CurrentFrame);


    ImageFactory::ShiftImageLayout(
        *m_CommandBuffers[m_CurrentFrame],
        m_ShadowPass->GetImage(),
        vk::ImageLayout::eDepthReadOnlyOptimal,
        vk::AccessFlagBits::eDepthStencilAttachmentWrite,
        vk::AccessFlagBits::eShaderRead,
        vk::PipelineStageFlagBits::eLateFragmentTests,
        vk::PipelineStageFlagBits::eFragmentShader
    );

    m_ColorPass->DoPass(m_SwapChainFactory->m_ImageViews, m_CurrentFrame, imageIndex, width, height);

    TransitionForPresentation(imageIndex);

    EndCommandBuffer();

    SubmitFrame();

    PresentFrame(imageIndex);

    m_CurrentFrame = (m_CurrentFrame + 1) % m_FramesInFlight;
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

    m_DepthPass->RecreateImage(m_VmaAllocator, m_VmaAllocatorsDeletionQueue, m_AllocationTracker.get(),
                               std::get<1>(m_DepthPass->GetFormat()), width, height);
    m_GBufferPass->RecreateGBuffer(m_VmaAllocator, m_VmaAllocatorsDeletionQueue, m_AllocationTracker.get(),
                                   m_SwapChainFactory->Extent.width, m_SwapChainFactory->Extent.height);

    auto transitionCmd = m_Renderer->CreateCommandBuffer(*m_Device, *m_CmdPool);
    transitionCmd.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
    m_GBufferPass->WindowResizeShiftLayout(transitionCmd);
    m_DepthPass->WindowResizeShiftLayout(transitionCmd);
    transitionCmd.end();

    vk::SubmitInfo submitInfo{};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &*transitionCmd;

    // Submit and wait for completion
    m_GraphicsQueue->submit(submitInfo, VK_NULL_HANDLE);
    m_GraphicsQueue->waitIdle();


    m_DescriptorSets->CreateDescriptorPool();
    m_DescriptorSets->CreateGlobalDescriptorSet(
        **m_GlobalDescriptorSetLayout, *m_Sampler,
        std::make_pair(m_PointLightBufferInfo, static_cast<uint32_t>(m_PointLights.size())),
        std::make_pair(m_DirectionalLightBufferInfo, static_cast<uint32_t>(m_DirectionalLights.size())),
        m_ImageResource,
        m_SwapChainImageViews, m_ShadowPass->GetSampler()
    );
    m_DescriptorSets->CreateFrameDescriptorSet(*m_FrameDescriptorSetLayout, m_GBufferPass->GetImageViews(),
                                               m_DepthPass->GetImageView(), m_UniformBufferInfo, m_ShadowUBOBufferInfo,
                                               m_ShadowPass->GetImageView());

    m_GBufferPass->m_DescriptorSets = m_DescriptorSets->GetDescriptorSets(m_CurrentFrame);
    m_DepthPass->m_DescriptorSets = m_DescriptorSets->GetDescriptorSets(m_CurrentFrame);
    m_ColorPass->m_DescriptorSets = m_DescriptorSets->GetDescriptorSets(m_CurrentFrame);

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

void VulkanWindow::BeginCommandBuffer() const {
    m_CommandBuffers[m_CurrentFrame]->reset();
    m_CommandBuffers[m_CurrentFrame]->begin(vk::CommandBufferBeginInfo());
}

void VulkanWindow::TransitionInitialLayouts(uint32_t imageIndex) {
    ImageFactory::ShiftImageLayout(*m_CommandBuffers[m_CurrentFrame],
                                   m_SwapChainImages[imageIndex], vk::ImageLayout::eColorAttachmentOptimal,
                                   vk::AccessFlagBits::eNone, vk::AccessFlagBits::eColorAttachmentWrite,
                                   vk::PipelineStageFlagBits::eTopOfPipe,
                                   vk::PipelineStageFlagBits::eColorAttachmentOutput);
}


void VulkanWindow::TransitionForPresentation(uint32_t imageIndex) {
    ImageFactory::ShiftImageLayout(*m_CommandBuffers[m_CurrentFrame],
                                   m_SwapChainImages[imageIndex], vk::ImageLayout::ePresentSrcKHR,
                                   vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eNone,
                                   vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                   vk::PipelineStageFlagBits::eBottomOfPipe);
}

void VulkanWindow::EndCommandBuffer() const {
    m_CommandBuffers[m_CurrentFrame]->end();
}

void VulkanWindow::SubmitFrame() const {
    vk::SubmitInfo submitInfo{};
    vk::Semaphore waitSemaphores[] = {*m_ImageAvailableSemaphore};
    vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
    vk::Semaphore signalSemaphores[] = {*m_RenderFinishedSemaphore};

    vk::CommandBuffer cmd = *m_CommandBuffers[m_CurrentFrame];
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

void VulkanWindow::CreateSemaphoreAndFences() {
    m_ImageAvailableSemaphore = std::make_unique<vk::raii::Semaphore>(*m_Device, vk::SemaphoreCreateInfo());
    m_RenderFinishedSemaphore = std::make_unique<vk::raii::Semaphore>(*m_Device, vk::SemaphoreCreateInfo());
    m_RenderFinishedFence = std::make_unique<vk::raii::Fence>(
        *m_Device, vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));
}

void VulkanWindow::LoadMesh() {
    m_MeshFactory = std::make_unique<MeshFactory>();
    m_MeshCmdBuffer = std::make_unique<vk::raii::CommandBuffer>(
        std::move(m_Renderer->CreateCommandBuffer(*m_Device, *m_CmdPool)));

    m_Meshes = m_MeshFactory->LoadModelFromGLTF("models/sponza/Sponza.gltf",
                                                m_VmaAllocator, m_VmaAllocatorsDeletionQueue,
                                                **m_MeshCmdBuffer, *m_GraphicsQueue, *m_Device,
                                                *m_CmdPool, m_ImageResource,
                                                m_SwapChainImageViews, m_AllocationTracker.get());
}

void VulkanWindow::CreatePipelineLayout() {
    std::vector<vk::DescriptorSetLayout> DescriptorSetLayouts{
        *m_FrameDescriptorSetLayout, *m_GlobalDescriptorSetLayout
    };

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

void VulkanWindow::CreateCommandBuffers() {
    for (size_t frames{}; frames < m_FramesInFlight; ++frames) {
        m_CommandBuffers.emplace_back(
            std::make_unique<vk::raii::CommandBuffer>(m_Renderer->CreateCommandBuffer(*m_Device, *m_CmdPool)));
    }
}
