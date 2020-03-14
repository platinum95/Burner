#include "common.h"
#include "pomMaths.h"
#include <math.h>

Mat4x4 createProjectionMatrix( BaseType _fovRad, BaseType _near, BaseType _far,
                               BaseType _width, BaseType _height ){
    BaseType aspectRatio = _width / _height;
    BaseType range = tanf( _fovRad * 0.5f ) * _near;
    BaseType Sx = ( 2 * _near ) / ( range * aspectRatio + range * aspectRatio );
    BaseType Sy = -_near / range;
    BaseType Sz = -( _far + _near ) / ( _far - _near );
    BaseType Pz = -( 2 * _far * _near ) / ( _far - _near );

    return (Mat4x4){
        {
            Vec4Gen( Sx, 0.0f, 0.0f, 0.0f ),
            Vec4Gen( 0.0f, Sy, 0.0f, 0.0f ),
            Vec4Gen( 0.0f, 0.0f, Sz, -1.0f ),
            Vec4Gen( 0.0f, 0.0f, Pz, 0.0f )
        }
    };
}

// TODO - SIMD versions
// Direct calc code (i.e. no SIMD)
#ifndef POM_MATHS_SIMD

Vec3 vec3Cross( Vec3 _a, Vec3 _b ){
    BaseType *a = _a.vec3;
    BaseType *b = _b.vec3;
    Vec3 ret;
    ret.vec3[ 0 ] = ( a[ 1 ] * b[ 2 ] ) - ( a[ 2 ] * b[ 1 ] );
    ret.vec3[ 1 ] = ( a[ 2 ] * b[ 0 ] ) - ( a[ 0 ] * b[ 2 ] );
    ret.vec3[ 2 ] = ( a[ 0 ] * b[ 1 ] ) - ( a[ 1 ] * b[ 0 ] );
    return ret;
}

BaseType vec2Dot( Vec2 _a, Vec2 _b ){
    return ( _a.vec2[ 0 ] * _b.vec2[ 0 ] ) +
           ( _a.vec2[ 1 ] * _b.vec2[ 1 ] );
} // 2 Mults, 1 add, 4 loads, 1 store

BaseType vec3Dot( Vec3 _a, Vec3 _b ){
    return ( _a.vec3[ 0 ] * _b.vec3[ 0 ] ) +
           ( _a.vec3[ 1 ] * _b.vec3[ 1 ] ) +
           ( _a.vec3[ 2 ] * _b.vec3[ 2 ] );
} // 3 mults, 2 adds, 6 loads, 1 store

BaseType vec4Dot( Vec4 _a, Vec4 _b ){
    return ( _a.vec4[ 0 ] * _b.vec4[ 0 ] ) +
           ( _a.vec4[ 1 ] * _b.vec4[ 1 ] ) +
           ( _a.vec4[ 2 ] * _b.vec4[ 2 ] ) +
           ( _a.vec4[ 3 ] * _b.vec4[ 3 ] );
           
} // 4 FP Mults, 3 FP Adds, 8 FP loads, 1 FP store

Vec2 vec2Div( Vec2 _a, Vec2 _b ){
    return (Vec2){
        { 
          _a.vec2[ 0 ] / _b.vec2[ 0 ],
          _a.vec2[ 1 ] / _b.vec2[ 1 ]
        }
    };
} // 2 FP div, 4 FP loads, 2 FP stores

Vec3 vec3Div( Vec3 _a, Vec3 _b ){
    return (Vec3){
        { 
          _a.vec3[ 0 ] / _b.vec3[ 0 ],
          _a.vec3[ 1 ] / _b.vec3[ 1 ],
          _a.vec3[ 2 ] / _b.vec3[ 2 ]
        }
    };
} // 3 FP divs, 6 FP loads, 3 FP stores

Vec4 vec4Div( Vec4 _a, Vec4 _b ){
    return (Vec4){
        { 
          _a.vec4[ 0 ] / _b.vec4[ 0 ],
          _a.vec4[ 1 ] / _b.vec4[ 1 ],
          _a.vec4[ 2 ] / _b.vec4[ 2 ],
          _a.vec4[ 3 ] / _b.vec4[ 3 ]
        }
    };
} // 4 FP divs, 8 FP loads, 4 FP stores


Vec2 vec2ScalarMult( Vec2 _a, BaseType _s ){
    return (Vec2){
        {
            _a.vec2[ 0 ] * _s,
            _a.vec2[ 1 ] * _s,
        }
    };
} // 2 FP mults, 2 FP loads, 2 FP stores

Vec3 vec3ScalarMult( Vec3 _a, BaseType _s ){
    return (Vec3){
        {
            _a.vec3[ 0 ] * _s,
            _a.vec3[ 1 ] * _s,
            _a.vec3[ 2 ] * _s,
        }
    };
} // 3 FP mult, 3 FP loads, 3 FP stores

Vec4 vec4ScalarMult( Vec4 _a, BaseType _s ){
    return (Vec4){
        {
            _a.vec4[ 0 ] * _s,
            _a.vec4[ 1 ] * _s,
            _a.vec4[ 2 ] * _s,
            _a.vec4[ 3 ] * _s,
        }
    };
} // 4 FP mult, 4 FP loads, 4 FP stores


