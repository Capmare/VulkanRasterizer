//
// Created by capma on 6/20/2025.
//

#include "PhysicalDevicePicker.h"

#include <iostream>
#include <ostream>

vk::raii::PhysicalDevice PhysicalDevicePicker::ChoosePhysicalDevice(const vk::raii::Instance& instance) {
    std::vector<vk::raii::PhysicalDevice> PhysicalDevices = instance.enumeratePhysicalDevices();

    if (PhysicalDevices.empty()) {
        throw std::runtime_error("No Vulkan-compatible physical devices found.");
    }

    std::vector<const char*> RequiredExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    for (const auto& PhysicalDevice : PhysicalDevices) {
        if (IsDeviceSuitable(PhysicalDevice, RequiredExtensions)) {
            std::cout << "Selected device: " << PhysicalDevice.getProperties().deviceName << std::endl;
            return PhysicalDevice;
        }
    }

    throw std::runtime_error("No suitable Vulkan physical device found.");
}

bool PhysicalDevicePicker::SupportsExtension(const vk::raii::PhysicalDevice& PhysicalDevice,
                                             const std::vector<const char*>& RequestedExtensions) {
    std::vector<vk::ExtensionProperties> ExtensionProperties = PhysicalDevice.enumerateDeviceExtensionProperties();

    for (const char* Requested : RequestedExtensions) {
        std::cout << "Requested extension: " << Requested << "\n";

        bool Supported = false;
        for (const auto& Extension : ExtensionProperties) {
            std::cout << "Supported extension: " << Extension.extensionName << "\n";
            if (strcmp(Extension.extensionName, Requested) == 0) {
                Supported = true;
                break;
            }
        }

        if (!Supported) {
            std::cout << "Extension NOT supported: " << Requested << "\n";
            return false;
        }
    }

    return true;
}

bool PhysicalDevicePicker::IsDeviceSuitable(const vk::raii::PhysicalDevice& device,
                                            const std::vector<const char*>& requiredExtensions) {
    if (!SupportsExtension(device, requiredExtensions)) {
        return false;
    }


    return true;
}
