#include <vulkan/vulkan.h>

int pomPickPhysicalDevice();

int pomDestroyPhysicalDevice();

int pomCreateLogicalDevice();

int pomDestroyLogicalDevice();

VkQueue * pomDeviceGetGraphicsQueue();

VkDevice * pomGetLogicalDevice();

VkFormat * pomGetSwapchainImageFormat();

VkExtent2D * pomGetSwapchainExtent();

VkImage * pomGetSwapchainImages( uint32_t *numImages );