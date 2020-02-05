#include <stdlib.h>
#include "hazard_ptr.h"
#include "linkedlist.h"


struct PomHpRec{
    void *hazardPtr;
    volatile PomHpRec *next;
};

int pomHpScan( PomHpGlobalCtx *_ctx, PomHpLocalCtx *_lctx );

int pomHpGlobalInit( PomHpGlobalCtx *_ctx ){
    _ctx->hpHead = (PomHpRec*) malloc( sizeof( PomHpRec ) ); // Dummy node
    _ctx->hpHead->next = NULL;
    _ctx->hpHead->hazardPtr = NULL;
    _ctx->rNodeThreshold = 10; // TODO have a proper threshold
    return 0;
}

int pomHpThreadInit( PomHpGlobalCtx *_ctx, PomHpLocalCtx *_lctx, size_t _numHp ){
    _lctx->rlist = (PomStackCtx*) malloc( sizeof( PomStackCtx ) );
    pomStackInit( _lctx->rlist );
    
    PomHpRec *newHps = (PomHpRec*) malloc( sizeof( PomHpRec ) * _numHp );
    newHps[ 0 ].hazardPtr = NULL;
    newHps[ 0 ].next = NULL;
    for( int i = 1; i < _numHp; i++ ){
        newHps[ i-1 ].hazardPtr = NULL;
        newHps[ i-1 ].next = &newHps[ i ];
    }
    // TODO - Make sure the new hazard pointers are set before proceeding
    //atomic_thread_fence( memory_order_release );

    // Try to emplace the new hazard pointers onto the list
    // TODO - for now we're assuming the global HPrec can't have nodes removed from it
    // so fix that at some point.
    // TODO - make the next-reads atomic
    volatile PomHpRec *currNode;
    PomHpRec *expNode = NULL;
    currNode = _ctx->hpHead;
    while( 1 ){
        if( !currNode->next){
            // If we think we're at the tail, try slot our nodes into it, provided it's still NULL
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

int pomHpScan( PomHpGlobalCtx *_ctx, PomHpLocalCtx *_lctx ){
    // Stage 1 - scan each thread's hazard pointers and add non-null values to local list
    PomLinkedListCtx *plist = (PomLinkedListCtx*) malloc( sizeof( PomLinkedListCtx ) );
    pomLinkedListInit( plist );
    volatile PomHpRec *hpRec = _ctx->hpHead;
    while( hpRec ){
        // TODO - atomic read here
        void * ptr = hpRec->hazardPtr;
        if( ptr ){
            pomLinkedListAdd( plist, (PllKeyType) ptr );
        }
        hpRec = hpRec->next;
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
            // TODO - handle the pointer release here
            free( currNode->data );
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
    // TODO - fix this awful code, i.e. implement a proper sorted linked list module
    
    pomStackPush( _lctx->rlist, _ptr );
    _lctx->rcount++;

    if( _lctx->rcount > _ctx->rNodeThreshold ){
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
    // TODO atomically set this pointer
    hpRecord->hazardPtr = _ptr;
    return 0;
}

