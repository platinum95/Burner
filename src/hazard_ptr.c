#include <stdlib.h>
#include "hazard_ptr.h"
#include "linkedlist.h"


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
    _ctx->releasedPtrs = (PomStackLfCtx*) malloc( sizeof( PomStackLfCtx ) );
    pomStackLfInit( _ctx->releasedPtrs );

    atomic_init( &_ctx->allocCntr, 0 );
    atomic_init( &_ctx->freeCntr, 0 );

    return 0;
}

int pomHpThreadInit( PomHpGlobalCtx *_ctx, PomHpLocalCtx *_lctx, size_t _numHp ){
    
    // Create new HP records for this thread
    PomHpRec *newHps = (PomHpRec*) malloc( sizeof( PomHpRec ) * _numHp );
    atomic_init( &newHps[ 0 ].hazardPtr, NULL );
    atomic_init( &newHps[ 0 ].next, NULL );
    for( int i = 1; i < _numHp; i++ ){
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

// Clear the thread-local hazard pointer data
int pomHpThreadClear( PomHpGlobalCtx *_ctx, PomHpLocalCtx *_lctx ){
    // Iterate over the list and free any dangling retired nodes.
    // Assume at this point that no pointers are hazards as other threads should be exited
    // TODO - ensure theres no double-freeing (might not be a problem)
    PomStackNode *retireNodes = pomStackPopAll( _lctx->rlist );
    PomStackNode *currNode = retireNodes;
    while( currNode ){
        PomStackNode * nextNode = currNode->next;
        // Free the stack node and the queue node hazard pointer)
        currNode->next = NULL;
        pomStackLfPush( _ctx->releasedPtrs, currNode->data );
        free( currNode );
        currNode = nextNode;
    }

    pomStackClear( _lctx->rlist);
    free( _lctx->rlist );
    free( _lctx->hp );

    return 0;
}

void *pomHpRequestNode( PomHpGlobalCtx *_ctx ){
    return pomStackLfPop( _ctx->releasedPtrs );
}

// Clear the global hazard pointer data
int pomHpGlobalClear( PomHpGlobalCtx *_ctx ){
    pomStackLfClear( _ctx->releasedPtrs );
    free( _ctx->releasedPtrs );
    // Just need to free the dummy node at the head of the list
    free( _ctx->hpHead );
    return 0;
}


int pomHpScan( PomHpGlobalCtx *_ctx, PomHpLocalCtx *_lctx ){
    // Stage 1 - scan each thread's hazard pointers and add non-null values to local list
    PomLinkedListCtx *plist = (PomLinkedListCtx*) malloc( sizeof( PomLinkedListCtx ) );
    pomLinkedListInit( plist );
    // TODO - consider making the HpRec loads memory_order_acquire
    // They're current seq_cst so hazardPtr should be loaded OK despite 
    // being stored with mem_order_relaxed
    PomHpRec * hpRec = atomic_load( &_ctx->hpHead );
    while( hpRec ){
        void * ptr = atomic_load( &hpRec->hazardPtr );
        if( ptr ){
            pomLinkedListAdd( plist, (PllKeyType) ptr );
        }
        hpRec = atomic_load( &hpRec->next );
    }

    // Stage 2 - check plist for the nodes we're trying to retire
    PomStackNode *retireNodes = pomStackPopAll( _lctx->rlist );
    _lctx->rcount = 0;

    PomStackNode *currNode = retireNodes;
    while( currNode ){
        PomStackNode * nextNode = currNode->next;
        if( pomLinkedListFind( plist, (PllKeyType) currNode->data ) ){
            // Pointer to retire is currently used (is a hazard pointer)
            currNode->next = NULL;
            pomStackPushMany( _lctx->rlist, currNode );
            _lctx->rcount++;
        }else{
            // Can now release/reuse the retired pointer
            pomStackLfPush( _ctx->releasedPtrs, currNode->data );
            free( currNode ); // Free the node (not the hazard pointer)
        }
        currNode = nextNode;
    }
    
    // Can clean up plist now
    pomLinkedListClear( plist );
    free( plist );

    return 0;
}

int pomHpRetireNode( PomHpGlobalCtx *_ctx, PomHpLocalCtx *_lctx, void *_ptr ){
    // Push the node onto the thread-local list
    
    pomStackPush( _lctx->rlist, _ptr );
    _lctx->rcount++;

    if( _lctx->rcount > atomic_load( &_ctx->rNodeThreshold ) ){
        pomHpScan( _ctx, _lctx );
    }
    return 0;
}   

int pomHpSetHazard( PomHpLocalCtx *_lctx, void* _ptr, size_t idx ){
    if( idx >= _lctx->numHp ){
        // Invalid HP index
        return 1;
    }
    PomHpRec *hpRecord = _lctx->hp + idx;
    // TODO - consider setting this to relaxed (?) or release
    atomic_store( &hpRecord->hazardPtr, _ptr );
    return 0;
}

