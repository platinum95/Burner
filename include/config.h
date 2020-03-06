#include <stdbool.h>
#include "cmore/hashmap.h"

typedef struct SystemConfig SystemConfig;

struct SystemConfig{
    bool initialised;
    //TODO change this name
    int numPairs;
    PomMapCtx mapCtx;
};

extern SystemConfig systemConfig;

int loadSystemConfig( const char * path );

int saveSystemConfig( const char * path );

int clearSystemConfig();