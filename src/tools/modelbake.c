#include <stdlib.h>
#include <stdio.h>
#include <assimp/scene.h>
#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include "pomModelFormat.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

#include "cmore/hashmap.h"


int loadRawModel( const char *modelPath, struct aiScene const **_scene );

static int getAllTextureSize( const struct aiScene *_scene, uint32_t *_textureCount,
                              size_t *_allTextureDataSize );
static int getMaterialSize( const struct aiScene *_scene, size_t *_materialSize );
static int getAllMeshSize( const struct aiScene *_scene, size_t *_sizeBytes );
static int populateMeshData( const struct aiMesh *mesh, PomModelMeshInfo *meshInfo,
                             uint8_t *dataBlock, size_t *bytesWritten );
static int populateTextureData( const struct aiScene *_scene, uint8_t *_texDataBlock,
                                PomModelTextureInfo* texInfos, size_t *_bytesWritten );
static int populateMaterialInfo( const struct aiScene *_scene, uint8_t *_matDataBlock,
                                 size_t *_bytesWritten );
static int getModelInfoSize( const struct aiScene *_scene, uint32_t *_numInfos, uint32_t *_numIds );
static int populateModelInfo( const struct aiScene *_scene, PomModelInfo *_modelInfoBlock,
                              uint32_t *_submodelIdArray, size_t *_bytesWritten );

const char *rawModelPath;
const char *rawModelDir;

