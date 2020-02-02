#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "common.h"


#define LOG( log, ... ) LOG_MODULE( DEBUG, config, log, ##__VA_ARGS__ )

void createDefaultConfig( SystemConfig * _config ){
    _config->initialised = true;
    pomMapInit( &_config->mapCtx, 0 );
}

int loadSystemConfig( const char * path, SystemConfig * config ){
    // Quick sanity check on the config and path
    if( !( path && config ) ){
        return 1;
    }

    // Read in contents of file to a buffer
    FILE * configFile = fopen( path, "r" );
    if( !configFile ){
        LOG( "Error opening config file %s", path );
        createDefaultConfig( config );
        return 1;
    }

    // TODO - evaluate the errors here
    int err = fseek( configFile, 0, SEEK_END );
    
    long fLen = ftell( configFile );
    err |= fseek( configFile, 0, SEEK_SET );

    if( err ){
        LOG( "Error getting config file size" );
        createDefaultConfig( config );
        return 1;
    }

    char * configStr = ( char * ) malloc( sizeof( char ) * ( fLen + 1 ) );
    long cRead = fread( configStr, 1, fLen, configFile );

    if( cRead != fLen ){
        LOG( "Error occurred when reading contents of config file %s", path );
        fclose( configFile );
        free( configStr );
        createDefaultConfig( config );
        return 1;
    }
    configStr[ fLen ] = '\0';

    // Finished with the file
    if( fclose( configFile ) ){
        LOG( "Error closing config file!" );
        free( configStr );
        createDefaultConfig( config );
        return 1;
    }
    
    
    pomMapInit( &config->mapCtx, 0 );
    int lastEnd = 0;
    int lastSep = 0;
    int numPairs = 0;
    bool inComment = false;
    // Search through string, adding key-value pairs as they are found
    for( int cIdx = 0; cIdx < fLen + 1; cIdx++ ){
        // If we've found a newline or EOF, check to see if we have a valid key-value pair
        if( configStr[ cIdx ] == '\n' || configStr[ cIdx ] == '\0' ){
            // Set newline to nullchar for easier line splitting
            configStr[ cIdx ] = '\0';
            if( inComment ){
                inComment = false;
                lastEnd = cIdx + 1;
                continue;
            }

            // Check for empty lines and keyless pairs.
            // Both occur when the number of spaces between
            // the last newline and the last seperator is
            // less than 1. Empy values are allowed
            if( lastSep - lastEnd > 1 ){
                numPairs++;
                const char * keyStart = &configStr[ lastEnd ];
                const char * valueStart = &configStr[ lastSep ];
                // Add the key-value pair to list
                //getKeyValue( &(config->keyPairArray), keyStart, valueStart );
                pomMapSet( &config->mapCtx, keyStart, valueStart );
                LOG( "Adding %s:%s", keyStart, valueStart );
            }
            lastEnd = cIdx + 1;
        }
        // If we've found a separator, mark its location
        else if( configStr[ cIdx ] == ':' ){
            lastSep = cIdx + 1;
            // Set seperator to nullchar for easier splitting
            configStr[ cIdx ] = '\0';
        }
        else if( configStr[ cIdx ] == '#' ){
            // We're in comment-town
            inComment = true;
        }
    }

    config->initialised = true;
    free( configStr );
    return 0;
}


int saveSystemConfig( SystemConfig * _config, const char * path ){
    // Optimise the map to consolidate all the data
    pomMapOptimise( &_config->mapCtx );
    size_t mapDataSize, mapDataUsed;
    const char * mapData = pomMapGetDataHeap( &_config->mapCtx, &mapDataUsed, &mapDataSize );
    
    // Copy the data so we can modify it
    char * mapDataCpy = (char*) malloc( sizeof( char ) * mapDataUsed );
    memcpy( mapDataCpy, mapData, mapDataUsed );

    // At this point we have to trust that the heap data is in
    // a consistent format, i.e. key<null>value<null>key<null>value<null> etc.
    bool onKey = true;
    for( uint32_t i = 0; i < mapDataUsed; i++ ){
        char * currChar = &mapDataCpy[ i ];
        if( *currChar == '\0' ){
            if( onKey ){
                // Reached null-terminator of key, so add separator
                *currChar = ':';
            }
            else{
                // Reached end of key/value pair, add newline
                *currChar = '\n';
            }
            onKey = !onKey;
        }
    }
    
    LOG( "Writing system config to %s", path );

    // Write to file
    FILE * configFile = fopen( path, "w" );
    if( !configFile ){
        LOG( "Error opening config file to write: %s", path );
        return 1;
    }

    long cWrote = fwrite( mapDataCpy, 1, mapDataUsed, configFile );

    if( cWrote != mapDataUsed ){
        LOG( "Error occurred when writing config file %s", path );
    }

    // Finished with the file
    if( fclose( configFile ) ){
        LOG( "Error closing config file!" );
    }

    free( mapDataCpy );

    return 0;

}


int clearSystemConfig( SystemConfig * config ){
    if( config->initialised ){
        pomMapClear( &config->mapCtx );
        config->initialised = false;
        return 0;
    }
    return 1;
}


/*
// Search for a key/value from a given node. If key is found, return pointer to the pair.
// If key is not found, add a new pair if (and only if) `value` is not NULL
ConfigKeyPair * getKeyValue( ConfigKeyPair * node, const char * key, const char * value ){
    ConfigKeyPair ** childNode;
    const char * currNodeKey = node->key;
    int strComp = strcmp( currNodeKey, key );
    if( strComp == 0 ){
        // Found the key
        return node;
    }
    else if( strComp > 0 ){
        childNode = &node->left;
    }
    else{
        childNode = &node->right;
    }

    if( !*childNode ){
        // Child does not exist in list, so key is not in list
        if( !value ){
            // Not adding a new node
            return NULL;
        }
        // Add new node
        ConfigKeyPair * newNode = (ConfigKeyPair*) malloc( sizeof( ConfigKeyPair ) );
        newNode->left = NULL;
        newNode->right = NULL;
        newNode->key = key;
        newNode->value = value;
        *childNode = newNode;
        return newNode;
    }
    
    return getKeyValue( *childNode, key, value );
}
*/
