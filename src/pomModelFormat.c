#include "common.h"
#include "pomModelFormat.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#define LOG( level, log, ... ) LOG_MODULE( level, pomModelFormat, log, ##__VA_ARGS__ )

int writeBakedModel( PomModelFormat *_format, uint8_t *_dataBlock, size_t _blockSize,
                     const char *_filePath ){
    int err = 0;
    if( relativisePointers( _format, _dataBlock ) ){
        LOG( ERR, "Failed to relativise model pointers: %s", _filePath );
        return 1;
    }

    FILE *outputFile = fopen( _filePath, "wb" );
    if( !outputFile ){
        printf( "ERR: failed to open output file %s\n", _filePath );
        return 1;
    }

    // size_t fwrite ( const void * ptr, size_t size, size_t count, FILE * stream );
    size_t headerWritten = fwrite( _format, sizeof( PomModelFormat ), 1, outputFile );
    if( headerWritten != 1 ){
        printf( "ERR: Failed to write format header\n" );
        err = 1;
        goto writeFailure;
    }

    size_t blockBytesWritten = fwrite( _dataBlock, sizeof( uint8_t ), _blockSize, outputFile );
    if( blockBytesWritten != _blockSize ){
        printf( "ERR: Failed to write data block\n" );
        err = 0;
        goto writeFailure;
    }
writeFailure:
    if( fclose( outputFile ) ){
        printf( "ERR: Failed to close output file\n" );
        return 1;
    }
    return err;
}

int loadBakedModel( const char *_filePath, PomModelFormat **_format,
                    uint8_t **_dataBlock ){
    FILE *inputFile = fopen( _filePath, "rb" );
    if( !inputFile ){
        LOG( ERR, "Failed to open baked file" );
        return 1;
    }
    fseek( inputFile, 0, SEEK_END );
    size_t fSize = ftell( inputFile );
    fseek( inputFile, 0, SEEK_SET );
    LOG( INFO, "Model %s, size %lu bytes", _filePath, fSize );

    uint8_t *modelData = (uint8_t*) malloc( fSize );
    if( !modelData ){
        LOG( ERR, "Failed to allocate memory for model %s", _filePath );
        fclose( inputFile );
        return 1;
    }
    size_t bytesRead = fread( modelData, sizeof( uint8_t ), fSize, inputFile );

    if( fclose( inputFile ) ){
        LOG( ERR, "Failed to close model file %s", _filePath );
        free( modelData );
        return 1;
    }

    if( bytesRead != fSize ){
        LOG( ERR, "Could not load model data %s", _filePath );
        free( modelData );
        return 1;
    }

    *_format = (PomModelFormat*) modelData;
    if( _dataBlock ){
        *_dataBlock = modelData + sizeof( PomModelFormat );
    }

    // Quick sanity check on data sizes
    size_t expectedSize = sizeof( PomModelFormat ) + (*_format)->dataBlockSize;
    if( expectedSize != fSize ){
        LOG( ERR, "Expected model size not same as size loaded: %s", _filePath );
        free( modelData );
        return 1;
    }
    if( absolutisePointers( *_format ) ){
        LOG( ERR, "Failed to absolutise model pointers %s", _filePath );
        free( modelData );
        return 1;
    }

    return 0;
}


// TODO - consider making this a macro
static inline uint8_t *getRelativeOffset( const uint8_t *_dataBlockStart, const uint8_t *_dataLoc ){
    const size_t formatHeaderSize = sizeof( PomModelFormat ); // Should this be static?

    ptrdiff_t byteOffset = _dataLoc - _dataBlockStart;
    if ( byteOffset < 0 ){
        // Something has gone wrong here, since _dataLoc should always be after _dataBlockStart
        printf( "ERR: Invalid data location within block\n" );
        return NULL;
    }

    size_t overallOffset = formatHeaderSize + byteOffset;
    uint8_t *relativeLoc = (uint8_t*) NULL + overallOffset;
    return relativeLoc; 
}

