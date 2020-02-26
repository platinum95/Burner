#include <stdint.h>
#include <stdbool.h>
#include <vulkan/vulkan.h>

// Initialise IO given values in the system configuration
int pomIoInit();

// Clean up any IO-related items
int pomIoDestroy();

// Reset the IO (mainly window) with new configuration values
int pomIoReset();

// Return the extensions required by the window handler as an
// array of strings. Number of extensions stored in _ecount
const char ** pomIoGetWindowExtensions( uint32_t * _ecount );

// Create the rendering surface
int pomIoCreateSurface();

// Get the render surface
VkSurfaceKHR* pomIoGetSurface();

// Get the current window resolution
const VkExtent2D *pomIoGetWindowExtent();

// Destroy the rendering surface
int pomIoDestroySurface();

// Check if the window manager is telling us to close
bool pomIoShouldClose();

// Poll for IO events
int pomIoPoll();