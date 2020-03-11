#ifndef POM_MATHS_H
#define POM_MATHS_H
#include <stdalign.h>

#define POM_MATHS_FORCE_SISD

#if defined( __AVX2__ ) && !defined( POM_MATHS_FORCE_SISD )
#define POM_MATHS_SIMD
#endif

#ifdef POM_MATHS_SIMD
#define VECTOR_ALIGNMENT alignas(16)
#else
#define VECTOR_ALIGNMENT
#endif

// Base type definition
typedef float BaseType;
// Vector definitions
typedef struct Vec2 { VECTOR_ALIGNMENT BaseType vec2[ 2 ]; } Vec2;
typedef struct Vec3 { VECTOR_ALIGNMENT BaseType vec3[ 3 ]; } Vec3;
typedef struct Vec4 { VECTOR_ALIGNMENT BaseType vec4[ 4 ]; } Vec4;

// Matrix definitions
// For now these are arrays of above vectors, but SIMD
// alignment may not play nicely with the data since 
// for vectors smaller than alignment (2,3) padding
// will be inserted between elements.
// These will be treated as column major matrices
typedef struct Mat2x2 { Vec2 mat2x2[ 2 ]; } Mat2x2;
typedef struct Mat3x3 { Vec3 mat3x3[ 3 ]; } Mat3x3;
typedef struct Mat4x4 { Vec4 mat4x4[ 4 ]; } Mat4x4;

typedef struct VectorN VectorN;
struct VectorN{
    union{
        Vec2 vec2;
        Vec3 vec3;
        Vec4 vec4;
    };
};

// Common initialisers
#define vec2Zeros() (Vec2){ { 0.0f, 0.0f } }
#define vec3Zeros() (Vec3){ { 0.0f, 0.0f, 0.0f } }
#define vec4Zeros() (Vec4){ { 0.0f, 0.0f, 0.0f, 0.0f } }

#define mat2x2Zeros() (Mat2x2){ { vec2Zeros, vec2Zeros } }
#define mat3x3Zeros() (Mat3x3){ { vec3Zeros, vec3Zeros, vec3Zeros } }
#define mat4x4Zeros() (Mat4x4){ { vec4Zeros, vec4Zeros, vec4Zeros, vec4Zeros } }

// TODO - better pi
#define POM_PI 3.1415

#define radToDeg( rad ) ((BaseType)rad/(BaseType)POM_PI)*(BaseType)180.0
#define degToRad( deg ) ((BaseType)deg/(BaseType)180.0)*(BaseType)POM_PI

#define mat2x2Identity() (Mat2x2){{   \
    (Vec2){ { 1.0f, 0.0f } },       \
    (Vec2){ { 0.0f, 1.0f } },       \
}}

#define mat3x3Identity() (Mat3x3){{   \
    (Vec3){ { 1.0f, 0.0f, 0.0f } }, \
    (Vec3){ { 0.0f, 1.0f, 0,0f } }, \
    (Vec3){ { 0.0f, 0.0f, 1,0f } }, \
}}

#define mat4x4Identity() (Mat4x4){{   \
    (Vec4){ { 1.0f, 0.0f, 0.0f, 0.0f } },       \
    (Vec4){ { 0.0f, 1.0f, 0.0f, 0.0f } },       \
    (Vec4){ { 0.0f, 0.0f, 1.0f, 0.0f } },       \
    (Vec4){ { 0.0f, 0.0f, 0.0f, 1.0f } },       \
}}

// Macros for matrix element access
#define mat2x2RowVector( matrix, row ) (Vec2){ { _a.mat2x2[ 0 ].vec2[ row ], _a.mat2x2[ 1 ].vec2[ row ] } }
#define mat2x2ColumnVector( matrix, column ) _a.mat2x2[ column ]
#define mat2x2Element( _matrix, column, row ) _a.mat2x2[ column ].vec2[ row ]

#define mat3x3RowVector( matrix, row ) (Vec3){ { _a.mat3x3[ 0 ].vec3[ row ], _a.mat3x3[ 1 ].vec3[ row ], _a.mat3x3[ 2 ].vec3[ row ] } }
#define mat3x3ColumnVector( matrix, column ) _a.mat3x3[ column ]
#define mat3x3Element( _matrix, column, row ) _a.mat3x3[ column ].vec3[ row ]

#define mat4x4RowVector( matrix, row ) (Vec4){ { _a.mat4x4[ 0 ].vec4[ row ], _a.mat4x4[ 1 ].vec4[ row ], _a.mat4x4[ 2 ].vec4[ row ], _a.mat4x4[ 3 ].vec4[ row ] } }
#define mat4x4ColumnVector( matrix, column ) _a.mat4x4[ column ]
#define mat4x4Element( _matrix, column, row ) _a.mat4x4[ column ].vec4[ row ]

#define Vec4Gen( e0, e1, e2, e3 ) (Vec4){ { e0, e1, e2, e3 } }

// Funtion defs
//Vec2 vec2Cross( Vec2 _a, Vec2 _b );
Vec3 vec3Cross( Vec3 _a, Vec3 _b );

BaseType vec2Dot( Vec2 _a, Vec2 _b );
BaseType vec3Dot( Vec3 _a, Vec3 _b );
BaseType vec4Dot( Vec4 _a, Vec4 _b );

Vec2 vec2Div( Vec2 _a, Vec2 _b );
Vec3 vec3Div( Vec3 _a, Vec3 _b );
Vec4 vec4Div( Vec4 _a, Vec4 _b );

Vec2 vec2ScalarMult( Vec2 _a, BaseType _s );
Vec3 vec3ScalarMult( Vec3 _a, BaseType _s );
Vec4 vec4ScalarMult( Vec4 _a, BaseType _s );

Vec2 vec2MatMult( Mat2x2 _m, Vec2 _a );
Vec3 vec3MatMult( Mat3x3 _m, Vec3 _a );
Vec4 vec4MatMult( Mat4x4 _m, Vec4 _a );

Mat2x2 mat2Transpose( Mat2x2 _a );
Mat3x3 mat3Transpose( Mat3x3 _a );
Mat4x4 mat4Transpose( Mat4x4 _a );

Mat2x2 mat2Mult( Mat2x2 _a, Mat2x2 _b );
Mat3x3 mat3Mult( Mat3x3 _a, Mat3x3 _b );
Mat4x4 mat4Mult( Mat4x4 _a, Mat4x4 _b );

Mat2x2 mat2ScalarMult( Mat2x2 _a, BaseType _s );
Mat3x3 mat3ScalarMult( Mat3x3 _a, BaseType _s );
Mat4x4 mat4ScalarMult( Mat4x4 _a, BaseType _s );

Mat4x4 createProjectionMatrix( BaseType _fovRad, BaseType _near, BaseType _far,
                               BaseType _width, BaseType _height );

#endif // POM_MATHS_H