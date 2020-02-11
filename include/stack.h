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
* Thread-safe version - locked
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


/*******************************************
* Thread-safe version - lockless
********************************************/
#include <threads.h>
#include <stdatomic.h>

typedef struct PomStackLfCtx PomStackLfCtx;
typedef struct PomStackLfNode PomStackLfNode;

struct PomStackLfNode{
    void *data;
    PomStackLfNode * _Atomic next;
    _Atomic int inUse;
};

struct PomStackLfCtx{
    PomStackLfNode * _Atomic head;
    PomStackLfNode * _Atomic retiredList;
    PomStackLfNode * stackDummy;
    PomStackLfNode * retireDummy;
    _Atomic int stackSize;
    _Atomic int retireSize;

};

// Initialise the stack
int pomStackLfInit( PomStackLfCtx *_ctx );

// Clear the stack and free memory
int pomStackLfClear( PomStackLfCtx *_ctx );

/*  Removing these since we aren't guaranteeing that no other threads have
    local copies of nodes that we're removing. Might add this functionality
    at some point.

// Pop all items off the stack (releasing memory ownership to caller)
PomStackLfNode * pomStackLfPopAll( PomStackLfCtx *_ctx );

// Pop many items off the stack. Caller assumes responsibility for freeing the node memory
int pomStackLfPopMany( PomStackLfCtx *_ctx, PomStackLfNode ** _nodes, int _numNodes );

// Cull the stack to a certain size. Caller assumes responsibility for freeing the node memory
int pomStackLfCull( PomStackLfCtx *_ctx, PomStackLfNode ** _nodes, int _numNodes );

// Push many nodes onto the stack (must be null terminated)
int pomStackLfPushMany( PomStackLfCtx *_ctx, PomStackLfNode * _nodes );

*/

// Pop a single item off the stack
void * pomStackLfPop( PomStackLfCtx *_ctx );


// Push a single item onto the stack
int pomStackLfPush( PomStackLfCtx *_ctx, void * _data );


#endif // STACK_H