int main( int argc, char ** argv ){
    if( argc != 3 ){
        printf( "modelbake requires a 2 paths as argument, first to raw input file and second to baked output file"
                " e.g. modelbake ./model.dae ./model.pom\n" );
        return 1;
    }
    int err = 0;
    rawModelPath = argv[ 1 ];
    const char *bakedModelPath = argv[ 2 ];

    // Load the model
    const struct aiScene *scene;
    if( loadRawModel( rawModelPath, &scene ) ){
        printf( "Failed to load input model file\n" );
        return 1;
    }

    // Get model directory since paths within model will be relative to it.
    // Amounts to getting everything up to the last separator
    size_t pathLen = strlen( rawModelPath );
    for( int32_t i = (int32_t) pathLen - 1; i >= 0 ; i-- ){
        char c = rawModelPath[ i ];
        if( c == '/' ){
            size_t dirLen = (size_t) i;
            char *dirVal = (char*) malloc( sizeof( char ) * dirLen + 2 );
            memcpy( dirVal, rawModelPath, sizeof( char ) * ( dirLen + 1 ) );
            dirVal[ dirLen + 1 ] = '\0';
            rawModelDir = dirVal;
            break;
        } 
    }
    
    /*
    * Get size of data blocks required for the output file
    */

    size_t meshBlockSize;
    if( getAllMeshSize( scene, &meshBlockSize ) ){
        printf( "ERR: Failed to get mesh block size\n" );
        err = 1;
        goto getSizeError;
    }
    printf( "Mesh block size %lu bytes\n", meshBlockSize );

    // Get texture count and block size
    size_t texSize;
    uint32_t texCount;
    if( getAllTextureSize( scene, &texCount, &texSize ) ){
        printf( "ERR: Unable to get texture info\n" );
        err = 1;
        goto getSizeError;
    }
    printf( "Texture count %u, texture size %lu bytes\n", texCount, texSize );

    printf( "Get material info block size\n" );
    size_t materialBlockSize;
    if( getMaterialSize( scene, &materialBlockSize ) ){
        printf( "Failed to get material block size\n" );
        err = 1;
        goto getSizeError;
    }

    printf( "Get model info block size\n" );
    uint32_t numModelInfos, numSubmodelIds;
    if( getModelInfoSize( scene, &numModelInfos, &numSubmodelIds ) ){
        printf( "Failed to get material block size\n" );
        err = 1;
        goto getSizeError;
    }
    size_t formatHeaderSizeBytes = sizeof( PomModelFormat );
    size_t modelInfoSizeBytes = ( sizeof( PomModelInfo ) * numModelInfos );
    size_t submodelIdArrayBlockSizeBytes = ( sizeof( uint32_t ) * numSubmodelIds );
    size_t modelDataBlockSizeBytes = modelInfoSizeBytes + submodelIdArrayBlockSizeBytes;
    
    // Create data block
    printf( "Create data block\n" );
    size_t meshInfoSize = sizeof( PomModelMeshInfo ) * scene->mNumMeshes;
    size_t texInfoSize = sizeof( PomModelTextureInfo ) * texCount;

    size_t dataBlockSize = meshInfoSize + meshBlockSize +
                           texInfoSize + texSize +
                           materialBlockSize + modelDataBlockSizeBytes;
    size_t totalModelFileSize = formatHeaderSizeBytes + dataBlockSize;
                              
    uint8_t *totalModelDataBlock = (uint8_t*) calloc( totalModelFileSize, sizeof( uint8_t ) );
    // Accumulator makes it easier to later add blocks between existing ones
    uint8_t *dataBlockAccum = totalModelDataBlock;

    PomModelFormat *format = (PomModelFormat*) dataBlockAccum;
    dataBlockAccum += formatHeaderSizeBytes;

    uint8_t *dataBlockStart = dataBlockAccum;
    PomModelMeshInfo *meshInfos = (PomModelMeshInfo*) dataBlockAccum;
    dataBlockAccum += meshInfoSize;

    uint8_t *materialDataBlock = dataBlockAccum;
    dataBlockAccum += materialBlockSize;
    
    PomModelTextureInfo *texInfos = (PomModelTextureInfo*) dataBlockAccum;
    dataBlockAccum += texInfoSize;

    PomModelInfo *modelInfoDataBlock = (PomModelInfo*) dataBlockAccum;
    dataBlockAccum += modelInfoSizeBytes;

    uint32_t *submodelIdsArrayBlock = (uint32_t*) dataBlockAccum;
    dataBlockAccum += submodelIdArrayBlockSizeBytes;

    uint8_t *meshDataBlock = (uint8_t*)dataBlockAccum;
    dataBlockAccum += meshBlockSize;

    uint8_t *textureBlock = dataBlockAccum;
    dataBlockAccum += texSize;

    *format = (PomModelFormat){
        .magicNumber = POM_FORMAT_MAGIC_NUM,
        .sceneNameOffset = NULL,
        .dataBlockSize = dataBlockSize,
        .numTextureInfo = texCount,
        .textureInfoOffset = texInfos,
        .numMeshInfo = scene->mNumMeshes,
        .meshInfoOffset = meshInfos,
        .numMaterialInfo = scene->mNumMaterials,
        .materialInfoOffset = (PomModelMaterialInfo*) materialDataBlock,
        .numSubmodelInfo = 0,
        .submodelInfo = NULL,
        .numModelInfo = numModelInfos,
        .modelInfo = modelInfoDataBlock
    };

    // Populate mesh info
    printf( "Populate mesh info\n" );
    size_t currOffsetBytes = 0;
    for( uint32_t i = 0; i < scene->mNumMeshes; i++ ){
        PomModelMeshInfo *meshInfo = &meshInfos[ i ];
        const struct aiMesh *mesh = scene->mMeshes[ i ];
        size_t bytesWritten;
        if( populateMeshData( mesh, meshInfo, &meshDataBlock[ currOffsetBytes ], &bytesWritten ) ){
            err = 1;
            goto populateDataFailure;
        }
        meshInfo->dataBlockOffset = &meshDataBlock[ currOffsetBytes ];
        meshInfo->meshId = i;
        currOffsetBytes += bytesWritten;
    }
    // Check that we wrote the estimated amount of data
    if( currOffsetBytes != meshBlockSize ){
        printf( "Inconsistency between estimated mesh block size and bytes written. "
                "Wrote %lu, expected %lu\n", currOffsetBytes, meshBlockSize );
        err = 1;
        goto populateDataFailure;
    }

    printf( "Populate texture info\n" );
    size_t texBytesWritten;
    if( populateTextureData( scene, textureBlock, texInfos, &texBytesWritten ) ){
        printf( "Failed to populate texture block\n" );
        err = 1;
        goto populateDataFailure;
    }
    if( texBytesWritten != texSize ){
        printf( "Inconsistency in texture bytes written and expected texture block size\n" );
        err = 1;
        goto populateDataFailure;
    }

    printf( "Write material info block data\n" );
    size_t materialDataWritten;
    if( populateMaterialInfo( scene, materialDataBlock, &materialDataWritten ) ){
        printf( "Failed to populate material data\n" );
        err = 1;
        goto populateDataFailure;
    }
    if( texBytesWritten != texSize ){
        printf( "Inconsistency in material bytes written and expected material block size\n" );
        err = 1;
        goto populateDataFailure;
    }

    printf( "Write model info block data\n" );
    size_t modelInfoDataWritten;    
    if( populateModelInfo( scene, modelInfoDataBlock, 
                           submodelIdsArrayBlock, &modelInfoDataWritten ) ){
        printf( "Failed to populate model data\n" );
        err = 1;
        goto populateDataFailure;
    }
    if( modelInfoDataWritten != modelDataBlockSizeBytes ){
        printf( "Inconsistency in model info bytes written and expected block size\n" );
        err = 1;
        goto populateDataFailure;
    }

    printf( "Write output file\n" );
    if( writeBakedModel( format, dataBlockStart, dataBlockSize, bakedModelPath ) ){
        printf( "Failed to write output file\n" );
        goto populateDataFailure;
    }

#ifdef SANITY_CHECK_MODEL
    // Quick test on loading models
    PomModelFormat *loadedModel;
    uint8_t *loadedDataBlock;
    if( loadBakedModel( bakedModelPath, &loadedModel, &loadedDataBlock ) ){
        printf( "Failed to reload model" );
        err = 1;
        goto populateDataFailure;
    }
    // Relativise loaded model
    relativisePointers( loadedModel, loadedDataBlock );
    if( memcmp( loadedModel, format, totalModelFileSize ) != 0 ){
        printf( "File comparison failed" );
        free( loadedModel );
        err = 1;
        goto populateDataFailure;
    }
    free( loadedModel );
#endif // SANITY_CHECK_MODEL

populateDataFailure:
    free( totalModelDataBlock );
getSizeError:
    return err;
}

