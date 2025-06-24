
#include <iostream>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>



class InstanceFactory {
public:
	InstanceFactory() = default;
	virtual ~InstanceFactory() = default;

	InstanceFactory(const InstanceFactory&) = delete;
	InstanceFactory(InstanceFactory&&) noexcept = delete;
	InstanceFactory& operator=(const InstanceFactory&) = delete;
	InstanceFactory& operator=(InstanceFactory&&) noexcept = delete;


	vk::raii::Instance Build_Instance(const vk::raii::Context &Context, std::vector<const char *> InstanceExtension, std::vector<const char *>
	                                  ValidationLayer);



};
