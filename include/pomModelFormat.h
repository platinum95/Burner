#include <stdint.h>
#include <stddef.h>

#define POM_FORMAT_MAGIC_NUM 0xDEADBEEFDEADBEEF

// TODO - Verify cross-platform alignment on these structs
typedef struct PomModelTextureInfo PomModelTextureInfo;
typedef struct PomModelMaterialInfo PomModelMaterialInfo;
typedef struct PomModelMeshInfo PomModelMeshInfo;
typedef struct PomSubmodelInfo PomSubmodelInfo;
typedef struct PomModelInfo PomModelInfo;
typedef struct PomModelFormat PomModelFormat;

typedef uint8_t PomDataBlock;

int relativisePointers( PomModelFormat *_format, uint8_t *_dataBlock );
int absolutisePointers( PomModelFormat *_format );
int writeBakedModel( PomModelFormat *_format, uint8_t *_dataBlock, size_t _blockSize,
                     const char *filePath );
int loadBakedModel( const char *_filePath, PomModelFormat **_format,
                    uint8_t **_dataBlock );

struct PomModelTextureInfo{
    uint32_t textureId;
    const char *nameOffset;
    uint64_t textureType;
    uint64_t dataType;  // Colour, vector, data, etc (largely unimportant)
    uint32_t dataUnitSizeBytes; // vec2 vs vec4 etc
    uint32_t dataBlockSizeBytes;
    uint8_t *dataOffset;
};

struct PomModelMaterialInfo{
    uint32_t materialId;
    const char *nameOffset;
    uint64_t materialType; // Shading type
    uint8_t *paramDataOffset; // maybe replace this with struct
    uint32_t numTextures;
    uint32_t *textureIdsOffset;
};

struct PomModelMeshInfo{
    uint32_t meshId;
    const char *nameOffset;
    uint32_t numIndices;
    uint32_t numVertices;
    uint8_t *dataBlockOffset;
    uint64_t dataSize;
    uint32_t dataStride;
    uint32_t numUvCoords;
    uint32_t hasTangentSpace;
};

struct PomSubmodelInfo{
    uint32_t submodelId;
    const char *nameOffset;
    uint32_t materialId;
    uint64_t dataSize;
    float *dataOffset;
    uint64_t numIndices;
    uint32_t *indexOffset;
};

struct PomModelInfo{
    uint32_t modelId;
    const char *nameOffset;
    uint32_t numSubmodels;
    uint32_t *submodelIdsOffset;
    float *defaultMatrixOffset;
};

struct PomModelFormat{

    uint64_t magicNumber;
    const char *sceneNameOffset;
    size_t dataBlockSize;

    uint32_t numTextureInfo;
    PomModelTextureInfo *textureInfoOffset;
    
    uint32_t numMeshInfo;
    PomModelMeshInfo *meshInfoOffset;

    uint32_t numMaterialInfo;
    PomModelMaterialInfo *materialInfoOffset;

    uint32_t numSubmodelInfo;
    PomSubmodelInfo *submodelInfo;

    uint32_t numModelInfo;
    PomModelInfo *modelInfo;
};