int loadRawModel( const char *modelPath, struct aiScene const **_scene ){
    
    unsigned int aiFlags = aiProcess_CalcTangentSpace |
                           aiProcess_Debone | // no bones for now
                           aiProcess_GenNormals |
                           aiProcess_OptimizeGraph |
                           aiProcess_OptimizeMeshes |
                           aiProcess_Triangulate;
    
    *_scene = aiImportFile( modelPath, aiFlags );
	if ( !*_scene ) {
        printf( "ERR: Failed to open model file \"%s\"\n", modelPath );
		return 1;
	}

    return 0;
}


/*
    unsigned int mPrimitiveTypes;
    unsigned int mNumVertices;
    unsigned int mNumFaces;
    C_STRUCT aiVector3D* mVertices;
    C_STRUCT aiVector3D* mNormals;
    C_STRUCT aiVector3D* mTangents;
    C_STRUCT aiVector3D* mBitangents;
    C_STRUCT aiColor4D* mColors[AI_MAX_NUMBER_OF_COLOR_SETS];
    C_STRUCT aiVector3D* mTextureCoords[AI_MAX_NUMBER_OF_TEXTURECOORDS];
    unsigned int mNumUVComponents[AI_MAX_NUMBER_OF_TEXTURECOORDS];
    C_STRUCT aiFace* mFaces;
    unsigned int mNumBones;
    C_STRUCT aiBone** mBones;
    unsigned int mMaterialIndex;
    C_STRUCT aiString mName;
    unsigned int mNumAnimMeshes;
    C_STRUCT aiAnimMesh** mAnimMeshes;
    unsigned int mMethod;
*/  

