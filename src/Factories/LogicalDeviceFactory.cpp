//
// Created by capma on 6/21/2025.
//

#include "LogicalDeviceFactory.h"

#include <iostream>

uint32_t LogicalDeviceFactory::FindQueueFamilyIndex(const vk::raii::PhysicalDevice &PhysicalDevice,
                                                    vk::SurfaceKHR Surface, vk::QueueFlags QueueFlags) {

    std::vector<vk::QueueFamilyProperties> QueueFamilyProperties = PhysicalDevice.getQueueFamilyProperties();

    // check queue flags against the queue tag and return first suitable one
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

        if (QueueFamilyProperties[i].queueFlags & QueueFlags) {
            return i;
        }
    }

    return static_cast<uint32_t>(-1);
}

vk::raii::Device LogicalDeviceFactory::Build_Device(const vk::raii::PhysicalDevice &PhysicalDevice, vk::SurfaceKHR Surface) {

    // find the graphics queue
    uint32_t QueueFamilyIndex = LogicalDeviceFactory::FindQueueFamilyIndex(PhysicalDevice,Surface, vk::QueueFlagBits::eGraphics);
    float QueuePriority = 1.f;
    vk::DeviceQueueCreateInfo QueueCreateInfo = vk::DeviceQueueCreateInfo(
        vk::DeviceQueueCreateFlags(),
        QueueFamilyIndex,
        1,
        &QueuePriority
    );


    vk::PhysicalDeviceFeatures PhysicalDeviceFeatures = vk::PhysicalDeviceFeatures();

    const char** EnabledLayers = static_cast<const char**>(malloc(sizeof(const char *)));
    EnabledLayers[0] = "VK_LAYER_KHRONOS_validation";

    const char** Extension = static_cast<const char**>(malloc(sizeof(const char *)));
    Extension[0] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

    vk::DeviceCreateInfo DeviceCreateInfo = vk::DeviceCreateInfo(
        vk::DeviceCreateFlags(),
        1,
        &QueueCreateInfo,
        1,
        EnabledLayers
        ,1,
        Extension,
        &PhysicalDeviceFeatures
        );

    try {
        vk::raii::Device Device = PhysicalDevice.createDevice(DeviceCreateInfo);
        std::cout << "Device created" << std::endl;
        return Device;
    } catch (const vk::SystemError& err) {
        std::cerr << "Failed to create device: " << err.what() << std::endl;
    }
    return nullptr;
}
