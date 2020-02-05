#include <stdatomic.h>
#include <stddef.h>
#include "stack.h"

typedef struct PomHpRec PomHpRec;
typedef struct PomHpGlobalCtx PomHpGlobalCtx;
typedef struct PomHpLocalCtx PomHpLocalCtx;

struct PomHpGlobalCtx{
    PomHpRec * _Atomic hpHead; // Atomic pointer to a hp record
    _Atomic size_t rNodeThreshold;
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
int pomHpRetireNode( PomHpGlobalCtx *_ctx, PomHpLocalCtx *_lctx, void *_node );

// Set a hazard pointer in the thread-local list
int pomHpSetHazard( PomHpLocalCtx *_lctx, void* _ptr, size_t idx );