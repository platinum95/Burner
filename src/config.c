#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

ConfigKeyPair * getKeyValue( ConfigKeyPair ** node, const char * key, const char * value );

void createDefaultConfig( SystemConfig * _config ){
    _config->initialised = true;
    _config->keyPairArray = NULL;
}

int loadSystemConfig( const char * path, SystemConfig * config ){
    // Quick sanity check on the config and path
    if( !( path && config ) ){
        return 1;
    }

    // Read in contents of file to a buffer
    FILE * configFile = fopen( path, "r" );
    if( !configFile ){
        printf( "Error opening config file %s\n", path );
        createDefaultConfig( config );
        return 1;
    }

    // TODO - evaluate the errors here
    int err = fseek( configFile, 0, SEEK_END );
    
    long fLen = ftell( configFile );
    err |= fseek( configFile, 0, SEEK_SET );

    if( err ){
        printf( "Error getting config file size\n" );
        createDefaultConfig( config );
        return 1;
    }

    char * configStr = ( char * ) malloc( sizeof( char ) * ( fLen + 1 ) );
    long cRead = fread( configStr, 1, fLen, configFile );

    if( cRead != fLen ){
        printf( "Error occurred when reading contents of config file %s\n", path );
        fclose( configFile );
        free( configStr );
        createDefaultConfig( config );
        return 1;
    }
    configStr[ fLen ] = '\0';

    // Finished with the file
    if( fclose( configFile ) ){
        printf( "Error closing config file!\n" );
        free( configStr );
        createDefaultConfig( config );
        return 1;
    }
    
    // Create empty head of tree
    config->keyPairArray = NULL;

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
                getKeyValue( &(config->keyPairArray), keyStart, valueStart );
                printf( "Adding %s:%s\n", keyStart, valueStart );
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
    return 0;
}

// Search for a key/value from a given node. If key is found, return pointer to the pair.
// If key is not found, add a new pair if (and only if) `value` is not NULL
ConfigKeyPair * getKeyValue( ConfigKeyPair ** node, const char * key, const char * value ){

    if( !*node ){
        // Node does not exist in list, so key is not in list
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
        *node = newNode;
        return newNode;
    }

    // If current node is valid, find next node (or return if key match)
    ConfigKeyPair ** childNode;
    const char * currNodeKey = (*node)->key;
    int strComp = strcmp( currNodeKey, key );
    if( strComp == 0 ){
        // Found the key
        return *node;
    }
    else if( strComp > 0 ){
        childNode = &(*node)->left;
    }
    else{
        childNode = &(*node)->right;
    }
    
    return getKeyValue( childNode, key, value );
}

// Search for a config value. If key doesn't exist, add it if value is not NULL
ConfigKeyPair * getConfigValue( SystemConfig * config, const char * key, const char * value ){
    if( !config->initialised ){
        // Quick sanity check on the map context
        return NULL;
    }
    return getKeyValue( &(config->keyPairArray), key, value );

}

void freeMap( ConfigKeyPair * node ){
    if( !node ){
        return;
    }
    freeMap( node->left );
    freeMap( node->right );
    free( node->left );
    free( node->right );
    return;
}

int clearSystemConfig( SystemConfig * config ){
    if( config->initialised ){
        free( config->stringStr );
        freeMap( config->keyPairArray );
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
