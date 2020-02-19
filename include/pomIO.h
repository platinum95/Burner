#include <stdint.h>
#include <stdbool.h>

// Initialise IO given values in the system configuration
int pomIoInit();

// Clean up any IO-related items
int pomIoDestroy();

// Reset the IO (mainly window) with new configuration values
int pomIoReset();

// Return the extensions required by the window handler as an
// array of strings. Number of extensions stored in _ecount
const char ** pomIoGetWindowExtensions( uint32_t * _ecount );

// Check if the window manager is telling us to close
bool pomIoShouldClose();

// Poll for IO events
int pomIoPoll();