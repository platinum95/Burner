#include "queue.h"
#include <stdatomic.h>
#include <stdlib.h>

int pomQueueInit( PomQueueCtx *_ctx, size_t _dataLen ){
    PomQueueNode * dummyNode = (PomQueueNode*) malloc( sizeof( PomQueueNode ) );
    _ctx->head = dummyNode;
    _ctx->tail = dummyNode;
    return 0;
    //_ctx->dataLen = _dataLen;
}

int pomQueuePush( PomQueueCtx *_ctx, PomHpLocalCtx *_hplctx, void * _data ){
    PomQueueNode *newNode = (PomQueueNode*) malloc( sizeof( PomQueueNode ) );
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
    return data;
}


int pomQueueClear( PomQueueCtx *_ctx ){
    // TODO - add clearing code
    return 0;
}