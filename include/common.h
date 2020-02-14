#ifndef COMMON_H
#define COMMON_H

#define LOG_MODULE( level, module, log, ... ) printf( #level " (" #module "): " log "\n", ##__VA_ARGS__ );

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