// TODO - consider merging this with getRelativeOffset
static inline int relativiseOffset( const uint8_t * _dataBlockStart, uint8_t ** const _dataLoc ){
    
    if( *_dataLoc == NULL ){
        // Nothing to do
        return 0;
    }
    uint8_t *newOffset = getRelativeOffset( _dataBlockStart, *_dataLoc );
    
    if( !newOffset ){
        // Failure in relative offset update
        return 1;
    }
    *_dataLoc = newOffset;
    return 0;
}

// It's important to not dereference any pointers that we adjust here after
// calling this function/
int relativisePointers( PomModelFormat *_format, uint8_t *_dataBlock ){
    // Treat location of _dataBlock as though it occurs directly after location of _format
    // since that's how it'll be in the file

    for( uint32_t i = 0; i < _format->numTextureInfo; i++ ){
        PomModelTextureInfo *texInfo = &_format->textureInfoOffset[ i ];
        if( relativiseOffset( _dataBlock, &texInfo->dataOffset ) || 
            relativiseOffset( _dataBlock, (uint8_t**)&texInfo->nameOffset ) ){
            // Failure to relativise texture info
            printf( "Failed to relativise texture info\n" );
            return 1;
        }
        
    }
    
    for( uint32_t i = 0; i < _format->numMeshInfo; i++ ){
        PomModelMeshInfo *meshInfo = &_format->meshInfoOffset[ i ];
        if( relativiseOffset( _dataBlock, &meshInfo->dataBlockOffset ) ||
            relativiseOffset( _dataBlock, (uint8_t**)&meshInfo->nameOffset ) ){
            printf( "Failed to relativise mesh info\n" );
            return 1;
        }
    }

    for( uint32_t i = 0; i < _format->numMaterialInfo; i++ ){
        PomModelMaterialInfo *materialInfo = &_format->materialInfoOffset[ i ];
        if( relativiseOffset( _dataBlock, (uint8_t**)&materialInfo->nameOffset ) ||
            relativiseOffset( _dataBlock, &materialInfo->paramDataOffset ) ||
            relativiseOffset( _dataBlock, (uint8_t**)&materialInfo->textureIdsOffset ) ){
            printf( "Failed to relativise material info\n" );
            return 1;
        }
    }

    for( uint32_t i = 0; i < _format->numSubmodelInfo; i++ ){
        PomSubmodelInfo *submodelInfo = &_format->submodelInfo[ i ];
        if( relativiseOffset( _dataBlock, (uint8_t**)&submodelInfo->nameOffset ) ||
            relativiseOffset( _dataBlock, (uint8_t**)&submodelInfo->indexOffset ) ||
            relativiseOffset( _dataBlock, (uint8_t**)&submodelInfo->dataOffset ) ){
            printf( "Failed to relativise submodel info\n" );
            return 1;
        }
    }

    for( uint32_t i = 0; i < _format->numModelInfo; i++ ){
        PomModelInfo *modelInfo = &_format->modelInfo[ i ];
        if( relativiseOffset( _dataBlock, (uint8_t**)&modelInfo->nameOffset ) ||
            relativiseOffset( _dataBlock, (uint8_t**)&modelInfo->submodelIdsOffset ) ||
            relativiseOffset( _dataBlock, (uint8_t**)&modelInfo->defaultMatrixOffset ) ){
            printf( "Failed to relativise model info\n" );
            return 1;
        }
    }

    if( relativiseOffset( _dataBlock, (uint8_t**)&_format->textureInfoOffset ) ||
        relativiseOffset( _dataBlock, (uint8_t**)&_format->meshInfoOffset ) ||
        relativiseOffset( _dataBlock, (uint8_t**)&_format->materialInfoOffset ) ||
        relativiseOffset( _dataBlock, (uint8_t**)&_format->submodelInfo ) ||
        relativiseOffset( _dataBlock, (uint8_t**)&_format->modelInfo ) ){
            printf( "Failed to relativise data block pointers\n" );
            return 1;
    }

    return 0;
}

