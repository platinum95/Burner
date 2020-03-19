// Output baked blob containing SPIRV bytecode and a header with information about the shader

#include "common.h"
#include "pomShaderFormat.h"
#include <shaderc/shaderc.h>
#include <string.h>
#include "cmore/linkedlist.h"
#include "cmore/pstring.h"
#include <stdlib.h>
#include <limits.h>

#define LOG( lvl, log, ... ) LOG_MODULE( lvl, ShaderBake, log, ##__VA_ARGS__ )

static char* loadFile( const char *_path, size_t *_sourceLength );
static uint32_t *compileShader( const char *_shaderSrc, size_t _shaderSourceLength,
                                const char *_sourceName,
                                size_t *_shaderBlobSizeBytes );
static int parseShaderInterface( char *_shaderSource, size_t _sourceLength,
                                 PomShaderFormat *_format );
// Take the scattered data we've dynamically allocated and turn it into a nice contiguous block.
// Returns number of bytes in the block on success, 0 on failure
static size_t contiguifyData( PomShaderFormat *_format );


int main( int argc, char * argv[] ){
    int toRet = 0;
    if( argc != 3 ){
        LOG( ERR, "Usage: shaderbake <shader glsl path> <blob output path>" );
        toRet = 1;
        goto initialisationError;
    }
    char *shaderPath = argv[ 1 ];
    const char *outputPath = argv[ 2 ];
    LOG( INFO, "Baking shader %s", shaderPath );

    // Start by loading the shader source
    size_t shaderSourceLength = 0;
    char *shaderSrc = loadFile( shaderPath, &shaderSourceLength );
    if( !shaderSrc ){
        // No need to log, loadFile should have done that
        toRet = 1;
        goto initialisationError;
    }

    // Compile the shader to bytecode
    size_t shaderBinarySizeBytes;
    uint32_t *shaderBytecode = compileShader( shaderSrc, shaderSourceLength, shaderPath,
                                              &shaderBinarySizeBytes );
    if( !shaderBytecode ){
        // No need to log
        toRet = 1;
        goto postSourceLoadError;
    }
    
    PomShaderFormat format = { 0 };
    format.shaderNameOffset = shaderPath;
    format.shaderBytecodeOffset = shaderBytecode;
    format.shaderBytecodeSizeBytes = shaderBinarySizeBytes;
    
    // Parse for attributes and descriptors
    if( parseShaderInterface( shaderSrc, shaderSourceLength, &format ) ){
        LOG( ERR, "Failed to parse shader interface" );
        toRet = 1;
        goto finalStageError;
    }

    // Transfer all the data we've created so far into a contiguous block
    size_t dataBlockSize = contiguifyData( &format );
    if( !dataBlockSize ){
        LOG( ERR, "Failed to contiguify shader data" );
        toRet = 1;
        goto postSourceLoadError;
    }
    format.dataBlockSize = dataBlockSize;

    size_t expectedSize = sizeof( PomShaderFormat ) + dataBlockSize;
    void *dataBlock = (void*) format.shaderNameOffset;
    if( pomShaderFormatWrite( outputPath, &format, dataBlock ) != 
            expectedSize ){
        LOG( ERR, "Did not write expected number of bytes to output file" );
        toRet = 1;
        goto finalStageError;
    }

    toRet = 0;
finalStageError:
    free( dataBlock );
postSourceLoadError:
    free( shaderSrc );
initialisationError:
    return toRet;
}

