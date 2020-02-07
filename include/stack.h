#ifndef STACK_H
#define STACK_H
/*
Simple non-contiguous stack implementation.
Node memory is handled by module (except for popAll).
User data memory is handled by user (i.e. it is not copied)
Not thread safe
*/

typedef struct PomStackNode PomStackNode;
typedef struct PomStackCtx PomStackCtx;

struct PomStackNode{
    void *data;
    PomStackNode *next;
};

struct PomStackCtx{
    PomStackNode *head;
};

// Initialise the stack
int pomStackInit( PomStackCtx *_ctx );

// Clear the stack and free memory
int pomStackClear( PomStackCtx *_ctx );

// Pop all items off the stack (releasing memory ownership to caller)
PomStackNode * pomStackPopAll( PomStackCtx *_ctx );

// Pop a single item off the stack
void * pomStackPop( PomStackCtx *_ctx );

// Push a single item onto the stack
int pomStackPush( PomStackCtx *_ctx, void * _data );

// Push many nodes onto the stack (must be null terminated)
int pomStackPushMany( PomStackCtx *_ctx, PomStackNode * _nodes );

/*******************************************
* Stack safe version
********************************************/
#include <threads.h>

typedef struct PomStackTsCtx PomStackTsCtx;

struct PomStackTsCtx{
    PomStackNode *head;
    int stackSize;
    mtx_t mtx;
};

// Initialise the stack
int pomStackTsInit( PomStackTsCtx *_ctx );

// Clear the stack and free memory
int pomStackTsClear( PomStackTsCtx *_ctx );

// Pop all items off the stack (releasing memory ownership to caller)
PomStackNode * pomStackTsPopAll( PomStackTsCtx *_ctx );

// Pop a single item off the stack
void * pomStackTsPop( PomStackTsCtx *_ctx );

// Pop many items off the stack. Caller assumes responsibility for freeing the node memory
int pomStackTsPopMany( PomStackTsCtx *_ctx, PomStackNode ** _nodes, int _numNodes );

// Cull the stack to a certain size. Caller assumes responsibility for freeing the node memory
int pomStackTsCull( PomStackTsCtx *_ctx, PomStackNode ** _nodes, int _numNodes );

// Push a single item onto the stack
int pomStackTsPush( PomStackTsCtx *_ctx, void * _data );

// Push many nodes onto the stack (must be null terminated)
int pomStackTsPushMany( PomStackTsCtx *_ctx, PomStackNode * _nodes );

#endif // STACK_H