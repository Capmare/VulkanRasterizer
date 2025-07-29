//
// Created by capma on 6/20/2025.
//

#ifndef PHYSICALDEVICEPICKER_H
#define PHYSICALDEVICEPICKER_H
#include "vulkan/vulkan_raii.hpp"


class PhysicalDevicePicker {

public:
    PhysicalDevicePicker() = default;
    virtual ~PhysicalDevicePicker() = default;

    PhysicalDevicePicker(const PhysicalDevicePicker&) = delete;
    PhysicalDevicePicker(PhysicalDevicePicker&&) noexcept = delete;
    PhysicalDevicePicker& operator=(const PhysicalDevicePicker&) = delete;
    PhysicalDevicePicker& operator=(PhysicalDevicePicker&&) noexcept = delete;

    static vk::raii::PhysicalDevice ChoosePhysicalDevice(const vk::raii::Instance& instance);
    static bool SupportsExtension(const vk::raii::PhysicalDevice &PhysicalDevice, const std::vector<const char *> &RequestedExtensions);
    static bool IsDeviceSuitable(const vk::raii::PhysicalDevice &device, const std::vector<const char *> &requiredExtensions);
private:


};



#endif //PHYSICALDEVICEPICKER_H
