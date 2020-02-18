#include <stdlib.h>
#include "hazard_ptr.h"
#include "linkedlist.h"

/*
For this module, we implement a simple, "weak" stack
to keep track of the retired nodes.
The "weak" identifier just means that operations on 
the stack may fail, so it's more of an opportunistic
set of optimisations, rather than a guaranteed
function.
Some declarations for the stack are below.
Stack definitions are below HP definitions
*/

// TODO - separate stack implementation may not be required,
// so integrate it into the HP code

struct PomHpStackCtx{
    PomCommonNode * _Atomic head;
    _Atomic int stackSize;
};

// Initialise the stack
int pomHpStackInit( PomHpStackCtx *_ctx );

// Clear the stack and free memory
PomCommonNode* pomHpStackDestroy( PomHpStackCtx *_ctx );

// Pop a single item off the stack
PomCommonNode* pomHpStackPop( PomHpStackCtx *_ctx );

// Push a single item onto the stack
int pomHpStackPush( PomHpStackCtx *_ctx, PomCommonNode * _data );

/**********************************
* Begin Hazard Pointer Definitions
***********************************/

struct PomHpRec{
    void * _Atomic hazardPtr; // Atomic pointer to the hazard pointer
    PomHpRec * _Atomic next; // Atomic pointer to the next record
};

int pomHpScan( PomHpGlobalCtx *_ctx, PomHpLocalCtx *_lctx );

int pomHpGlobalInit( PomHpGlobalCtx *_ctx ){
    PomHpRec* newHead = (PomHpRec*) malloc( sizeof( PomHpRec ) ); // Dummy node
    atomic_init( &_ctx->hpHead, newHead );
    atomic_init( &_ctx->hpHead->next, NULL );
    atomic_init( &_ctx->hpHead->hazardPtr, NULL );
    atomic_init( &_ctx->rNodeThreshold, 10 ); // TODO have a proper threshold
    _ctx->releasedPtrs = (PomHpStackCtx*) malloc( sizeof( PomHpStackCtx ) );
    pomHpStackInit( _ctx->releasedPtrs );

    atomic_init( &_ctx->allocCntr, 0 );
    atomic_init( &_ctx->freeCntr, 0 );

    return 0;
}

int pomHpThreadInit( PomHpGlobalCtx *_ctx, PomHpLocalCtx *_lctx, size_t _numHp ){
    
    // Create new HP records for this thread
    PomHpRec *newHps = (PomHpRec*) malloc( sizeof( PomHpRec ) * _numHp );
    atomic_init( &newHps[ 0 ].hazardPtr, NULL );
    atomic_init( &newHps[ 0 ].next, NULL );
    for( uint32_t i = 1; i < _numHp; i++ ){
        atomic_init( &newHps[ i-1 ].hazardPtr, NULL );
        atomic_init( &newHps[ i-1 ].next, &newHps[ i ] );
    }
    atomic_init( &newHps[ _numHp - 1 ].hazardPtr, NULL );
    atomic_init( &newHps[ _numHp - 1 ].next, NULL );
    // TODO - Make sure the new hazard pointers are set before proceeding
    // (i.e. make sure this fence and the relaxed inits make sense)
    //atomic_thread_fence( memory_order_release );
    //atomic_thread_fence( memory_order_seq_cst );
    _lctx->hp = newHps;
    _lctx->rlist = (PomStackCtx*) malloc( sizeof( PomStackCtx ) );
    pomStackInit( _lctx->rlist );
    _lctx->numHp = _numHp;
    _lctx->rcount = 0;

    // Try to emplace the new hazard pointers onto the list
    // TODO - for now we're assuming the global HPrec can't have nodes removed from it
    // so fix that at some point.
    // TODO - make the next-reads atomic
    PomHpRec * _Atomic currNode;
    PomHpRec *expNode = NULL;
    currNode = _ctx->hpHead;
    while( 1 ){
        if( !currNode->next ){
            // If we think we're at the tail, try slot our nodes into it, provided it's still NULL
            // TODO - consider making this memory_order_release
            if( atomic_compare_exchange_weak( &currNode->next, &expNode, newHps ) ){
                // Tail was set
                break;
            }
            else{
                // Tail was not set, so start over
                currNode = _ctx->hpHead;
                continue;
            }
        }
        else{
            currNode = currNode->next;
        }
    }
    return 0;
}

// TODO - maybe just free the ndoes here rather than pushing them all to the
// retired list
// Clear the thread-local hazard pointer data
int pomHpThreadClear( PomHpGlobalCtx *_ctx, PomHpLocalCtx *_lctx ){
    // Iterate over the list and free any dangling retired nodes.
    // Assume at this point that no pointers are hazards as other threads should be exited
    // TODO - ensure theres no double-freeing (might not be a problem)
    PomCommonNode *retireNodes = pomStackPopAll( _lctx->rlist );
    PomCommonNode *currNode = retireNodes;
    while( currNode ){
        PomCommonNode * nextNode = currNode->next;
        // Free the stack node and the queue node hazard pointer)
        currNode->next = NULL;
        pomHpStackPush( _ctx->releasedPtrs, currNode );
        currNode = nextNode;
    }

    pomStackClear( _lctx->rlist );
    free( _lctx->rlist );
    free( _lctx->hp );

    return 0;
}

