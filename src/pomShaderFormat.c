#include "pomShaderFormat.h"
#include <stdlib.h>

#define LOG( lvl, log, ... ) LOG_MODULE( lvl, PomShaderFormat, log, ##__VA_ARGS__ )

// Set pointer pointed to by _ptr to an offset relative to _format pointer.
// Returns 0 on success, 1 otherwise
static int _priv_relativisePointer( const PomShaderFormat *_format, uint8_t ** _ptr, 
                                    uint8_t *_dataBlockStart ){
    uint8_t *ptr = *_ptr;
    if( ptr < _dataBlockStart || ptr >= _dataBlockStart + _format->dataBlockSize ){
        // Pointer couldn't be within data block
        LOG( ERR, "Invalid pointer to relativise" );
        return 1;
    }
    size_t ptrOffsetInBlock = (*_ptr) - _dataBlockStart;
    size_t offsetFromBase = ptrOffsetInBlock + sizeof( PomShaderFormat );
    
    *_ptr = (void*)offsetFromBase;
    return 0;
}

// Set offset from _format pointed to by _ptr to an absolute pointer.
// Returns 0 on success, 1 otherwise
static int _priv_absolutisePointer( const PomShaderFormat *_format, uint8_t ** _ptr ){
    uintptr_t offset = (uintptr_t) *_ptr;
    if( offset < sizeof( PomShaderFormat ) ){
        // Pointer within header
        LOG( ERR, "Invalid pointer to relativise" );
        return 1;
    }
    *_ptr = (void*) ( ( (uint8_t*) _format ) + offset );
    return 0;
}

// Turn absolute pointers to system memory into relative offsets into the format block
int pomShaderFormatRelativisePointers( PomShaderFormat *_format, uint8_t *_dataBlock ){
    if( _priv_relativisePointer( _format, (void*)&_format->shaderNameOffset, _dataBlock ) ){
        LOG( ERR, "Failed to relativise shader name" );
        return 1;
    }
    // Adjust attribute/descriptor name pointers
    for( uint32_t i = 0; i < _format->numAttributeInfo; i++ ){
        PomShaderAttributeInfo *attrInfo = &_format->attributeInfoOffset[ i ];
        if( _priv_relativisePointer( _format, (void*)&attrInfo->nameOffset, _dataBlock ) ){
            LOG( ERR, "Failed to relativise attribute name" );
            return 1;
        }
    }
    for( uint32_t i = 0; i < _format->numDescriptorInfo; i++ ){
        PomShaderDescriptorInfo *descInfo = &_format->descriptorInfoOffset[ i ];
        if( _priv_relativisePointer( _format, (void*)&descInfo->nameOffset, _dataBlock ) ){
            LOG( ERR, "Failed to relativise descriptor name" );
            return 1;
        }
    }
    // Adjust the attribute/descriptor/bytecode blocks
    if( _priv_relativisePointer( _format, (void*)&_format->attributeInfoOffset, _dataBlock ) ){
            LOG( ERR, "Failed to relativise attribute infos" );
            return 1;
    }
    if( _priv_relativisePointer( _format, (void*)&_format->descriptorInfoOffset, _dataBlock ) ){
            LOG( ERR, "Failed to relativise descriptor infos" );
            return 1;
    }
    if( _priv_relativisePointer( _format, (void*)&_format->shaderBytecodeOffset, _dataBlock ) ){
            LOG( ERR, "Failed to relativise shader bytecode block" );
            return 1;
    }
    return 0;
}

// Turn relative offsets into the format block into absolute pointers to system memory
int pomShaderFormatAbsolutisePointers( PomShaderFormat *_format ){
    // Adjust the attribute/descriptor/bytecode blocks
    if( _priv_absolutisePointer( _format, (void*)&_format->attributeInfoOffset ) ){
            LOG( ERR, "Failed to absolutise attribute infos" );
            return 1;
    }
    if( _priv_absolutisePointer( _format, (void*)&_format->descriptorInfoOffset ) ){
            LOG( ERR, "Failed to absolutise descriptor infos" );
            return 1;
    }
    if( _priv_absolutisePointer( _format, (void*)&_format->shaderBytecodeOffset ) ){
            LOG( ERR, "Failed to absolutise shader bytecode block" );
            return 1;
    }

    // Adjust attribute/descriptor name pointers
    for( uint32_t i = 0; i < _format->numAttributeInfo; i++ ){
        PomShaderAttributeInfo *attrInfo = &_format->attributeInfoOffset[ i ];
        if( _priv_absolutisePointer( _format, (void*)&attrInfo->nameOffset ) ){
            LOG( ERR, "Failed to absolutise attribute name" );
            return 1;
        }
    }
    for( uint32_t i = 0; i < _format->numDescriptorInfo; i++ ){
        PomShaderDescriptorInfo *descInfo = &_format->descriptorInfoOffset[ i ];
        if( _priv_absolutisePointer( _format, (void*)&descInfo->nameOffset ) ){
            LOG( ERR, "Failed to absolutise descriptor name" );
            return 1;
        }
    }
    if( _priv_absolutisePointer( _format, (void*)&_format->shaderNameOffset ) ){
        LOG( ERR, "Failed to absolutise shader name" );
        return 1;
    }

    return 0;
}


