#ifndef STACK_H
#define STACK_H
#include "common.h"

/*
Simple non-contiguous stack implementation.
Node memory is handled by module (except for popAll).
User data memory is handled by user (i.e. it is not copied)
Not thread safe
*/

//typedef struct PomStackNode PomStackNode;
typedef struct PomStackCtx PomStackCtx;

/*
struct PomStackNode{
    void *data;
    PomStackNode *next;
};
*/
struct PomStackCtx{
    PomCommonNode *head;
};

// Initialise the stack
int pomStackInit( PomStackCtx *_ctx );

// Clear the stack and free memory
int pomStackClear( PomStackCtx *_ctx );

// Pop all items off the stack (releasing memory ownership to caller)
PomCommonNode * pomStackPopAll( PomStackCtx *_ctx );

// Pop a single item off the stack
PomCommonNode * pomStackPop( PomStackCtx *_ctx );

// Push a single item onto the stack
int pomStackPush( PomStackCtx *_ctx, PomCommonNode *_data );

// Push many nodes onto the stack (must be null terminated)
int pomStackPushMany( PomStackCtx *_ctx, PomCommonNode * _nodes );

/*******************************************
* Thread-safe version - locked
********************************************/
#if defined(__STDC_NO_THREADS__) || defined(_WIN32) || defined(__CYGWIN__)
#include "tinycthread.h"
#else
#include <threads.h>
#endif

typedef struct PomStackTsCtx PomStackTsCtx;

struct PomStackTsCtx{
    PomCommonNode *head;
    int stackSize;
    mtx_t mtx;
};

// Initialise the stack
int pomStackTsInit( PomStackTsCtx *_ctx );

// Clear the stack and free memory
int pomStackTsClear( PomStackTsCtx *_ctx );

// Pop all items off the stack (releasing memory ownership to caller)
PomCommonNode * pomStackTsPopAll( PomStackTsCtx *_ctx );

// Pop a single item off the stack
PomCommonNode * pomStackTsPop( PomStackTsCtx *_ctx );

// Pop many items off the stack. Caller assumes responsibility for freeing the node memory
int pomStackTsPopMany( PomStackTsCtx *_ctx, PomCommonNode ** _nodes, int _numNodes );

// Cull the stack to a certain size. Caller assumes responsibility for freeing the node memory
int pomStackTsCull( PomStackTsCtx *_ctx, PomCommonNode ** _nodes, int _numNodes );

// Push a single item onto the stack
int pomStackTsPush( PomStackTsCtx *_ctx, PomCommonNode * _data );

// Push many nodes onto the stack (must be null terminated)
int pomStackTsPushMany( PomStackTsCtx *_ctx, PomCommonNode * _nodes );

#endif // STACK_H