int populateMeshData( const struct aiMesh *mesh, PomModelMeshInfo *meshInfo,
                      uint8_t *dataBlock, size_t *bytesWritten ){
    // Assume triangulated faces
    uint32_t numFaces = mesh->mNumFaces;
    uint32_t numIndices = numFaces * 3;
    uint32_t *indexArray = (uint32_t*) dataBlock;
    uint32_t curIndexPos = 0;

    // Copy indices to data block
    for( uint32_t i = 0; i < numFaces; i++ ){
        struct aiFace * face = &mesh->mFaces[ i ];
        if( face->mNumIndices != 3 ){
            printf( "ERR: mesh face does not have 3 indices\n" );
            return 1;
        }
        uint32_t *faceIndices = face->mIndices;
        memcpy( &indexArray[ curIndexPos ], faceIndices, 3 * sizeof( uint32_t ) );
        curIndexPos += 3;
    }
    if( numIndices != curIndexPos ){
        printf( "Inconsistency between estimated number of indices and number writted\n" );
        return 1;
    }
    

    size_t currWrittenBytes = curIndexPos * sizeof( uint32_t );

    float *vertexArray = (float*) &indexArray[ curIndexPos ];
    uint32_t vertexCount = mesh->mNumVertices;
    size_t currVertexOffset = 0;
    bool hasTangentSpace = ( mesh->mBitangents ) && ( mesh->mTangents );
    for( uint32_t i = 0; i < vertexCount; i++ ){
        struct aiVector3D * posData = &mesh->mVertices[ i ];
        struct aiVector3D * normalData = &mesh->mNormals[ i ];
        
        memcpy( &vertexArray[ currVertexOffset ], posData, 3 * sizeof( float ) );
        currVertexOffset += 3;
        memcpy( &vertexArray[ currVertexOffset ], normalData, 3 * sizeof( float ) );
        currVertexOffset += 3;

        if( hasTangentSpace ){
            struct aiVector3D * tangentData = &mesh->mTangents[ i ];
            struct aiVector3D * bitangentData = &mesh->mBitangents[ i ];
            memcpy( &vertexArray[ currVertexOffset ], tangentData, 3 * sizeof( float ) );
            currVertexOffset += 3;
            memcpy( &vertexArray[ currVertexOffset ], bitangentData, 3 * sizeof( float ) );
            currVertexOffset += 3;
        }

        // Copy over all UV coord data
        for( uint32_t uvIdx = 0; uvIdx < AI_MAX_NUMBER_OF_TEXTURECOORDS; uvIdx++ ){
            uint32_t numUvComponents = mesh->mNumUVComponents[ uvIdx ];
            if( numUvComponents == 0 ){
                continue;
            }
            struct aiVector3D * uvCoordData = &mesh->mTextureCoords[ uvIdx ][ i ];
            memcpy( &vertexArray[ currVertexOffset ], uvCoordData, numUvComponents * sizeof( float ) );
            currVertexOffset += numUvComponents;
        }
    }
    uint32_t numUvCoords = 0;
    for( uint32_t uvIdx = 0; uvIdx < AI_MAX_NUMBER_OF_TEXTURECOORDS; uvIdx++ ){
        uint32_t numUvComponents = mesh->mNumUVComponents[ uvIdx ];
        if( numUvComponents > 0 ){
            numUvCoords++;
        }
    }
    size_t floatBlockSize = currVertexOffset * sizeof( float );
    currWrittenBytes += floatBlockSize;
    *bytesWritten = currWrittenBytes;
    size_t blockSizeDiff = ( (uint8_t*) &vertexArray[ currVertexOffset ] ) - dataBlock;
    if( blockSizeDiff != currWrittenBytes ){
        printf( "Inconsistency between reported written bytes and actual written bytes\n" );
        return 1;
    }

    meshInfo->dataStride = floatBlockSize / vertexCount;
    meshInfo->numUvCoords = numUvCoords;
    meshInfo->numVertices = vertexCount;
    meshInfo->numIndices = numIndices;
    meshInfo->dataSize = currWrittenBytes;
    meshInfo->hasTangentSpace = hasTangentSpace;
    meshInfo->nameOffset = NULL;

    return 0;
}

int getAllMeshSize( const struct aiScene *_scene, size_t *_sizeBytes ){
    uint32_t numMesh = _scene->mNumMeshes;
    size_t bytesAccum = 0; 

    const uint8_t vectorLen = 3; // 3-element vector

    for( uint32_t i = 0; i < numMesh; i++ ){
        const struct aiMesh *mesh = _scene->mMeshes[ i ];
        // Should always have position + normal vectors.
        // Tangent space is not guaranteed.
        bool hasTangentSpace = ( mesh->mTangents ) && ( mesh->mBitangents );
        const uint8_t numVectorTypes = hasTangentSpace ? 4 : 2;
        
        uint32_t meshIndexCount;
        uint32_t meshFloatElementCount;
        // Assume triangulated faces
        meshIndexCount = mesh->mNumFaces * 3;
        uint32_t numVertices = mesh->mNumVertices;
        uint32_t numUvComponents = 0, numUvCoords = 0;
        // Count how many components overall we have for texture coords
        for( uint32_t uvIdx = 0; uvIdx < AI_MAX_NUMBER_OF_TEXTURECOORDS; uvIdx++ ){
            numUvComponents += mesh->mNumUVComponents[ uvIdx ];
            if( mesh->mNumUVComponents[ uvIdx ] ) numUvCoords++;
        }
        meshFloatElementCount = numVertices * numVectorTypes * vectorLen;
        meshFloatElementCount += numVertices * numUvComponents;
        size_t floatBlockByteSize = meshFloatElementCount * sizeof( float );
        size_t meshByteSize = ( meshIndexCount * sizeof( uint32_t ) ) + floatBlockByteSize;

        bytesAccum += meshByteSize;
    }

    *_sizeBytes = bytesAccum;

    return 0;
}

