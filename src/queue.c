#include "queue.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>

#define some_threshold 20

PomQueueNode *pomQueueRequestNode( PomHpGlobalCtx *_hpgctx ){
    PomQueueNode *node = (PomQueueNode*) pomHpRequestNode( _hpgctx );
    if( !node ){
         atomic_fetch_add( &_hpgctx->allocCntr, 1 );
         node = (PomQueueNode*) malloc( sizeof( PomQueueNode ) );
    }
    return node;
}

int pomQueueRetireNode( PomHpGlobalCtx *_hpgctx, PomHpLocalCtx *_hplctx, PomQueueNode *_node ){
    
    pomHpRetireNode( _hpgctx, _hplctx, _node );

    int relStackSize = _hpgctx->releasedPtrs->stackSize;
    // TODO - change this some_threshold stuff. Maybe increase it on each culling
    if( relStackSize > some_threshold ){
        // Cull the retired list down to free some space
        // TODO - reimplement this
        /*
        PomStackLfNode * nNodes;
        int ret = pomStackLfCull( _hpgctx->releasedPtrs, &nNodes, some_threshold / 2 );
        if( ret == 0 ){
            // Didn't pop any
            return 0;
        }
        PomStackLfNode *currStackNode = nNodes;
        while( currStackNode ){
            PomQueueNode * currQNode = (PomQueueNode*) currStackNode->data;
            free( currQNode );
            //free( currStackNode );
            currStackNode = currStackNode->next;
            atomic_fetch_add( &_hpgctx->freeCntr, 1 );
        }
        */
    }

    return 0;
}


int pomQueueInit( PomQueueCtx *_ctx, size_t _dataLen ){
    PomQueueNode * dummyNode = (PomQueueNode*) malloc( sizeof( PomQueueNode ) );
    dummyNode->data = NULL;
    atomic_init( &dummyNode->next, NULL );
    _ctx->head = dummyNode;
    _ctx->tail = dummyNode;
    atomic_init( &_ctx->queueLength, 0 );
    return 0;
    //_ctx->dataLen = _dataLen;
}

int pomQueuePush( PomQueueCtx *_ctx, PomHpGlobalCtx *_hpgctx, PomHpLocalCtx *_hplctx, void * _data ){
    PomQueueNode *newNode = pomQueueRequestNode( _hpgctx );
    PomQueueNode *nullNode = NULL;
    newNode->next = NULL;
    newNode->data = _data;
    PomQueueNode *tail;
    while( 1 ){
        tail = atomic_load( &_ctx->tail );
        // Ensure this is atomic
        pomHpSetHazard( _hplctx, tail, 0 );
        if( tail != atomic_load( &_ctx->tail ) ){
            // Tail has been updated, reloop
            continue;
        }
        PomQueueNode * next = atomic_load( &tail->next );
        if( tail != atomic_load( &_ctx->tail ) ){
            continue;
        }
        if( next ){
            atomic_compare_exchange_strong( &_ctx->tail, &tail, next );
            continue;
        }
        if( atomic_compare_exchange_strong( &tail->next, &nullNode, newNode ) ){
            // Enque successful
            break;
        }
    }
    atomic_compare_exchange_strong( &_ctx->tail, &tail, newNode );
    pomHpSetHazard( _hplctx, NULL, 0 );
    atomic_fetch_add( &_ctx->queueLength, 1 );
    return 0;
}

void * pomQueuePop( PomQueueCtx *_ctx, PomHpGlobalCtx *_hpctx, PomHpLocalCtx *_hplctx ){
    PomQueueNode *head, *tail, *next;
    void *data;
    while( 1 ){
        head = atomic_load( &_ctx->head );
        pomHpSetHazard( _hplctx, head, 0 );
        if( head != atomic_load( &_ctx->head ) ){
            continue;
        }
        tail = atomic_load( &_ctx->tail );
        next = atomic_load( &head->next );
        pomHpSetHazard( _hplctx, next, 1 );
        if( head != atomic_load( &_ctx->head ) ){
            continue;
        }
        if( !next ){
            // Empty queue
            pomHpSetHazard( _hplctx, NULL, 0 );
            pomHpSetHazard( _hplctx, NULL, 1 );
            return NULL;
        }
        if( head == tail ){
            atomic_compare_exchange_strong( &_ctx->tail, &tail, next  );
            continue;
        }
        data = next->data;
        if( atomic_compare_exchange_strong( &_ctx->head, &head, next ) ){
            break;
        }
    }
    pomHpSetHazard( _hplctx, NULL, 0 );
    pomHpSetHazard( _hplctx, NULL, 1 );
    pomQueueRetireNode( _hpctx, _hplctx, head );
    //pomHpRetireNode( _hpctx, _hplctx, head );
    atomic_fetch_add( &_ctx->queueLength, -1 );
    return data;
}

uint32_t pomQueueLength( PomQueueCtx *_ctx ){
    return atomic_load( &_ctx->queueLength );
}

#include <stdio.h>

int pomQueueClear( PomQueueCtx *_ctx, PomHpGlobalCtx *_hpctx ){
    // Assume at this point that no other threads are going to continue adding/removing stuff
    if( atomic_load( &_ctx->queueLength ) ){
        // Clear any remaining items.
        // Unfreed data is lost (responsibility of queue owner to free them before calling this)
        PomQueueNode * qNode = _ctx->head;
        while( qNode ){
            PomQueueNode *nNode = qNode->next;
            free( qNode );
            qNode = nNode;
        }
    }else{
        // Free the dummy node
        free( _ctx->head );
    }

    // Now go through the hazard pointer's released list to free up remaining nodes
    int i = 0;
    PomQueueNode *relStackNode;
    while( ( relStackNode = (PomQueueNode*) pomStackLfPop( _hpctx->releasedPtrs ) ) ){
        i++;
        free( relStackNode );
        atomic_fetch_add( &_hpctx->freeCntr, 1 );
    }
    printf( "%i nodes in released list\n", i );
    return 0;
}