Vec2 vec2MatMult( Mat2x2 _m, Vec2 _a ){
    return (Vec2){
        {
            vec2Dot( _a, _m.mat2x2[ 0 ] ),
            vec2Dot( _a, _m.mat2x2[ 1 ] ),
        }
    };
} // 4 FP mult, 2 FP add, 8 FP load, 2 FP store

Vec3 vec3MatMult( Mat3x3 _m, Vec3 _a ){
    return (Vec3){
        {
            vec3Dot( _a, _m.mat3x3[ 0 ] ),
            vec3Dot( _a, _m.mat3x3[ 1 ] ),
            vec3Dot( _a, _m.mat3x3[ 2 ] ),
        }
    };
}

Vec4 vec4MatMult( Mat4x4 _m, Vec4 _a ){
    return (Vec4){
        {
            vec4Dot( _a, _m.mat4x4[ 0 ] ),
            vec4Dot( _a, _m.mat4x4[ 1 ] ),
            vec4Dot( _a, _m.mat4x4[ 1 ] ),
            vec4Dot( _a, _m.mat4x4[ 1 ] ),
        }
    };
}

Mat2x2 mat2Transpose( Mat2x2 _a ){
    return (Mat2x2){
        {
            (Vec2){
                { _a.mat2x2[ 0 ].vec2[ 0 ], _a.mat2x2[ 1 ].vec2[ 0 ] }
            },
            (Vec2){
                { _a.mat2x2[ 0 ].vec2[ 1 ], _a.mat2x2[ 1 ].vec2[ 1 ] }
            }
        }
    };
}

Mat3x3 mat3Transpose( Mat3x3 _a ){
    return (Mat3x3){
        {
            (Vec3){
                { _a.mat3x3[ 0 ].vec3[ 0 ], _a.mat3x3[ 1 ].vec3[ 0 ], _a.mat3x3[ 2 ].vec3[ 0 ] }
            },
            (Vec3){
                { _a.mat3x3[ 0 ].vec3[ 1 ], _a.mat3x3[ 1 ].vec3[ 1 ], _a.mat3x3[ 2 ].vec3[ 1 ] }
            },
            (Vec3){
                { _a.mat3x3[ 0 ].vec3[ 2 ], _a.mat3x3[ 1 ].vec3[ 2 ], _a.mat3x3[ 2 ].vec3[ 2 ] }
            }
        }
    };
}

Mat4x4 mat4Transpose( Mat4x4 _a ){
    return (Mat4x4){
        {
            (Vec4){
                { _a.mat4x4[ 0 ].vec4[ 0 ], _a.mat4x4[ 1 ].vec4[ 0 ], _a.mat4x4[ 2 ].vec4[ 0 ], _a.mat4x4[ 3 ].vec4[ 0 ] }
            },
            (Vec4){
                { _a.mat4x4[ 0 ].vec4[ 1 ], _a.mat4x4[ 1 ].vec4[ 1 ], _a.mat4x4[ 2 ].vec4[ 1 ], _a.mat4x4[ 3 ].vec4[ 1 ] }
            },
            (Vec4){
                { _a.mat4x4[ 0 ].vec4[ 2 ], _a.mat4x4[ 1 ].vec4[ 2 ], _a.mat4x4[ 2 ].vec4[ 2 ], _a.mat4x4[ 3 ].vec4[ 2 ] }
            },
            (Vec4){
                { _a.mat4x4[ 0 ].vec4[ 3 ], _a.mat4x4[ 1 ].vec4[ 3 ], _a.mat4x4[ 2 ].vec4[ 3 ], _a.mat4x4[ 3 ].vec4[ 3 ] }
            }
        }
    };
}


Mat2x2 mat2Mult( Mat2x2 _a, Mat2x2 _b ){
    return (Mat2x2){
        (Vec2){ // Column 1
            { 
                vec2Dot( _b.mat2x2[ 0 ], mat2x2RowVector( _a, 0 ) ),
                vec2Dot( _b.mat2x2[ 0 ], mat2x2RowVector( _a, 1 ) ),
            }
        },
        (Vec2){ // Column 2
            {
                vec2Dot( _b.mat2x2[ 1 ], mat2x2RowVector( _a, 0 ) ),
                vec2Dot( _b.mat2x2[ 1 ], mat2x2RowVector( _a, 1 ) ),
            }
        }
    };
}