// Get the memory footprint of the all the materials in the outgoing file.
// Includes space for all material info structs as well as their texture ID
// (uint32_t) arrays.
int getMaterialSize( const struct aiScene *_scene, size_t *_materialSize ){
    struct aiMaterial ** const materials = _scene->mMaterials;

    uint32_t numTexIds = 0;
    for( uint32_t i = 0; i < _scene->mNumMaterials; i++ ){
        const struct aiMaterial *material = materials[ i ];
        for( uint32_t texType = 1; texType <= AI_TEXTURE_TYPE_MAX; texType++ ){
            uint32_t texCount = aiGetMaterialTextureCount( material, texType );
            numTexIds += texCount;
        }
    }
    size_t texIdArraysSize = sizeof( uint32_t ) * numTexIds;
    size_t matInfoSize = sizeof( PomModelMaterialInfo ) * _scene->mNumMaterials;
    
    *_materialSize = texIdArraysSize + matInfoSize;
    return 0;
}

int populateMaterialInfo( const struct aiScene *_scene, uint8_t *_matDataBlock,
                          size_t *_bytesWritten ){
    struct aiMaterial ** const materials = _scene->mMaterials;

    size_t materialInfosSize = sizeof( PomModelMaterialInfo ) * _scene->mNumMaterials;
    
    PomModelMaterialInfo *materialInfos = (PomModelMaterialInfo*) _matDataBlock;
    uint32_t *texIdArrays = (uint32_t*) ( _matDataBlock + materialInfosSize );

    // TODO - make a proper lookup to the loaded texture data to link properly
    uint32_t texIdArrayIdx = 0;
    for( uint32_t i = 0; i < _scene->mNumMaterials; i++ ){
        const struct aiMaterial *material = materials[ i ];
        PomModelMaterialInfo *materialInfo = &materialInfos[ i ];
        materialInfo->textureIdsOffset = &texIdArrays[ texIdArrayIdx ];

        uint32_t numTexIds = 0;
        for( uint32_t texType = 1; texType <= AI_TEXTURE_TYPE_MAX; texType++ ){
            uint32_t texCount = aiGetMaterialTextureCount( material, texType );
            numTexIds += texCount;
            for( uint32_t tIds = 0; tIds < texCount; tIds++ ){
                texIdArrays[ texIdArrayIdx ] = texIdArrayIdx;
                texIdArrayIdx++;
            }
        }
        materialInfo->numTextures = numTexIds;
    }
    size_t texIdsWritten = sizeof( uint32_t ) * texIdArrayIdx;
    size_t materialsWritten = materialInfosSize;
    *_bytesWritten = texIdsWritten + materialsWritten;
    return 0;                              
}