uint32_t *compileShader( const char *_shaderSrc, size_t _shaderSourceLength, const char *_sourceName,
                         size_t *_shaderBlobSizeBytes ){
    shaderc_compiler_t compiler = shaderc_compiler_initialize();
    if( !compiler ){
        LOG( ERR, "Failed to create shader compiler" );
        return NULL;
    }
    // TODO - Allow entry point selection
    // TODO - Allow options
    // Note that we always infer shader type from source - i.e. we require a pragma annotation
    shaderc_compilation_result_t compileResult =
        shaderc_compile_into_spv( compiler, _shaderSrc, _shaderSourceLength,
                                  shaderc_glsl_infer_from_source, _sourceName, "main", NULL );
    uint32_t numErrors = shaderc_result_get_num_errors( compileResult );
    uint32_t numWarnings = shaderc_result_get_num_errors( compileResult );
    uint32_t *toReturn;
    LOG( INFO, "Shader Compile, %u errors, %u warnings", numErrors, numWarnings );
    shaderc_compilation_status compileStatus = shaderc_result_get_compilation_status( compileResult );
    if( compileStatus != shaderc_compilation_status_success ){
        if( compileStatus == shaderc_compilation_status_invalid_stage ){
            LOG( ERR, "Could not infer shader type. Are you sure #pragma shader_stage(<type>) is"\
                      " present in shader?" );
        }
        else{
            const char *errorMsg = shaderc_result_get_error_message( compileResult );
            LOG( ERR, "Failed to compile shader %s. Error: %s", _sourceName, errorMsg );
        }
        toReturn = NULL;
        
    }
    else{
        uint32_t *compiledShader = (uint32_t*) shaderc_result_get_bytes( compileResult );
        size_t shaderLength = shaderc_result_get_length( compileResult );
        *_shaderBlobSizeBytes = shaderLength;
        // Copy the shader data
        toReturn = (uint32_t*) malloc( shaderLength );
        memcpy( toReturn, compiledShader, shaderLength );
    }
    shaderc_result_release( compileResult );
    shaderc_compiler_release( compiler );
    return toReturn;
}


char * loadFile( const char *_path, size_t *_sourceLength ){
    FILE *inputFile = fopen( _path, "r" );
    if( !inputFile ){
        LOG( ERR, "Failed to open shader file %s", _path );
        return NULL;
    }

    fseek( inputFile, 0, SEEK_END );
    size_t fSize = ftell( inputFile );
    fseek( inputFile, 0, SEEK_SET );

    if( !fSize ){
        LOG( WARN, "Shader source file read as empty" );
        fclose( inputFile );
        return NULL;
    }

    const size_t charSize = sizeof( char ); // Unnecessary, just here because maybe someday wchar
    if( fSize % charSize != 0 ){
        LOG( ERR, "Shader source filesize not a multiple of character width" );
        fclose( inputFile );
        return NULL;
    }

    const uint32_t numChars = fSize / charSize;
    *_sourceLength = numChars;
    char *fileStr = (char*) malloc( fSize + 1 );
    size_t nRead = fread( fileStr, sizeof( char ), numChars, inputFile );

    if( nRead != numChars ){
        LOG( WARN, "Shader file load did not read as expected. Compilation may fail" );
    }

    if( fclose( inputFile ) ){
        LOG( WARN, "Failed to close input shader file" );
    }
    // Add null-terminator at end
    fileStr[ numChars ] = '\0';

    return fileStr;
}

// Takes an input source string and an existing pointer to a line & length within that string.
// Sets line to the next line in the string, and sets lineLength to the string length of the line.
// Returns 0 if no error, 1 if EOF, 2 if error
static int _getNextLine( char *_sourceString, size_t _sourceLength,
                         char ** line, size_t *_lineLength ){
    // Bounds check
    if( *line < _sourceString ){
        // Weird error
        LOG( ERR, "Invalid line pointer for _getNextLine" );
        return 2;
    }
    // Check if we're at EOF (EoS?)
    char * nextLineStart = *line + *_lineLength;
    size_t nextLineOffset = nextLineStart - _sourceString;
    if(  nextLineOffset >= _sourceLength ){
        // End of string
        return 2;
    }
    // Find the next non-EOL character (or EOF)
    do{
        char c = *nextLineStart;
        if( c == '\n' || c == '\r' ){
            // Ignore these
            continue;
        }
        else if( c == '\0' ){
            // EOS
            return 1;
        }
        else{
            // Found start of next line
            break;
        }
    }while( nextLineStart++ );

    // Now find the length of this string
    size_t lineLen = 0;
    do{
        char c = nextLineStart[ lineLen ];
        if( c == '\n' || c == '\r' || c == '\0' ){
            break;
        }
        lineLen++;
    }while( lineLen );
    
    *_lineLength = lineLen;
    *line = nextLineStart;
    return 0;
}

