#ifndef HAZARD_PTR_H
#define HAZARD_PTR_H

#include <stdatomic.h>
#include <stddef.h>
#include "common.h"
#include "stack.h"

typedef struct PomHpRec PomHpRec;
typedef struct PomHpGlobalCtx PomHpGlobalCtx;
typedef struct PomHpLocalCtx PomHpLocalCtx;

typedef struct PomHpStackCtx PomHpStackCtx;

struct PomHpGlobalCtx{
    PomHpRec * _Atomic hpHead; // Atomic pointer to a hp record
    _Atomic size_t rNodeThreshold;
    PomHpStackCtx * releasedPtrs;
    _Atomic int allocCntr, freeCntr;
};

struct PomHpLocalCtx{
    PomHpRec *hp;
    size_t numHp;
    // Need some list type here, doesn't need to be type-safe
    PomStackCtx *rlist;
    size_t rcount;
};

// Initialise the hazard pointer handler (call once per process)
int pomHpGlobalInit( PomHpGlobalCtx *_ctx );

// Initalise the thread-specific context (call once per thread)
int pomHpThreadInit( PomHpGlobalCtx *_ctx, PomHpLocalCtx *_lctx, size_t _numHp );

// Mark a node for retirement
int pomHpRetireNode( PomHpGlobalCtx *_ctx, PomHpLocalCtx *_lctx, PomCommonNode *_node );

// Set a hazard pointer in the thread-local list
int pomHpSetHazard( PomHpLocalCtx *_lctx, PomCommonNode *_ptr, size_t idx );

// Clear the thread-local hazard pointer data
int pomHpThreadClear( PomHpGlobalCtx *_ctx, PomHpLocalCtx *_lctx );

// Clear the global hazard pointer data
int pomHpGlobalClear( PomHpGlobalCtx *_ctx );

// Request a node from the released list. Returns NULL if none available
PomCommonNode *pomHpRequestNode( PomHpGlobalCtx *_ctx );

#endif