int getAllTextureSize( const struct aiScene *_scene, uint32_t *_textureCount,
                       size_t *_allTextureDataSize ){
    uint32_t texCount = 0;
    size_t texSize = 0;
    PomMapCtx texPathsMap;
    if( pomMapInit( &texPathsMap, 0 ) ){
        printf( "Failed to create texture path hashmap\n" );
        return 1;
    }
    for( uint32_t i = 0; i < _scene->mNumMaterials; i++ ){
        const struct aiMaterial *material = _scene->mMaterials[ i ];
        for( uint32_t texType = 1; texType <= AI_TEXTURE_TYPE_MAX; texType++ ){
            uint32_t matTexCount = aiGetMaterialTextureCount( material, texType );
            texCount += matTexCount;
            for( uint32_t texIdx = 0; texIdx < matTexCount; texIdx++ ){
                struct aiString texPath;
                enum aiTextureMapping texMapping;
                unsigned int texUvIndex;
                ai_real texBlend;
                enum aiTextureOp texOp;
                enum aiTextureMapMode texMapMope;
                unsigned int texFlags;
                aiGetMaterialTexture( material, texType, texIdx,
                                      &texPath, &texMapping, &texUvIndex,
                                      &texBlend, &texOp, &texMapMope, &texFlags );
                // Check if we've already registered this texture
                const char *pathExists = pomMapGet( &texPathsMap, texPath.data, NULL );
                if( pathExists ){
                    // Path has already been loaded
                    continue;
                }
                // Path has not yet been loaded
                pomMapSet( &texPathsMap, texPath.data, "1" );
                // Get dir-relative path. Fair to assume that adding the dir wont cause the
                // aiString to go OOB
                size_t cPathLen = strlen( texPath.data );
                size_t dirLen = strlen( rawModelDir );
                char *newPathDest = &texPath.data[ dirLen ];
                memmove( newPathDest, &texPath.data[ 0 ], cPathLen + 1 );
                memcpy( &texPath.data, rawModelDir, dirLen );

                int x, y, c;
                if( !stbi_info( texPath.data, &x, &y, &c ) ){
                    printf( "Failed to get texture information on file %s: %s\n",
                            texPath.data, stbi_failure_reason() );
                    return 1;
                }
                // Assume 8 bits per channel
                texSize += ( x * y * c * sizeof( uint8_t ) );
            }
        }
    }
    *_textureCount = texCount;
    *_allTextureDataSize = texSize;
    pomMapClear( &texPathsMap );
    return 0;
}

int populateTextureData( const struct aiScene *_scene, uint8_t *_texDataBlock,
                         PomModelTextureInfo *_texInfos, size_t *_bytesWritten ){
    PomMapCtx texMapCtx;
    pomMapInit( &texMapCtx, 0 );
    if( pomMapInit( &texMapCtx, 0 ) ){
        printf( "Failed to create texture path hashmap\n" );
        return 1;
    }
    uint32_t currInfoIdx = 0;
    uint8_t *currFreeTexBlock = _texDataBlock;
    size_t bytesWritten = 0;
    for( uint32_t i = 0; i < _scene->mNumMaterials; i++ ){
        const struct aiMaterial *material = _scene->mMaterials[ i ];
        for( uint32_t texType = 1; texType <= AI_TEXTURE_TYPE_MAX; texType++ ){
            uint32_t matTexCount = aiGetMaterialTextureCount( material, texType );
            for( uint32_t texIdx = 0; texIdx < matTexCount; texIdx++ ){
                PomModelTextureInfo *currInfo = &_texInfos[ currInfoIdx++ ];
                struct aiString texPath;
                enum aiTextureMapping texMapping;
                unsigned int texUvIndex;
                ai_real texBlend;
                enum aiTextureOp texOp;
                enum aiTextureMapMode texMapMope;
                unsigned int texFlags;
                aiGetMaterialTexture( material, texType, texIdx,
                                      &texPath, &texMapping, &texUvIndex,
                                      &texBlend, &texOp, &texMapMope, &texFlags );
                // Check if we've already registered this texture
                const char *pathExists = pomMapGet( &texMapCtx, texPath.data, NULL );
                if( pathExists ){
                    // Path has already been loaded
                    uint32_t texOffsetBytes = (uint32_t) atoi( pathExists ); // Gross - TODO - change this 
                    uint8_t* texDataLoc = _texDataBlock + texOffsetBytes;
                    currInfo->dataOffset = texDataLoc;
                    continue;
                }

                // Path has not yet been loaded
                // Get dir-relative path. Fair to assume that adding the dir wont cause the
                // aiString to go OOB
                size_t cPathLen = strlen( texPath.data );
                size_t dirLen = strlen( rawModelDir );
                char *newPathDest = &texPath.data[ dirLen ];
                memmove( newPathDest, &texPath.data[ 0 ], cPathLen + 1 );
                memcpy( &texPath.data, rawModelDir, dirLen );

                int x, y, c;
                stbi_uc *imgData = stbi_load( texPath.data, &x, &y, &c, 0 );
                if( !imgData ){
                    printf( "Failed to get texture information on file %s: %s\n", texPath.data, stbi_failure_reason() );
                    stbi_image_free( imgData );
                    return 1;
                }
                // Copy image data to the block, and update the path cache
                // Assume 8 bits per channel.
                // At some point it'd be nice to be able to get stb image to write directly
                // to our buffer since thats where it's going anyway, and we could avoid
                // the memcpy.
                size_t texSize = ( x * y * c * sizeof( uint8_t ) );
                memcpy( currFreeTexBlock, imgData, texSize );
                char buff[ 50 ];
                uintptr_t ptrVal = (uintptr_t) currFreeTexBlock;
                sprintf( buff, "%lu", ptrVal );    // Also gross - TODO - change this
                pomMapSet( &texMapCtx, texPath.data, &buff[ 0 ] );
                currInfo->dataOffset = currFreeTexBlock;
                currFreeTexBlock += texSize;
                bytesWritten += texSize;
                stbi_image_free( imgData );
            }
        }
    }
    *_bytesWritten = bytesWritten;
    return 0;
}