static int absolutiseOffset( uint8_t *dataStart, uint8_t **_relativeLoc ){
    ptrdiff_t offset = (ptrdiff_t) *_relativeLoc;
    if( offset == 0 ){
        *_relativeLoc = NULL;
        return 0;
    }
    *_relativeLoc = dataStart + offset;
    return 0;
}

int absolutisePointers( PomModelFormat *_format ){
    uint8_t *dataStart = (uint8_t*) _format;
    if( absolutiseOffset( dataStart, (uint8_t**)&_format->textureInfoOffset ) ||
        absolutiseOffset( dataStart, (uint8_t**)&_format->meshInfoOffset ) ||
        absolutiseOffset( dataStart, (uint8_t**)&_format->materialInfoOffset ) ||
        absolutiseOffset( dataStart, (uint8_t**)&_format->submodelInfo ) ||
        absolutiseOffset( dataStart, (uint8_t**)&_format->modelInfo ) ){
            printf( "Failed to relativise data block pointers\n" );
            return 1;
    }

    for( uint32_t i = 0; i < _format->numTextureInfo; i++ ){
        PomModelTextureInfo *texInfo = &_format->textureInfoOffset[ i ];
        if( absolutiseOffset( dataStart, &texInfo->dataOffset ) || 
            absolutiseOffset( dataStart, (uint8_t**)&texInfo->nameOffset ) ){
            // Failure to relativise texture info
            printf( "Failed to relativise texture info\n" );
            return 1;
        }
        
    }
    
    for( uint32_t i = 0; i < _format->numMeshInfo; i++ ){
        PomModelMeshInfo *meshInfo = &_format->meshInfoOffset[ i ];
        if( absolutiseOffset( dataStart, &meshInfo->dataBlockOffset ) ||
            absolutiseOffset( dataStart, (uint8_t**)&meshInfo->nameOffset ) ){
            printf( "Failed to relativise mesh info\n" );
            return 1;
        }
    }

    for( uint32_t i = 0; i < _format->numMaterialInfo; i++ ){
        PomModelMaterialInfo *materialInfo = &_format->materialInfoOffset[ i ];
        if( absolutiseOffset( dataStart, (uint8_t**)&materialInfo->nameOffset ) ||
            absolutiseOffset( dataStart, &materialInfo->paramDataOffset ) ||
            absolutiseOffset( dataStart, (uint8_t**)&materialInfo->textureIdsOffset ) ){
            printf( "Failed to relativise material info\n" );
            return 1;
        }
    }

    for( uint32_t i = 0; i < _format->numSubmodelInfo; i++ ){
        PomSubmodelInfo *submodelInfo = &_format->submodelInfo[ i ];
        if( absolutiseOffset( dataStart, (uint8_t**)&submodelInfo->nameOffset ) ||
            absolutiseOffset( dataStart, (uint8_t**)&submodelInfo->indexOffset ) ||
            absolutiseOffset( dataStart, (uint8_t**)&submodelInfo->dataOffset ) ){
            printf( "Failed to relativise submodel info\n" );
            return 1;
        }
    }

    for( uint32_t i = 0; i < _format->numModelInfo; i++ ){
        PomModelInfo *modelInfo = &_format->modelInfo[ i ];
        if( absolutiseOffset( dataStart, (uint8_t**)&modelInfo->nameOffset ) ||
            absolutiseOffset( dataStart, (uint8_t**)&modelInfo->submodelIdsOffset ) ||
            absolutiseOffset( dataStart, (uint8_t**)&modelInfo->defaultMatrixOffset ) ){
            printf( "Failed to relativise model info\n" );
            return 1;
        }
    }

    return 0;
}