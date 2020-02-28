#include <vulkan/vulkan.h>

int pomPickPhysicalDevice();

int pomDestroyPhysicalDevice();

int pomCreateLogicalDevice();

int pomDestroyLogicalDevice();

VkQueue * pomDeviceGetGraphicsQueue();

VkDevice * pomGetLogicalDevice();