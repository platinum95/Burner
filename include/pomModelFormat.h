#include <stdint.h>

#define POM_FORMAT_MAGIC_NUM 0xDEADBEEFDEADBEEF

typedef struct PomModelTextureInfo PomModelTextureInfo;
typedef struct PomModelMaterialInfo PomModelMaterialInfo;
typedef struct PomModelMeshInfo PomModelMeshInfo;
typedef struct PomSubmodelInfo PomSubmodelInfo;
typedef struct PomModelInfo PomModelInfo;
typedef struct PomModelFormat PomModelFormat;

typedef uint8_t PomDataBlock;

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
    uint32_t paramDataOffset; // maybe replace this with struct
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
    uint64_t defaultMatrixOffset;
};

struct PomModelFormat{

    uint64_t magicNumber;
    const char *sceneNameOffset;

    uint32_t numTextureInfo;
    PomModelTextureInfo *textureInfoOffset;
    
    uint32_t numMaterialInfo;
    PomModelMaterialInfo *materialInfoOffset;

    uint32_t numSubmodelInfo;
    PomSubmodelInfo *submodelInfo;

    uint32_t numModelInfo;
    PomModelInfo *modelInfo;
    

    PomDataBlock dataBlock;
};