// Take a given start of parameter, and scan to find the start of the next parameter.
// Returns 0 if success, or 1 if EOL found before next parameter.
// Call with _attrStart pointing to start of a parameter. 
// If successful, _paramStart will be set to the start of the next parameter.
// _lastParamLen is set to the number of characters in the last parameter
static int _findNextParameter( char ** _paramStart, size_t *_lastParamLen ){
    char *paramStart = *_paramStart;
    size_t lastParamLen = 0;
    while( ++paramStart && ++lastParamLen ){
        if( *paramStart == ' ' ){
            break;
        }else if( *paramStart == '\0' ){
            // EOL found
            *_lastParamLen = lastParamLen;
            return 1;
        }
    }
    *_lastParamLen = lastParamLen;
    // If we get here, paramStart points to the space immediately following the last parameter.
    // Scan for the start of the next parameter now
    while( paramStart++ ){
        if( *paramStart == '\0' ){
            // EOL
            return 1;
        }else if( *paramStart != ' ' ){
            break;
        }
    }
    // Next param start found
    *_paramStart = paramStart;
    return 0;
}

static PomShaderDataTypes _parseAttrDataType( const char * _typeStr, size_t _typeLen ){
    if( strlen( "SHADER_FLOAT" ) == _typeLen && !strncmp( "SHADER_FLOAT", _typeStr, _typeLen ) ){
        return SHADER_FLOAT;
    }
    if( strlen( "SHADER_INT8" ) == _typeLen && !strncmp( "SHADER_INT8", _typeStr, _typeLen ) ){
        return SHADER_INT8;
    }
    if( strlen( "SHADER_INT16" ) == _typeLen && !strncmp( "SHADER_INT16", _typeStr, _typeLen ) ){
        return SHADER_INT16;
    }
    if( strlen( "SHADER_INT32" ) == _typeLen && !strncmp( "SHADER_INT32", _typeStr, _typeLen ) ){
        return SHADER_INT32;
    }
    if( strlen( "SHADER_INT64" ) == _typeLen && !strncmp( "SHADER_INT64", _typeStr, _typeLen ) ){
        return SHADER_INT64;
    }
    if( strlen( "SHADER_VEC2" ) == _typeLen && !strncmp( "SHADER_VEC2", _typeStr, _typeLen ) ){
        return SHADER_VEC2;
    }
    if( strlen( "SHADER_VEC3" ) == _typeLen && !strncmp( "SHADER_VEC3", _typeStr, _typeLen ) ){
        return SHADER_VEC3;
    }
    if( strlen( "SHADER_VEC4" ) == _typeLen && !strncmp( "SHADER_VEC4", _typeStr, _typeLen ) ){
        return SHADER_INT8;
    }

    return SHADER_DATATYPE_UNKNOWN;
}

// Attempt to parse a given line that may contain a valid Attribute declaration.
// Return 0 if valid declaraion was found, 1 if no valid declaration, and 2 if mangled declaration
static int _parseAttributeLine( char *_attrLine, PomShaderAttributeInfo *_attrInfo ){
    char *attrStart;
    if( !pomStringFind( _attrLine, "POM_ATTRIBUTE", &attrStart ) ){
        // Could not find attribute keyword
        return 1;
    }
    // attrStart points to the start of POM_DESCRIPTOR in the line.
    // Format should be `POM_ATTRIBUTE <NAME(string)> <LOCATION(uint)> <TYPE(enum)>
    
    // Lets first attempt to get offsets to these points in the string
    size_t attrDeclParamLen = strlen( "POM_ATTRIBUTE" );
    size_t nameLen;
    size_t locationLen;
    size_t typeLen;

    char *nameStart = attrStart;
    if( _findNextParameter( &nameStart, &attrDeclParamLen ) ){
        LOG( WARN, "Failed to find name in attribute declaration" );
        return 2;
    }
    // Quick assertion on the parameter length
    if( attrDeclParamLen != strlen( "POM_ATTRIBUTE" ) ){
        LOG( WARN, "_findNextParameter not working as expected" );
    }
    
    char *locationStart = nameStart;
    if( _findNextParameter( &locationStart, &nameLen ) ){
        LOG( WARN, "Failed to find location in attribute declaration" );
        return 2;
    }
    
    char *typeStart = locationStart;
    if( _findNextParameter( &typeStart, &locationLen ) ){
        LOG( WARN, "Failed to find type in attribute declaration" );
        return 2;
    }

    char *eolStart = typeStart;
    // Doesn't matter if next call fails, just need length of type parameter
    _findNextParameter( &eolStart, &typeLen );
    

    // Now start parsing the parameters.

    // Try to parse the location string
    char locationBuffer[ 11 ];
    if( locationLen > 10 ){
        LOG( WARN, "Location parameter incorrectly extracted" );
        return 2;
    }
    memcpy( locationBuffer, locationStart, locationLen );
    locationBuffer[ locationLen ] = '\0';
    long int parsedLocation = strtol( locationBuffer, NULL, 10 );
    // strtol returns 0, LONG_MAX, or LONG_MIN if error occurred. Need to check for these.
    if( parsedLocation == LONG_MAX || parsedLocation < 0 ||
        ( parsedLocation == 0 && locationBuffer[ 0 ] != '0' ) ){
        LOG( WARN, "Failed to parse location parameter" );
        return 2;
    }
    // Location should be nicely parsed now
    // TODO - can also check for endptr in strtol to make sure all the string was a number
    _attrInfo->location = (uint32_t) parsedLocation;

    // Next parse the data type
    _attrInfo->dataType = _parseAttrDataType( typeStart, typeLen );
    if( _attrInfo->dataType == SHADER_DATATYPE_UNKNOWN ){
        LOG( WARN, "Failed to parse attribute datatype" );
        return 2;
    }

    // Name requires no parsing, we'll just copy the string to new memory
    // TODO - don't forget to free this later
    _attrInfo->nameOffset = (char*) calloc( nameLen + 1, sizeof( char ) );
    memcpy( _attrInfo->nameOffset, nameStart, sizeof( char ) * nameLen );

    return 0;
}