// Write Shader Format to disk. Function will relativise pointers in the format
size_t pomShaderFormatWrite( const char *_path, PomShaderFormat *_format, void *_dataBlock  ){
    if( pomShaderFormatRelativisePointers( _format, (uint8_t*) _dataBlock ) ){
        // No need to log.
        return 0;
    }
    FILE *outputFile = fopen( _path, "w" );
    if( !outputFile ){
        LOG( ERR, "Failed to open output file" );
        return 0;
    }
    int toRet = 0;
    size_t headerWritten = fwrite( _format, sizeof( PomShaderFormat ), 1, outputFile );
    if( headerWritten != 1 ){
        LOG( ERR, "Failed to write shader format header" );
        toRet = 0;
        goto WriteErr;
    }
    size_t dataBlockSize = _format->dataBlockSize;
    size_t dataBytesWritten = fwrite( _dataBlock, sizeof( uint8_t ), dataBlockSize, outputFile );

    if( dataBytesWritten != dataBlockSize ){
        LOG( ERR, "Failed to write shader data block" );
        toRet = 0;
        goto WriteErr;
    }
    toRet = ( headerWritten * sizeof( PomShaderFormat ) ) + dataBytesWritten;
WriteErr:
    if( fclose( outputFile ) ){
        LOG( ERR, "Failed to close output file!" );
        return 0;
    }
    return toRet;
}

// Load Shader Format from disk. Function will absolutise pointers during loading
size_t pomShaderFormatLoad( const char *_path, PomShaderFormat **_format ){
    FILE *inputFile = fopen( _path, "r" );
    if( !inputFile ){
        LOG( ERR, "Failed to open shader blob file %s", _path );
        return 0;
    }

    fseek( inputFile, 0, SEEK_END );
    size_t fSize = ftell( inputFile );
    fseek( inputFile, 0, SEEK_SET );

    if( !fSize ){
        LOG( WARN, "Shader blob file read as empty" );
        fclose( inputFile );
        return 0;
    }
    if( fSize < sizeof( PomShaderFormat ) ){
        LOG( ERR, "Shader blob file does not contain header" );
        fclose( inputFile );
        return 0;
    }

    void *fileData = (void*) malloc( fSize );

    // Read the header into memory
    PomShaderFormat *format = (PomShaderFormat*) fileData;
    size_t headerRead = fread( format, sizeof( PomShaderFormat ), 1, inputFile );
    if( headerRead != 1 ){
        LOG( ERR, "Shader blob file does not contain header" );
        fclose( inputFile );
        free( fileData );
        return 0;
    }
    size_t dataBlockExpectedSize = fSize - sizeof( PomShaderFormat );
    if( dataBlockExpectedSize != format->dataBlockSize ){
        LOG( ERR, "Shader blob file size incosistent with header block description" );
        fclose( inputFile );
        free( fileData );
        return 0;
    }

    void *dataBlock = (uint8_t*)fileData + sizeof( PomShaderFormat );
    size_t dataBytesRead = fread( dataBlock, sizeof( uint8_t ), dataBlockExpectedSize, inputFile );
    if( dataBytesRead + sizeof( PomShaderFormat ) != fSize ){
        LOG( ERR, "Failed to correctly read shader blob %s", _path );
        free( fileData );
        fclose( inputFile );
        return 0;
    }
    if( fclose( inputFile ) ){
        LOG( WARN, "Failed to close shader blob file %s", _path );
    }

    if( pomShaderFormatAbsolutisePointers( format ) ){
        LOG( ERR, "Failed to absolutise shader blob pointers for file %s", _path );
        free( fileData );
        return 0;
    }

    LOG( DEBUG, "Successfully loaded shader blob %s", _path );
    *_format = format;
    return fSize;
}