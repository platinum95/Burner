#include "linkedlist.h"
#include <stdlib.h>

int pomLinkedListInit( PomLinkedListCtx *_ctx ){
    _ctx->head = NULL;
    _ctx->tail = NULL;
    _ctx->size = 0;

    return 0;
}

int pomLinkedListClear( PomLinkedListCtx *_ctx ){
    // TODO - implement
    return 0;
}

int pomLinkedListAdd( PomLinkedListCtx *_ctx, PllKeyType key ){
    PomLinkedListNode *newNode = (PomLinkedListNode*) malloc( sizeof( PomLinkedListNode ) );
    newNode->key = key;
    newNode->next = NULL;
    if( !_ctx->head ){
        newNode->prev = NULL;
        _ctx->head = newNode;
        _ctx->tail = newNode;
        _ctx->size = 1;
        return 0;
    }
    newNode->prev = _ctx->tail;
    _ctx->tail->next = newNode;
    _ctx->tail = newNode;
    return 0;
}

PomLinkedListNode * pomLinkedListFind( PomLinkedListCtx *_ctx, PllKeyType _key ){
    for( PomLinkedListNode *node = _ctx->head; node; node=node->next ){
        if( _key == node->key ){
            return node;
        }
    }
    return NULL;
}