static PomShaderDataTypes _parseDescriptorType( const char * _typeStr, size_t _typeLen ){
    if( strlen( "VK_DESCRIPTOR_TYPE_SAMPLER" ) == _typeLen && !strncmp( "VK_DESCRIPTOR_TYPE_SAMPLER", _typeStr, _typeLen ) ){
        return VK_DESCRIPTOR_TYPE_SAMPLER;
    }
    if( strlen( "VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER" ) == _typeLen && !strncmp( "VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER", _typeStr, _typeLen ) ){
        return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    }
    if( strlen( "VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE" ) == _typeLen && !strncmp( "VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE", _typeStr, _typeLen ) ){
        return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    }
    if( strlen( "VK_DESCRIPTOR_TYPE_STORAGE_IMAGE" ) == _typeLen && !strncmp( "VK_DESCRIPTOR_TYPE_STORAGE_IMAGE", _typeStr, _typeLen ) ){
        return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    }
    if( strlen( "VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER" ) == _typeLen && !strncmp( "VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER", _typeStr, _typeLen ) ){
        return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    }
    if( strlen( "VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER" ) == _typeLen && !strncmp( "VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER", _typeStr, _typeLen ) ){
        return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
    }
    if( strlen( "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER" ) == _typeLen && !strncmp( "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER", _typeStr, _typeLen ) ){
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }
    if( strlen( "VK_DESCRIPTOR_TYPE_STORAGE_BUFFER" ) == _typeLen && !strncmp( "VK_DESCRIPTOR_TYPE_STORAGE_BUFFER", _typeStr, _typeLen ) ){
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    }
    if( strlen( "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC" ) == _typeLen && !strncmp( "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC", _typeStr, _typeLen ) ){
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    }
    if( strlen( "VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC" ) == _typeLen && !strncmp( "VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC", _typeStr, _typeLen ) ){
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    }
    if( strlen( "VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT" ) == _typeLen && !strncmp( "VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT", _typeStr, _typeLen ) ){
        return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    }
    if( strlen( "VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT" ) == _typeLen && !strncmp( "VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT", _typeStr, _typeLen ) ){
        return VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT;
    }
    if( strlen( "VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV" ) == _typeLen && !strncmp( "VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV", _typeStr, _typeLen ) ){
        return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;
    }

    return VK_DESCRIPTOR_TYPE_MAX_ENUM;
}

