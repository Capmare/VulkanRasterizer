//
// Created by capma on 6/21/2025.
//

#ifndef LOGICALDEVICEFACTORY_H
#define LOGICALDEVICEFACTORY_H
#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_raii.hpp"


class LogicalDeviceFactory {
public:
    LogicalDeviceFactory() = default;
    virtual ~LogicalDeviceFactory() = default;

    LogicalDeviceFactory(const LogicalDeviceFactory&) = delete;
    LogicalDeviceFactory(LogicalDeviceFactory&&) noexcept = delete;
    LogicalDeviceFactory& operator=(const LogicalDeviceFactory&) = delete;
    LogicalDeviceFactory& operator=(LogicalDeviceFactory&&) noexcept = delete;


    vk::raii::Device Build_Device(const vk::raii::PhysicalDevice& PhysicalDevice, vk::SurfaceKHR Surface);
private:
    uint32_t FindQueueFamilyIndex(const vk::raii::PhysicalDevice& PhysicalDevice, vk::SurfaceKHR Surface, vk::QueueFlags QueueFlags);

};



#endif //LOGICALDEVICEFACTORY_H
