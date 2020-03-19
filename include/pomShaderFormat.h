#ifndef POM_SHADER_FORMAT_H
#define POM_SHADER_FORMAT_H

#include "common.h"
#include <stdint.h>
#include <vulkan/vulkan.h>

typedef struct PomShaderFormat PomShaderFormat;
typedef struct PomShaderAttributeInfo PomShaderAttributeInfo;
typedef struct PomShaderDescriptorInfo PomShaderDescriptorInfo;

typedef enum PomShaderDataTypes{
    SHADER_FLOAT = 0,
    SHADER_INT8  = 1,
    SHADER_INT16 = 2,
    SHADER_INT32 = 3,
    SHADER_INT64 = 4,
    // TODO - etc etc etc
    SHADER_VEC2 = 5,
    SHADER_VEC3 = 6,
    SHADER_VEC4 = 7,

    SHADER_DATATYPE_UNKNOWN = 8,
    SHADER_DATATYPE_END = SHADER_DATATYPE_UNKNOWN,
    SHADER_DATATYPE_START = SHADER_FLOAT,
    SHADER_DATATYPE_RANGE = SHADER_DATATYPE_END - SHADER_DATATYPE_START
}PomShaderDataTypes;

// Represents a single attribute (e.g. position, normal, etc.)
struct PomShaderAttributeInfo{
    char *nameOffset;
    PomShaderDataTypes dataType;
    //size_t sizeBytes;
    uint32_t location;
};

struct PomShaderDescriptorInfo{
    char *nameOffset;
    size_t unpaddedSizeBytes;
    uint32_t set;
    uint32_t binding;
    VkDescriptorType type;
};

struct PomShaderFormat{
    // ShaderNameOffset is always the start of the data block
    char *shaderNameOffset; //8

    uint64_t numAttributeInfo; //4
    PomShaderAttributeInfo *attributeInfoOffset; //8

    // TODO - consider splitting this into model/rendergroup/program-local descriptors
    uint64_t numDescriptorInfo; //4
    PomShaderDescriptorInfo *descriptorInfoOffset; //8

    size_t shaderBytecodeSizeBytes; //8
    uint32_t *shaderBytecodeOffset; //8

    size_t dataBlockSize; //8
};

// Turn absolute pointers to system memory into relative offsets into the format block
int pomShaderFormatRelativisePointers( PomShaderFormat *_format, uint8_t *_dataBlock );

// Turn relative offsets into the format block into absolute pointers to system memory
int pomShaderFormatAbsolutisePointers( PomShaderFormat *_format );

// Write Shader Format to disk. Function will relativise pointers in the format.
// Returns number of bytes written if success, 0 on failure
size_t pomShaderFormatWrite( const char *_path, PomShaderFormat *_format, void *_dataBlock );

// Load Shader Format from disk. Function will absolutise pointers during loading.
// Returns number of bytes loaded on success, 0 on failure
size_t pomShaderFormatLoad( const char *_path, PomShaderFormat **_format );

#endif // POM_SHADER_FORMAT_H