// Attempt to parse a given line that may contain a valid Descriptor declaration.
// Return 0 if valid declaraion was found, 1 if no valid declaration, and 2 if mangled declaration
static int _parseDescriptorLine( char *_descLine, PomShaderDescriptorInfo *_descInfo ){
    char *descStart;
    if( !pomStringFind( _descLine, "POM_DESCRIPTOR", &descStart ) ){
        // Could not find decriptor keyword
        return 1;
    }
    // descStart points to the start of POM_DESCRIPTOR in the line.
    // Format should be:
    // `POM_DESCRIPTOR <NAME(string)> <DESCRIPTOR_TYPE(enum)> <SET(uint)> <BINDING(uint)> <SIZE_UNPADDED(size_t)>
    
    // Lets first attempt to get offsets to these points in the string
    size_t descDeclParamLen = strlen( "POM_DESCRIPTOR" );
    size_t nameLen;
    size_t typeLen;
    size_t setLen;
    size_t bindingLen;
    size_t sizeLen;

    char *nameStart = descStart;
    if( _findNextParameter( &nameStart, &descDeclParamLen ) ){
        LOG( WARN, "Failed to find name in attribute declaration" );
        return 2;
    }
    // Quick assertion on the parameter length
    if( descDeclParamLen != strlen( "POM_DESCRIPTOR" ) ){
        LOG( WARN, "_findNextParameter not working as expected" );
    }
    
    char *typeStart = nameStart;
    if( _findNextParameter( &typeStart, &nameLen ) ){
        LOG( WARN, "Failed to find type in attribute declaration" );
        return 2;
    }
    
    char *setStart = typeStart;
    if( _findNextParameter( &typeStart, &typeLen ) ){
        LOG( WARN, "Failed to find set in attribute declaration" );
        return 2;
    }

    char *bindingStart = setStart;
    if( _findNextParameter( &bindingStart, &setLen ) ){
        LOG( WARN, "Failed to find binding in attribute declaration" );
        return 2;
    }

    char *sizeStart = bindingStart;
    if( _findNextParameter( &sizeStart, &bindingLen ) ){
        LOG( WARN, "Failed to find size in attribute declaration" );
        return 2;
    }

    char *eolStart = sizeStart;
    // Doesn't matter if next call fails, just need length of type parameter
    _findNextParameter( &eolStart, &sizeLen );
    

    // Now start parsing the parameters.

    // Try to parse the set string
    char setBuffer[ 11 ];
    if( setLen > 10 ){
        LOG( WARN, "Set parameter incorrectly extracted" );
        return 2;
    }
    memcpy( setBuffer, setStart, setLen );
    setBuffer[ setLen ] = '\0';
    long int parsedSet = strtol( setBuffer, NULL, 10 );
    // strtol returns 0, LONG_MAX, or LONG_MIN if error occurred. Need to check for these.
    if( parsedSet == LONG_MAX || parsedSet < 0 ||
        ( parsedSet == 0 && setBuffer[ 0 ] != '0' ) ){
        LOG( WARN, "Failed to parse set parameter" );
        return 2;
    }
    // Set should be nicely parsed now
    // TODO - can also check for endptr in strtol to make sure all the string was a number
    _descInfo->set = (uint32_t) parsedSet;

    // Similarly, try to parse the binding string
    char bindingBuffer[ 11 ];
    if( bindingLen > 10 ){
        LOG( WARN, "Binding parameter incorrectly extracted" );
        return 2;
    }
    memcpy( bindingBuffer, bindingStart, bindingLen );
    bindingBuffer[ bindingLen ] = '\0';
    long int parsedBinding = strtol( bindingBuffer, NULL, 10 );
    // strtol returns 0, LONG_MAX, or LONG_MIN if error occurred. Need to check for these.
    if( parsedBinding == LONG_MAX || parsedBinding < 0 ||
        ( parsedBinding == 0 && bindingBuffer[ 0 ] != '0' ) ){
        LOG( WARN, "Failed to parse binding parameter" );
        return 2;
    }
    // Set should be nicely parsed now
    // TODO - can also check for endptr in strtol to make sure all the string was a number
    _descInfo->binding = (uint32_t) parsedBinding;

    // Next parse the data type
    _descInfo->type = _parseDescriptorType( typeStart, typeLen );
    if( _descInfo->type == VK_DESCRIPTOR_TYPE_MAX_ENUM ){
        LOG( WARN, "Failed to parse descriptor type" );
        return 2;
    }


    // Name requires no parsing, we'll just copy the string to new memory
    // TODO - don't forget to free this later
    _descInfo->nameOffset = (char*) calloc( nameLen + 1, sizeof( char ) );
    memcpy( _descInfo->nameOffset, nameStart, sizeof( char ) * nameLen );

    return 0;
}