Mat3x3 mat3Mult( Mat3x3 _a, Mat3x3 _b ){
    return (Mat3x3){
        (Vec3){ // Column 0
            { 
                vec3Dot( _b.mat3x3[ 0 ], mat3x3RowVector( _a, 0 ) ),
                vec3Dot( _b.mat3x3[ 0 ], mat3x3RowVector( _a, 1 ) ),
                vec3Dot( _b.mat3x3[ 0 ], mat3x3RowVector( _a, 2 ) ),
            }
        },
        (Vec3){ // Column 1
            { 
                vec3Dot( _b.mat3x3[ 1 ], mat3x3RowVector( _a, 0 ) ),
                vec3Dot( _b.mat3x3[ 1 ], mat3x3RowVector( _a, 1 ) ),
                vec3Dot( _b.mat3x3[ 1 ], mat3x3RowVector( _a, 2 ) ),
            }
        },
        (Vec3){ // Column 2
            { 
                vec3Dot( _b.mat3x3[ 2 ], mat3x3RowVector( _a, 0 ) ),
                vec3Dot( _b.mat3x3[ 2 ], mat3x3RowVector( _a, 1 ) ),
                vec3Dot( _b.mat3x3[ 2 ], mat3x3RowVector( _a, 2 ) ),
            }
        }
    };
}

Mat4x4 mat4Mult( Mat4x4 _a, Mat4x4 _b ){
    return (Mat4x4){
        (Vec4){ // Column 1
            { 
                vec4Dot( _b.mat4x4[ 0 ], mat4x4RowVector( _a, 0 ) ),
                vec4Dot( _b.mat4x4[ 0 ], mat4x4RowVector( _a, 1 ) ),
                vec4Dot( _b.mat4x4[ 0 ], mat4x4RowVector( _a, 2 ) ),
                vec4Dot( _b.mat4x4[ 0 ], mat4x4RowVector( _a, 3 ) ),
            }
        },
        (Vec4){ // Column 1
            { 
                vec4Dot( _b.mat4x4[ 1 ], mat4x4RowVector( _a, 0 ) ),
                vec4Dot( _b.mat4x4[ 1 ], mat4x4RowVector( _a, 1 ) ),
                vec4Dot( _b.mat4x4[ 1 ], mat4x4RowVector( _a, 2 ) ),
                vec4Dot( _b.mat4x4[ 1 ], mat4x4RowVector( _a, 3 ) ),
            }
        },
        (Vec4){ // Column 1
            { 
                vec4Dot( _b.mat4x4[ 2 ], mat4x4RowVector( _a, 0 ) ),
                vec4Dot( _b.mat4x4[ 2 ], mat4x4RowVector( _a, 1 ) ),
                vec4Dot( _b.mat4x4[ 2 ], mat4x4RowVector( _a, 2 ) ),
                vec4Dot( _b.mat4x4[ 2 ], mat4x4RowVector( _a, 3 ) ),
            }
        },
        (Vec4){ // Column 1
            { 
                vec4Dot( _b.mat4x4[ 3 ], mat4x4RowVector( _a, 0 ) ),
                vec4Dot( _b.mat4x4[ 3 ], mat4x4RowVector( _a, 1 ) ),
                vec4Dot( _b.mat4x4[ 3 ], mat4x4RowVector( _a, 2 ) ),
                vec4Dot( _b.mat4x4[ 3 ], mat4x4RowVector( _a, 3 ) ),
            }
        }
    };
}

Mat2x2 mat2ScalarMult( Mat2x2 _a, BaseType _s ){
    return (Mat2x2){{
        vec2ScalarMult( mat2x2ColumnVector( _a, 0 ), _s ),
        vec2ScalarMult( mat2x2ColumnVector( _a, 1 ), _s ),
    }};
}

Mat3x3 mat3ScalarMult( Mat3x3 _a, BaseType _s ){
    return (Mat3x3){{
        vec3ScalarMult( mat3x3ColumnVector( _a, 0 ), _s ),
        vec3ScalarMult( mat3x3ColumnVector( _a, 1 ), _s ),
        vec3ScalarMult( mat3x3ColumnVector( _a, 2 ), _s ),
    }};
}

Mat4x4 mat4ScalarMult( Mat4x4 _a, BaseType _s ){
    return (Mat4x4){{
        vec4ScalarMult( mat4x4ColumnVector( _a, 0 ), _s ),
        vec4ScalarMult( mat4x4ColumnVector( _a, 1 ), _s ),
        vec4ScalarMult( mat4x4ColumnVector( _a, 2 ), _s ),
        vec4ScalarMult( mat4x4ColumnVector( _a, 3 ), _s ),
    }};
}


Mat4x4 mat4Translate( Mat4x4 _matrix, Vec4 _translation ){
    Mat4x4 translationMatrix = mat4x4Identity();
    mat4x4ColumnVector( translationMatrix, 3 ) = _translation;
    return mat4Mult( translationMatrix, _matrix );
}

#else

#include <immintrin.h>

BaseType vec4Dot( Vec4 _a, Vec4 _b ){
    __m128 inpA = _mm_load_ps( &_a.vec4[ 0 ] );
    __m128 inpB = _mm_load_ps( &_b.vec4[ 0 ] );

    __m128 multRes = _mm_mul_ps( inpA, inpB );
    __m128 sumOut = _mm_hadd_ps( multRes, multRes );
    sumOut = _mm_hadd_ps( sumOut, sumOut );
    BaseType dotProd = _mm_cvtss_f32( sumOut );

    return dotProd;
} // 1 FP Mult, 2 FP adds, 2(8) FP Loads, 1 FP Store

#endif