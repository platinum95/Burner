#include "stack.h"
#include <stdlib.h>

// Initialise the stack
int pomStackInit( PomStackCtx *_ctx ){
    _ctx->head = NULL;
    return 0;
}

// Clear the stack and free memory
int pomStackClear( PomStackCtx *_ctx ){
    // TODO - implement this cleaning code
    return 0;
}

// Pop all items off the stack
PomStackNode * pomStackPopAll( PomStackCtx *_ctx ){
    PomStackNode *toRet = _ctx->head;
    _ctx->head = NULL;
    return toRet;
}

// Pop a single item off the stack
void * pomStackPop( PomStackCtx *_ctx ){
    PomStackNode * toPop = _ctx->head;
    _ctx->head = toPop->next;
    void *data = toPop->data;
    free( toPop );
    return data;
}

// Push a single item onto the stack
int pomStackPush( PomStackCtx *_ctx, void * _data ){
    PomStackNode *newNode = (PomStackNode*) malloc( sizeof( PomStackNode ) );
    newNode->next = _ctx->head;
    newNode->data = _data;
    _ctx->head = newNode;

    return 0;
}

// Push many nodes onto the stack
int pomStackPushMany( PomStackCtx *_ctx, PomStackNode * _nodes ){
    PomStackNode *newNodeTail = _nodes;
    while( newNodeTail->next ){
        newNodeTail = newNodeTail->next;
    }
    newNodeTail->next = _ctx->head;
    _ctx->head = newNodeTail;

    return 0;
}