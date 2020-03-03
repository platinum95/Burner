#include <stdlib.h>
#include <stdio.h>
#include <assimp/scene.h>
#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include "pomModelFormat.h"

typedef struct SubModelCtx SubModelCtx;
struct SubModelCtx{
    PomSubmodelInfo *submodelInfo;
    uint32_t numSubmodels;
    uint32_t currIdx;

    struct aiNode const **submodelData;
} submodelCtx = { 0 };

typedef struct MaterialCtx MaterialCtx;
struct MaterialCtx{
    PomModelMaterialInfo *materialInfoBlock;
    uint32_t numMaterial;
} materialCtx = { 0 };

typedef struct TextureCtx TextureCtx;
struct TextureCtx{  
    struct aiString texPath;
    enum aiTextureMapping texMapping;
    unsigned int texUvIndex;
    ai_real texBlend;
    enum aiTextureOp texOp;
    enum aiTextureMapMode texMapMope;
    unsigned int texFlags;
};

struct TextureCtxData{
    PomModelTextureInfo *textureInfoBlock;
    TextureCtx *ctx;
    uint32_t numTextures;
    uint32_t currIdx;
} textureCtxData = { 0 };

// Static allocation for model data that will be written
PomModelFormat modelOutput = { 0 };
//const struct aiScene* scene = NULL;

int writeBakedModel( const char *outputPath );
int loadRawModel( const char *modelPath, struct aiScene const **_scene );
int genSubmodel( const struct aiNode *_node, struct aiMesh **meshArray ); // Recursive
int genMaterial( const struct aiMaterial *_material, uint32_t _matId );
int genTextureCount( const struct aiScene *_scene );
int genSubmodelCount( const struct aiScene *_scene );
int getAllMeshSize( const struct aiScene *_scene, PomModelMeshInfo *meshInfos, size_t *_sizeBytes );
int populateMeshData( const struct aiMesh *mesh, PomModelMeshInfo *meshInfo,
                      uint8_t *dataBlock, size_t *bytesWritten );

uint8_t *meshDataBlock;

int main( int argc, char ** argv ){
    if( argc != 2 ){
        printf( "modelbake requires a single path to a raw model file as argument,"
                " e.g. modelbake ./model.dae\n" );
        return 1;
    }
    int err = 0;
    const char *modelPath = argv[ 1 ];
    //printf( "%s\n", modelPath );
    const struct aiScene *scene;
    if( loadRawModel( modelPath, &scene ) ){
        return 1;
    }

    // Create mesh header structs
    PomModelMeshInfo *meshInfos = (PomModelMeshInfo*) malloc( sizeof( PomModelMeshInfo ) * scene->mNumMeshes );

    size_t meshBlockSize;
    if( getAllMeshSize( scene, meshInfos, &meshBlockSize ) ){
        printf( "ERR: Failed to get mesh block size\n" );
        return 1;
    }

    printf( "Mesh block size %lu bytes\n", meshBlockSize );

    meshDataBlock = (uint8_t*) malloc( meshBlockSize );

    

    // Populate mesh info
    size_t currOffsetBytes = 0;
    for( uint32_t i = 0; i < scene->mNumMeshes; i++ ){
        PomModelMeshInfo *meshInfo = &meshInfos[ i ];
        const struct aiMesh *mesh = scene->mMeshes[ i ];
        size_t bytesWritten;
        if( populateMeshData( mesh, meshInfo, &meshDataBlock[ currOffsetBytes ], &bytesWritten ) ){
            free( meshDataBlock );
            free( meshInfos );
            return 1;
        }
        meshInfo->dataBlockOffset = &meshDataBlock[ currOffsetBytes ];
        meshInfo->meshId = i;
        currOffsetBytes += bytesWritten;
    }
    if( currOffsetBytes != meshBlockSize ){
        printf( "Inconsistency between estimated mesh block size and bytes written. "
                "Wrote %lu, expected %lu\n", currOffsetBytes, meshBlockSize );
        free( meshDataBlock );
        free( meshInfos );
        return 1;
    }
    if( genTextureCount( scene ) ){
        printf( "ERR: Could not get texture count\n" );
        return 1;
    }
    printf( "Texture count: %i\n", textureCtxData.numTextures );

    uint32_t numMaterial = scene->mNumMaterials;
    materialCtx.numMaterial = numMaterial;
    materialCtx.materialInfoBlock = (PomModelMaterialInfo*) malloc( sizeof( PomModelMaterialInfo ) * numMaterial );

    for( uint32_t i = 0; i < scene->mNumMaterials; i++ ){
        if( genMaterial( scene->mMaterials[ i ], i ) ){
            printf( "ERR: Failed to load material %i\n", i );
        }
    }

    struct aiNode *rootNode = scene->mRootNode;
    if( genSubmodelCount( scene ) ){
        printf( "ERR: failed to generate submodel data block\n" );
        err = 1;
        goto submodelCountErr;
    }
    
    if( rootNode->mNumMeshes && rootNode->mNumChildren ){
        printf( "Root node is a mesh and has children. Not sure how to continue\n" );
        return 0;
    }
    else if( rootNode->mNumMeshes ){
        // Single node/mesh located at root
        printf( "Single node/mesh located at root node\n" );
    }
    else{
        // Multiple nodes/meshes. For now each node with meshes will be treated
        // as a submodel, with root node being the model
        
        
    }

    // Free up allocated material memory.
    // TODO - this should eventually be freed when the output file is being written
    for( uint32_t i = 0; i < scene->mNumMaterials; i++ ){
        // Discard const qualifer for the material string
        free( (char*) materialCtx.materialInfoBlock[ i ].nameOffset );
        free( materialCtx.materialInfoBlock[ i ].textureIdsOffset );
    }

submodelCountErr:
    free( meshDataBlock );
    free( meshInfos );
    free( materialCtx.materialInfoBlock );
    free( textureCtxData.ctx );
    free( textureCtxData.textureInfoBlock );
    return err;
}