// Recursive function for counting models + submodel IDs
int _rec_getModelInfoSize( const struct aiNode *_node, uint32_t *_numChildren, uint32_t *_numSubmodelIds ){
    for( uint32_t i = 0; i < _node->mNumChildren; i++ ){
        const struct aiNode *cNode = _node->mChildren[ i ];
        if( _rec_getModelInfoSize( cNode, _numChildren, _numSubmodelIds ) ){
            printf( "Failed to get node size\n" );
            return 1;
        }
    }
    if( _node->mNumMeshes ){
        *_numChildren += 1;
        *_numSubmodelIds += _node->mNumMeshes;
    }
    return 0;
}

int getModelInfoSize( const struct aiScene *_scene, uint32_t *_numInfos, uint32_t *_numIds ){
    // For now we'll treat all nodes with meshes as its own model.
    // TODO - add proper submodel->model support
    *_numInfos = 0;
    *_numIds = 0;
    if( _rec_getModelInfoSize( _scene->mRootNode, _numInfos, _numIds ) ){
        *_numInfos = *_numIds = 0;
        return 1;
    }
    return 0;
}

int _rec_populateModelInfo( const struct aiNode *_node, PomModelInfo **_modelInfo, 
                            uint32_t **_submodelIdArray, size_t *_bytesWritten ){
    if( _node->mNumMeshes ){
        // Populate an info block ourselves
        PomModelInfo *ourModelInfo = *_modelInfo;
        // TODO - proper submodel count (ie use our submodels instead of assimp meshes)
        ourModelInfo->numSubmodels = _node->mNumMeshes;
        ourModelInfo->submodelIdsOffset = *_submodelIdArray;
        for( uint32_t i = 0; i < _node->mNumMeshes; i++ ){
            // TODO - proper submodel lookup here
            **_submodelIdArray = i;
        }
        // Move the submodel ID array to the next free location
        *_submodelIdArray += _node->mNumMeshes;
        
        // TODO - proper modelID generation here
        ourModelInfo->modelId = 0;
        // TODO - get model name and copy it to the model info data block
        ourModelInfo->nameOffset = NULL;
        // TODO - proper transformation matrix setting
        ourModelInfo->defaultMatrixOffset = 0;

        // Move the info pointer to the next free spot
        *_modelInfo += 1;

        // Update the bytes written accumulator with what we've written
        size_t arrayWrittenSize = sizeof( uint32_t ) * _node->mNumMeshes;
        *_bytesWritten += sizeof( PomModelInfo ) + arrayWrittenSize;
    }

    // Now call recursively for all our children
    for( uint32_t i = 0; i < _node->mNumChildren; i++ ){
        const struct aiNode *cNode = _node->mChildren[ i ];
        if( _rec_populateModelInfo( cNode, _modelInfo, _submodelIdArray, _bytesWritten ) ){
            printf( "Failed to get node size\n" );
            return 1;
        }
    }
    
    return 0;

}

int populateModelInfo( const struct aiScene *_scene, PomModelInfo *_modelInfoBlock,
                       uint32_t *_submodelIdArray, size_t *_bytesWritten ){
    *_bytesWritten = 0;
    return _rec_populateModelInfo( _scene->mRootNode, &_modelInfoBlock, 
                                   &_submodelIdArray, _bytesWritten );
}
