#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>

#define LOG_MODULE( level, module, log, ... ) printf( #level " (" #module "): " log "\n", ##__VA_ARGS__ );

#ifdef UNUSED
#elif defined(__GNUC__)
# define UNUSED(x) UNUSED_ ## x __attribute__((unused))
#elif defined(__LCLINT__)
# define UNUSED(x) /*@unused@*/ x
#else
# define UNUSED(x) x
#endif

#ifndef BURNER_VERSION_MAJOR
#define BURNER_VERSION_MAJOR 0
#endif //BURNER_VERSION_MAJOR
#ifndef BURNER_VERSION_MINOR
#define BURNER_VERSION_MINOR 0
#endif //BURNER_VERSION_MINOR
#ifndef BURNER_VERSION_PATCH
#define BURNER_VERSION_PATCH 0
#endif //BURNER_VERSION_PATCH
#ifndef BURNER_NAME
#define BURNER_NAME "Burner"
#endif //BURNER_NAME
#ifndef DEFAULT_WIDTH
#define DEFAULT_WIDTH "800"
#endif //DEFAULT_WIDTH
#ifndef DEFAULT_HEIGHT
#define DEFAULT_HEIGHT "600"
#endif //DEFAULT_HEIGHT
#ifndef DEFAULT_VK_VALIDATION_LAYERS
#define DEFAULT_VK_VALIDATION_LAYERS "VK_LAYER_KHRONOS_validation"
#endif //DEFAULT_HEIGHT

// TODO - move this def to somewhere more common
typedef struct PomCommonNode PomCommonNode;

struct PomCommonNode{
    union{
        PomCommonNode * _Atomic aNext;
        PomCommonNode * next;
    };
    void *data;
};

#endif // COMMON_H
