#include <stdbool.h>
#include "hashmap.h"


typedef struct SystemConfig{
    bool initialised;
    //TODO change this name
    int numPairs;
    PomMapCtx mapCtx;
}SystemConfig;

int loadSystemConfig( const char * path, SystemConfig * config );

int saveSystemConfig( SystemConfig * _config, const char * path );

int clearSystemConfig( SystemConfig * config );