PomCommonNode *pomHpRequestNode( PomHpGlobalCtx *_ctx ){
    PomCommonNode *nNode = pomHpStackPop( _ctx->releasedPtrs );
    if( !nNode ){
        nNode = (PomCommonNode*) malloc( sizeof( PomCommonNode ) );
        atomic_fetch_add( &_ctx->allocCntr, 1 );
    }
    atomic_store( &nNode->aNext, NULL );
    return nNode;
}
#include <stdio.h>
// Clear the global hazard pointer data
int pomHpGlobalClear( PomHpGlobalCtx *_ctx ){
    PomCommonNode *rNode = pomHpStackDestroy( _ctx->releasedPtrs );
    while( rNode ){
        PomCommonNode *nNode = rNode->next;
        free( rNode );
        rNode = nNode;
        atomic_fetch_add( &_ctx->freeCntr, 1 );
    }
    free( _ctx->releasedPtrs );
    // Just need to free the dummy node at the head of the list
    free( _ctx->hpHead );

    int freeCnt = atomic_load( &_ctx->freeCntr );
    int allocCnt = atomic_load( &_ctx->allocCntr );
    printf( "Allocated %i, freed %i\n", allocCnt, freeCnt );
    return 0;
}


int pomHpScan( PomHpGlobalCtx *_ctx, PomHpLocalCtx *_lctx ){
    // Stage 1 - scan each thread's hazard pointers and add non-null values to local list
    PomLinkedListCtx plist;
    pomLinkedListInit( &plist );
    // TODO - consider making the HpRec loads memory_order_acquire
    // They're current seq_cst so hazardPtr should be loaded OK despite 
    // being stored with mem_order_relaxed
    PomHpRec * hpRec = atomic_load( &_ctx->hpHead );
    while( hpRec ){
        void * ptr = atomic_load( &hpRec->hazardPtr );
        if( ptr ){
            pomLinkedListAdd( &plist, (PllKeyType) ptr );
        }
        hpRec = atomic_load( &hpRec->next );
    }

    // Stage 2 - check plist for the nodes we're trying to retire
    PomCommonNode *retireNodes = pomStackPopAll( _lctx->rlist );
    _lctx->rcount = 0;

    PomCommonNode *currNode = retireNodes;
    while( currNode ){
        PomCommonNode * nextNode = currNode->next;
        if( pomLinkedListFind( &plist, (PllKeyType) currNode ) ){
            // Pointer to retire is currently used (is a hazard pointer)
            currNode->next = NULL;
            pomStackPush( _lctx->rlist, currNode );
            _lctx->rcount++;
        }else{
            // Can now release/reuse the retired pointer
            pomHpStackPush( _ctx->releasedPtrs, currNode );
        }
        currNode = nextNode;
    }
    
    // Can clean up plist now
    pomLinkedListClear( &plist );

    return 0;
}

int pomHpRetireNode( PomHpGlobalCtx *_ctx, PomHpLocalCtx *_lctx, PomCommonNode *_ptr ){
    // Push the node onto the thread-local list
    
    pomStackPush( _lctx->rlist, _ptr );
    _lctx->rcount++;

    if( _lctx->rcount > atomic_load( &_ctx->rNodeThreshold ) ){
        pomHpScan( _ctx, _lctx );
    }
    return 0;
}   

int pomHpSetHazard( PomHpLocalCtx *_lctx, PomCommonNode *_ptr, size_t idx ){
    if( idx >= _lctx->numHp ){
        // Invalid HP index
        return 1;
    }
    PomHpRec *hpRecord = _lctx->hp + idx;
    // TODO - consider setting this to relaxed (?) or release
    atomic_store( &hpRecord->hazardPtr, _ptr );
    return 0;
}

/************************
* Begin stack definitions
*************************/

// Initialise the stack
int pomHpStackInit( PomHpStackCtx *_ctx ){
    atomic_init( &_ctx->head, NULL );
    atomic_init( &_ctx->stackSize, 0 );
    return 0;
}

// Clear the stack and return the nodes
PomCommonNode* pomHpStackDestroy( PomHpStackCtx *_ctx ){
    // Empty the stack
    PomCommonNode *cNode;
    do{
        cNode = atomic_load( &_ctx->head );
    }while( !atomic_compare_exchange_weak( &_ctx->head, &cNode, NULL ) );
    
    return cNode;
}

// Push a single item onto the stack
int pomHpStackPush( PomHpStackCtx *_ctx, PomCommonNode * _node ){
    // TODO - solve ABA problem here
    PomCommonNode *head;
    do{
        head = atomic_load( &_ctx->head );
        atomic_store( &_node->aNext, head );
    }while( !atomic_compare_exchange_weak( &_ctx->head, &head, _node ) );

    atomic_fetch_add( &_ctx->stackSize, 1 );
    return 0;
}

// Pop a single item off the stack
PomCommonNode * pomHpStackPop( PomHpStackCtx *_ctx ){
    PomCommonNode *head, *next;
    // TODO - solve ABA problem here
    do{
        head = atomic_load( &_ctx->head );
        if( !head ){
            return NULL;
        }
        next = atomic_load( &head->next );
    }while( !atomic_compare_exchange_weak( &_ctx->head, &head, next ) );

    atomic_store( &head->next, NULL );
    atomic_fetch_add( &_ctx->stackSize, -1 );
    
    return head;
}