static int parseShaderInterface( char *_shaderSource, size_t _sourceLength,
                                 PomShaderFormat *_format ){
    int toReturn = 0;
    PomLinkedListCtx attrInfoList = { 0 };
    PomLinkedListCtx descInfoList = { 0 };

    pomLinkedListInit( &attrInfoList );
    pomLinkedListInit( &descInfoList );


    // Need to have *info structs ready to go, so create initial ones here
    PomShaderAttributeInfo *nextAttrInfo =
        (PomShaderAttributeInfo*) calloc( 1, sizeof( PomShaderAttributeInfo ) );
    PomShaderDescriptorInfo *nextDescInfo =
        (PomShaderDescriptorInfo*) calloc( 1, sizeof( PomShaderDescriptorInfo ) );
    // Read the source in line-by-line
    char * currLine = _shaderSource;
    size_t lineLength = 0;
    int nextLineRet;
    while( ( nextLineRet = _getNextLine( _shaderSource, _sourceLength, &currLine, &lineLength ) ) != 2 ){
        // Set EOL to nullchar so that we can treat this line as it's own string. Set back when done
        char eolChar = currLine[ lineLength ];
        currLine[ lineLength ] = '\0';

        // currLine is now a null-terminated string representing the current line, and can be
        // treated as such

        // Check for attribute declaration
        int parseAttrRet = _parseAttributeLine( currLine, nextAttrInfo );
        if( parseAttrRet == 0 ){
            LOG( DEBUG, "Adding attribute info for %s", nextAttrInfo->nameOffset );
            pomLinkedListAdd( &attrInfoList, (PllKeyType) nextAttrInfo );
            nextAttrInfo = (PomShaderAttributeInfo*) calloc( 1, sizeof( PomShaderAttributeInfo ) );
        } else if( parseAttrRet == 2 ){
            // Should probably give a better warning here
            LOG( WARN, "Mangled descriptor declaration" );
        }

        // Check for descriptor declaration
        int parseDescRet = _parseDescriptorLine( currLine, nextDescInfo );
        if( parseDescRet == 0 ){
            LOG( DEBUG, "Adding descriptor info for %s", nextDescInfo->nameOffset );
            pomLinkedListAdd( &attrInfoList, (PllKeyType) nextDescInfo );
            nextDescInfo = (PomShaderDescriptorInfo*) calloc( 1, sizeof( PomShaderDescriptorInfo ) );
        } else if( parseDescRet == 2 ){
            LOG( WARN, "Mangled descriptor declaration" );
        }

        
        // Reset the EOL char
        if( nextLineRet != 1 ){
        currLine[ lineLength ] = eolChar;
        }
        if( nextLineRet == 1 ){
            // Last line, can break
            break;
        }
    }
    free( nextAttrInfo );
    free( nextDescInfo );

    // Now copy any info declarations we found into a single block
    size_t infoBlockSize = ( attrInfoList.size * sizeof( PomShaderAttributeInfo ) ) +
                           ( descInfoList.size * sizeof( PomShaderDescriptorInfo ) );

    uint8_t *infoBlock = (void*) malloc( infoBlockSize );
    uint8_t *currOffset = infoBlock;
    _format->attributeInfoOffset = (PomShaderAttributeInfo*) currOffset;
    _format->numAttributeInfo = attrInfoList.size;

    PllKeyType nKey;
    while( pomLinkedListPopFront( &attrInfoList, &nKey ) ){
        PomShaderAttributeInfo *attrInfo = (PomShaderAttributeInfo*) nKey;
        memcpy( currOffset, attrInfo, sizeof( PomShaderAttributeInfo ) );
        currOffset += sizeof( PomShaderAttributeInfo );
    }

    _format->descriptorInfoOffset = (PomShaderDescriptorInfo*) currOffset;
    _format->numDescriptorInfo = descInfoList.size;
    while( pomLinkedListPopFront( &descInfoList, &nKey ) ){
        PomShaderDescriptorInfo *descInfo = (PomShaderDescriptorInfo*) nKey;
        memcpy( currOffset, descInfo, sizeof( PomShaderDescriptorInfo ) );
        currOffset += sizeof( PomShaderDescriptorInfo );
    }

    pomLinkedListClear( &attrInfoList );
    pomLinkedListClear( &descInfoList );
    return toReturn;
}

