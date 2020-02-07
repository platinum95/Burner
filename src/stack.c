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



/***************
* TS version
***************/

// TODO - implement a lock-free version

// Initialise the stack
int pomStackTsInit( PomStackTsCtx *_ctx ){
    _ctx->head = NULL;
    _ctx->stackSize = 0;
    mtx_init( &_ctx->mtx, mtx_plain );
    mtx_unlock( &_ctx->mtx );
    return 0;
}

// Clear the stack and free memory
int pomStackTsClear( PomStackTsCtx *_ctx ){
    mtx_destroy( &_ctx->mtx );
    return 0;
}

// Pop all items off the stack (releasing memory ownership to caller)
PomStackNode * pomStackTsPopAll( PomStackTsCtx *_ctx ){
    mtx_lock( &_ctx->mtx );
    PomStackNode *toRet =_ctx->head;
    _ctx->head = NULL;
    _ctx->stackSize = 0;
    mtx_unlock( &_ctx->mtx );

    return toRet;
}

// Pop a single item off the stack
void * pomStackTsPop( PomStackTsCtx *_ctx ){
    mtx_lock( &_ctx->mtx );
    PomStackNode *node = _ctx->head;
    if( !node ){
        mtx_unlock( &_ctx->mtx );
        return NULL;
    }
    PomStackNode *nNode = node->next;
    void *data = node->data;
    if( node ){
        free( node );
    }
    _ctx->head = nNode;
    _ctx->stackSize--;
    mtx_unlock( &_ctx->mtx );

    return data;
}

// Push a single item onto the stack
int pomStackTsPush( PomStackTsCtx *_ctx, void * _data ){
    mtx_lock( &_ctx->mtx );
    PomStackNode *nNode = (PomStackNode*) malloc( sizeof( PomStackNode ) );
    nNode->data = _data;
    nNode->next = _ctx->head;
    _ctx->head = nNode;
    _ctx->stackSize++;
    mtx_unlock( &_ctx->mtx );

    return 0;
}

int pomStackTsPopMany( PomStackTsCtx *_ctx, PomStackNode ** _nodes, int _numNodes ){
    mtx_lock( &_ctx->mtx );
    // Cycle to end of list and set it to the current head
    PomStackNode *head = _ctx->head;
    PomStackNode *curNode = _ctx->head;
    if( !curNode ){
        _nodes = NULL;
        mtx_unlock( &_ctx->mtx );
        return 0;
    }

    int nCount = 1;
    while( curNode->next && nCount < _numNodes ){
        nCount++;
        curNode = curNode->next;
    }
    
    _ctx->head = curNode->next;
    
    *_nodes = head;

    mtx_unlock( &_ctx->mtx );
    return nCount;
}

// Push many nodes onto the stack (must be null terminated)
int pomStackTsPushMany( PomStackTsCtx *_ctx, PomStackNode * _nodes ){
    mtx_lock( &_ctx->mtx );
    // Cycle to end of list and set it to the current head
    PomStackNode *curNode = _ctx->head;
    if( curNode ){
        _ctx->stackSize++;
        while( curNode->next ){
            _ctx->stackSize++;
            curNode = curNode->next;
        }
    } 
    curNode->next = _ctx->head;
    _ctx->head = curNode;

    mtx_unlock( &_ctx->mtx );

    return 0;
}