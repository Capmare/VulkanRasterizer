//
// Created by capma on 6/21/2025.
//

#include "LogicalDeviceFactory.h"

#include <iostream>

uint32_t LogicalDeviceFactory::FindQueueFamilyIndex(const vk::raii::PhysicalDevice &PhysicalDevice,
                                                    vk::SurfaceKHR Surface, vk::QueueFlags QueueFlags) {

    std::vector<vk::QueueFamilyProperties> QueueFamilyProperties = PhysicalDevice.getQueueFamilyProperties();

    for (uint32_t i = 0; i < QueueFamilyProperties.size(); i++) {

        bool bCanPresent = false;
        if (Surface) {
            if (PhysicalDevice.getSurfaceSupportKHR(i,Surface)) {
                bCanPresent = true;
            }
        }
        else {
            bCanPresent = true;
        }

        if ((QueueFamilyProperties[i].queueFlags & QueueFlags) && bCanPresent) {
            return i;
        }
    }

    return static_cast<uint32_t>(-1);
}

vk::raii::Device LogicalDeviceFactory::Build_Device(const vk::raii::PhysicalDevice &PhysicalDevice, vk::SurfaceKHR Surface) {

    uint32_t QueueFamilyIndex = LogicalDeviceFactory::FindQueueFamilyIndex(PhysicalDevice, Surface, vk::QueueFlagBits::eGraphics);
    float QueuePriority = 1.f;
    vk::DeviceQueueCreateInfo QueueCreateInfo(
        vk::DeviceQueueCreateFlags(),
        QueueFamilyIndex,
        1,
        &QueuePriority
    );

    std::vector<const char*> EnabledLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    std::vector<const char*> Extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_SHADER_OBJECT_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        VK_EXT_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_EXTENSION_NAME
    };

    vk::DeviceCreateInfo DeviceCreateInfo(
        vk::DeviceCreateFlags(),
        1,
        &QueueCreateInfo,
        static_cast<uint32_t>(EnabledLayers.size()),
        EnabledLayers.data(),
        static_cast<uint32_t>(Extensions.size()),
        Extensions.data(),
        nullptr
    );

    vk::PhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT dynamicRenderingUnusedAttachmentsFeatures{};
    dynamicRenderingUnusedAttachmentsFeatures.dynamicRenderingUnusedAttachments = VK_TRUE;
    dynamicRenderingUnusedAttachmentsFeatures.pNext = nullptr;

    vk::PhysicalDeviceVulkan13Features Vulkan13Features = {};
    Vulkan13Features.dynamicRendering = VK_TRUE;
    Vulkan13Features.synchronization2 = VK_TRUE;
    Vulkan13Features.pNext = &dynamicRenderingUnusedAttachmentsFeatures;


    vk::PhysicalDeviceVulkan12Features Vulkan12Features = {};
    Vulkan12Features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    Vulkan12Features.pNext = &Vulkan13Features;

    vk::PhysicalDeviceFeatures2 Features{};
    Features.features.samplerAnisotropy = VK_TRUE;
    Features.pNext = &Vulkan12Features;


    DeviceCreateInfo.pNext = &Features;



    try {
        vk::raii::Device Device = PhysicalDevice.createDevice(DeviceCreateInfo);
        std::cout << "Device created" << std::endl;
        return Device;
    } catch (const vk::SystemError& err) {
        std::cerr << "Failed to create device: " << err.what() << std::endl;
    }

    return nullptr;
}
