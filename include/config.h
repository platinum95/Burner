#include <stdbool.h>

typedef struct ConfigKeyPair{
    const char * key;
    const char * value;
    struct ConfigKeyPair * left;
    struct ConfigKeyPair * right;
}ConfigKeyPair;

typedef struct SystemConfig{
    bool initialised;
    //TODO change this name
    char * stringStr; // Disallow modification of the string since the map points here.
    int numPairs;
    ConfigKeyPair * keyPairArray;
}SystemConfig;

int loadSystemConfig( const char * path, SystemConfig * config );

int clearSystemConfig( SystemConfig * config );

// Search for a config value. If key doesn't exist, add it if `value` is not NULL
ConfigKeyPair * getConfigValue( SystemConfig * config, const char * key, const char * value );