static size_t contiguifyData( PomShaderFormat *_format ){
    // Find the total size required
    const size_t attribInfoSize = _format->numAttributeInfo * sizeof( PomShaderAttributeInfo );
    const size_t descInfoSize = _format->numDescriptorInfo * sizeof( PomShaderDescriptorInfo );
    const size_t shaderSize = _format->shaderBytecodeSizeBytes;
    const size_t shaderNameLen = strlen( _format->shaderNameOffset ) + 1;
    size_t nameStringSize = shaderNameLen;
    // Count the string length for each attribute information
    for( uint32_t i = 0; i < _format->numAttributeInfo; i++ ){
        nameStringSize += strlen( _format->attributeInfoOffset[ i ].nameOffset ) + 1;
    }
    // Count the string length for each descriptor information
    for( uint32_t i = 0; i < _format->numDescriptorInfo; i++ ){
        nameStringSize += strlen( _format->descriptorInfoOffset[ i ].nameOffset ) + 1;
    }

    const size_t totalBlockSize = attribInfoSize + descInfoSize + nameStringSize + shaderSize;
    uint8_t *dataBlock = (void*) malloc( totalBlockSize );
    uint8_t *currOffset = dataBlock;

    // Start copying data into our new block.
    // Start with strings, freeing as necessary
    
    memcpy( currOffset, _format->shaderNameOffset, shaderNameLen );
    _format->shaderNameOffset = (char*) dataBlock;
    currOffset += shaderNameLen;
    for( uint32_t i = 0; i < _format->numAttributeInfo; i++ ){
        PomShaderAttributeInfo *attrInfo = &_format->attributeInfoOffset[ i ];
        const size_t attrNameLen = strlen( attrInfo->nameOffset ) + 1;
        memcpy( currOffset, attrInfo->nameOffset, attrNameLen );
        free( attrInfo->nameOffset );
        attrInfo->nameOffset = (char*) currOffset;
        currOffset += attrNameLen;
    }
    for( uint32_t i = 0; i < _format->numDescriptorInfo; i++ ){
        PomShaderDescriptorInfo *descInfo = &_format->descriptorInfoOffset[ i ];
        const size_t descNameLen = strlen( descInfo->nameOffset ) + 1;
        memcpy( currOffset, descInfo->nameOffset, descNameLen );
        free( descInfo->nameOffset );
        descInfo->nameOffset = (char*) currOffset;
        currOffset += descNameLen;
    }
    // Now copy over the Attribute and Descriptor infos
    PomShaderAttributeInfo *oldAttrDescBlock = _format->attributeInfoOffset;
    PomShaderAttributeInfo *attrInfoBlock = (PomShaderAttributeInfo*) currOffset;
    memcpy( attrInfoBlock, _format->attributeInfoOffset, attribInfoSize );
    _format->attributeInfoOffset = attrInfoBlock;
    currOffset += attribInfoSize;

    PomShaderDescriptorInfo *descInfoBlock = (PomShaderDescriptorInfo*) currOffset;
    memcpy( descInfoBlock, _format->descriptorInfoOffset, descInfoSize );
    _format->descriptorInfoOffset = descInfoBlock;
    currOffset += descInfoSize;

    // All temp copies of attribute info and descriptor info are in the same data block, pointed to
    // by the original location in format, so free that now
    free( oldAttrDescBlock );

    // Finally, copy the shader bytecode over
    uint32_t *shaderBytecodeBlock = (uint32_t*) currOffset;
    memcpy( shaderBytecodeBlock, _format->shaderBytecodeOffset, shaderSize );
    free( _format->shaderBytecodeOffset );
    _format->shaderBytecodeOffset = shaderBytecodeBlock;
    currOffset += shaderSize;

    size_t bytesCopied = currOffset - dataBlock;

    // Make sure we copied the correct number of bytes
    if( bytesCopied != totalBlockSize ){
        LOG( ERR, "Did not copy expected number of bytes during contiguification" );
        free( dataBlock );
        return 0;
    }
    return totalBlockSize;
}