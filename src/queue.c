#include "queue.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>

#define some_threshold 20

int pomQueueInit( PomQueueCtx *_ctx ){
    PomCommonNode * dummyNode = (PomCommonNode*) malloc( sizeof( PomCommonNode ) );
    dummyNode->data = NULL;
    atomic_init( &dummyNode->next, NULL );
    _ctx->head = dummyNode;
    _ctx->tail = dummyNode;
    atomic_init( &_ctx->queueLength, 0 );
    return 0;
    //_ctx->dataLen = _dataLen;
}

int pomQueuePush( PomQueueCtx *_ctx, PomHpGlobalCtx *_hpgctx, PomHpLocalCtx *_hplctx, void * _data ){
    PomCommonNode *newNode = (PomCommonNode*) pomHpRequestNode( _hpgctx );
    PomCommonNode *nullNode = NULL;
    newNode->next = NULL;
    newNode->data = _data;
    PomCommonNode *tail;
    while( 1 ){
        tail = atomic_load( &_ctx->tail );
        // Ensure this is atomic
        pomHpSetHazard( _hplctx, tail, 0 );
        if( tail != atomic_load( &_ctx->tail ) ){
            // Tail has been updated, reloop
            continue;
        }
        PomCommonNode * next = atomic_load( &tail->next );
        if( tail != atomic_load( &_ctx->tail ) ){
            continue;
        }
        if( next ){
            atomic_compare_exchange_strong( &_ctx->tail, &tail, next );
            continue;
        }
        if( atomic_compare_exchange_strong( &tail->next, &nullNode, newNode ) ){
            // Enqueue successful
            break;
        }
    }

    atomic_compare_exchange_strong( &_ctx->tail, &tail, newNode );
    pomHpSetHazard( _hplctx, NULL, 0 );
    atomic_fetch_add( &_ctx->queueLength, 1 );
    return 0;
}

void * pomQueuePop( PomQueueCtx *_ctx, PomHpGlobalCtx *_hpctx, PomHpLocalCtx *_hplctx ){
    PomCommonNode *head, *tail, *next;
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
    pomHpRetireNode( _hpctx, _hplctx, head );
    //pomHpRetireNode( _hpctx, _hplctx, head );
    atomic_fetch_add( &_ctx->queueLength, -1 );
    return data;
}

uint32_t pomQueueLength( PomQueueCtx *_ctx ){
    return atomic_load( &_ctx->queueLength );
}

#include <stdio.h>

int pomQueueClear( PomQueueCtx *_ctx, PomHpGlobalCtx *_hpctx, PomHpLocalCtx *_hplctx ){
    // Assume at this point that no other threads are going to continue adding/removing stuff.

    // Return all the queue nodes to HP handler, i.e. retire them. HP will clear them up
    // later. 
    if( atomic_load( &_ctx->queueLength ) ){
        // Clear any remaining items.
        // Unfreed data is lost (responsibility of queue owner to free them before calling this)
        PomCommonNode * qNode = _ctx->head;
        while( qNode ){
            PomCommonNode *nNode = qNode->next;
            //free( qNode );
            pomHpRetireNode( _hpctx, _hplctx, qNode );
            qNode = nNode;
        }
    }else{
        // Free the dummy node
        free( _ctx->head );
    }

    return 0;
}