int loadRawModel( const char *modelPath, struct aiScene const **_scene ){
    *_scene = aiImportFile( modelPath, aiProcessPreset_TargetRealtime_MaxQuality );
    
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

    for( uint32_t i = 0; i < vertexCount; i++ ){
        struct aiVector3D * posData = &mesh->mVertices[ i ];
        struct aiVector3D * normalData = &mesh->mNormals[ i ];
        struct aiVector3D * tangentData = &mesh->mTangents[ i ];
        struct aiVector3D * bitangentData = &mesh->mBitangents[ i ];
        
        memcpy( &vertexArray[ currVertexOffset ], posData, 3 * sizeof( float ) );
        currVertexOffset += 3;
        memcpy( &vertexArray[ currVertexOffset ], normalData, 3 * sizeof( float ) );
        currVertexOffset += 3;
        memcpy( &vertexArray[ currVertexOffset ], tangentData, 3 * sizeof( float ) );
        currVertexOffset += 3;
        memcpy( &vertexArray[ currVertexOffset ], bitangentData, 3 * sizeof( float ) );
        currVertexOffset += 3;

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

    if( meshInfo->dataSize != currWrittenBytes ){
        printf( "Bleep\n" );
    }
    meshInfo->dataStride = floatBlockSize / vertexCount;
    meshInfo->numUvCoords = numUvCoords;
    meshInfo->numVertices = vertexCount;
    meshInfo->numIndices = numIndices;
    
    return 0;
}

int getAllMeshSize( const struct aiScene *_scene, PomModelMeshInfo *meshInfos, size_t *_sizeBytes ){
    uint32_t numMesh = _scene->mNumMeshes;
    size_t bytesAccum = 0; 

    const uint8_t numVectorTypes = 4; // Position, normal, tangent, bitangent
    const uint8_t vectorLen = 3; // 3-element vector

    for( uint32_t i = 0; i < numMesh; i++ ){
        PomModelMeshInfo *meshInfo = &meshInfos[ i ];
        const struct aiMesh *mesh = _scene->mMeshes[ i ];
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
        meshInfo->dataSize = meshByteSize;
        meshInfo->dataStride = floatBlockByteSize / numVertices;
        meshInfo->numUvCoords = numUvCoords;
        meshInfo->numVertices = numVertices;
        meshInfo->numIndices = meshIndexCount;

        bytesAccum += meshByteSize;
    }

    *_sizeBytes = bytesAccum;

    return 0;
}

int genSubmodel( const struct aiNode *_node, struct aiMesh **meshArray ){

    if( _node->mNumMeshes ){
        uint32_t currSubmodelIdx = submodelCtx.currIdx;
        submodelCtx.currIdx++;
        PomSubmodelInfo *subModelInfo = &submodelCtx.submodelInfo[ currSubmodelIdx ];
        submodelCtx.submodelData[ currSubmodelIdx ] = _node;
        subModelInfo->materialId = currSubmodelIdx;

        // Load mesh data
        uint32_t numMeshes = _node->mNumMeshes;
        for( uint32_t i = 0; i < numMeshes; i++ ){
            uint32_t meshId = _node->mMeshes[ i ];
            struct aiMesh *mesh = meshArray[ meshId ];
            mesh->mNumVertices++;
            mesh->mNumVertices--;
        }


    }
    
    return 0;
}

int genMaterial( const struct aiMaterial *_material, uint32_t _matId ){
    PomModelMaterialInfo *materialInfo = &materialCtx.materialInfoBlock[ _matId ];
    materialInfo->materialId = _matId;

    
    struct aiString name;
    aiGetMaterialString( _material, AI_MATKEY_NAME, &name );
    char * matName = (char*) malloc( sizeof( char ) * ( name.length + 1 ) );
    strncpy( matName, name.data, name.length );
    matName[ name.length ] = '\0';
    materialInfo->nameOffset = matName;

    printf( "Loading material %i:%s\n", _matId, matName );
    
    enum aiShadingMode shadingMode;
    aiGetMaterialIntegerArray( _material, AI_MATKEY_SHADING_MODEL, (int*)&shadingMode, NULL );

    materialInfo->materialType = shadingMode;
    uint32_t texStartIdx = textureCtxData.currIdx;
    // Loop over each texture type, ignoring type=NONE
    for( uint32_t texType = 1; texType <= AI_TEXTURE_TYPE_MAX; texType++ ){
        uint32_t texCount = aiGetMaterialTextureCount( _material, texType );
        for( uint32_t texIt = 0; texIt < texCount; texIt++ ){        
            struct aiString texPath;
            enum aiTextureMapping texMapping;
            unsigned int texUvIndex;
            ai_real texBlend;
            enum aiTextureOp texOp;
            enum aiTextureMapMode texMapMope;
            unsigned int texFlags;
            aiGetMaterialTexture( _material, texType, texIt,
                                  &texPath, &texMapping, &texUvIndex,
                                  &texBlend, &texOp, &texMapMope, &texFlags );

            TextureCtx *textureCtx = &textureCtxData.ctx[ textureCtxData.currIdx++ ];
            textureCtx->texPath = texPath;
            textureCtx->texMapping = texMapping;
            textureCtx->texUvIndex = texUvIndex;
            textureCtx->texBlend = texBlend;
            textureCtx->texOp = texOp;
            textureCtx->texMapMope = texMapMope;
            textureCtx->texFlags = texFlags;
        }
    }

    // Create texture ID array - i.e. the textures this material uses
    uint32_t texEndIdx = textureCtxData.currIdx;
    uint32_t matTexCount = texEndIdx - texStartIdx;

    uint32_t *matTexIdxs = (uint32_t*) malloc( sizeof( uint32_t ) * matTexCount );
    for( uint32_t i = 0; i < matTexCount; i++ ){
        matTexIdxs[ i ] = texStartIdx + i;
    }
    materialInfo->numTextures = matTexCount;
    materialInfo->textureIdsOffset = matTexIdxs;

    return 0;
}

int genTextureCount( const struct aiScene *_scene ){
    uint32_t texCount = 0;
    for( uint32_t i = 0; i < _scene->mNumMaterials; i++ ){
        const struct aiMaterial *material = _scene->mMaterials[ i ];
        for( uint32_t texType = 1; texType <= AI_TEXTURE_TYPE_MAX; texType++ ){
            uint32_t matTexCount = aiGetMaterialTextureCount( material, texType );
            texCount += matTexCount;
        }
    }
    textureCtxData.ctx = (TextureCtx*) malloc( sizeof( TextureCtx ) * texCount );
    textureCtxData.textureInfoBlock = (PomModelTextureInfo*) malloc( sizeof( PomModelTextureInfo ) * texCount );
    textureCtxData.numTextures = texCount;
    textureCtxData.currIdx = 0;
    return 0;
}


uint32_t getChildNodeCount( const struct aiNode *_node ){
    uint32_t nodeChildCount = 0;
    for( uint32_t i = 0; i < _node->mNumChildren; i++ ){
        const struct aiNode *cNode = _node->mChildren[ i ];
        nodeChildCount += getChildNodeCount( cNode );
    }
    if( _node->mNumMeshes ){
        nodeChildCount++;
    }
    return nodeChildCount;
}

int genSubmodelCount( const struct aiScene *_scene ){
    //uint32_t submodelCount = getChildNodeCount( _scene->mRootNode );
    uint32_t submodelCount = _scene->mNumMeshes;
    submodelCtx.submodelInfo = (PomSubmodelInfo*) malloc( sizeof( PomSubmodelInfo ) * submodelCount );
    submodelCtx.submodelData = (struct aiNode const**) malloc( sizeof( struct aiNode* ) * submodelCount );
    submodelCtx.numSubmodels = submodelCount;
    return 0;
}