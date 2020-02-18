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
    _ctx->head = NULL;
    return 0;
}

// Pop all items off the stack
PomCommonNode * pomStackPopAll( PomStackCtx *_ctx ){
    PomCommonNode *toRet = _ctx->head;
    _ctx->head = NULL;
    return toRet;
}

// Pop a single item off the stack
PomCommonNode * pomStackPop( PomStackCtx *_ctx ){
    PomCommonNode * toPop = _ctx->head;
    _ctx->head = toPop->next;
    return toPop;
}

// Push a single item onto the stack
int pomStackPush( PomStackCtx *_ctx, PomCommonNode * _data ){
    _data->next = _ctx->head;
    _ctx->head = _data;

    return 0;
}

// Push many nodes onto the stack
int pomStackPushMany( PomStackCtx *_ctx, PomCommonNode * _nodes ){
    PomCommonNode *newNodeTail = _nodes;
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
PomCommonNode * pomStackTsPopAll( PomStackTsCtx *_ctx ){
    mtx_lock( &_ctx->mtx );
    PomCommonNode *toRet =_ctx->head;
    _ctx->head = NULL;
    _ctx->stackSize = 0;
    mtx_unlock( &_ctx->mtx );

    return toRet;
}

// Pop a single item off the stack
PomCommonNode * pomStackTsPop( PomStackTsCtx *_ctx ){
    mtx_lock( &_ctx->mtx );
    PomCommonNode *node = _ctx->head;
    if( !node ){
        mtx_unlock( &_ctx->mtx );
        return NULL;
    }
    PomCommonNode *nNode = node->next;

    _ctx->head = nNode;
    _ctx->stackSize--;
    mtx_unlock( &_ctx->mtx );

    return node;
}

// Push a single item onto the stack
int pomStackTsPush( PomStackTsCtx *_ctx, PomCommonNode * _data ){
    mtx_lock( &_ctx->mtx );
    _data->next = _ctx->head;
    _ctx->head = _data;
    _ctx->stackSize++;
    mtx_unlock( &_ctx->mtx );

    return 0;
}

int pomStackTsPopMany( PomStackTsCtx *_ctx, PomCommonNode ** _nodes, int _numNodes ){
    mtx_lock( &_ctx->mtx );
    // Cycle to end of list and set it to the current head
    PomCommonNode *head = _ctx->head;
    PomCommonNode *curNode = _ctx->head;
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

int pomStackTsCull( PomStackTsCtx *_ctx, PomCommonNode ** _nodes, int _numNodes ){
    mtx_lock( &_ctx->mtx );
    if( _numNodes <= _ctx->stackSize ){
        mtx_unlock( &_ctx->mtx );
        return 0;
    }
    int toCull = _ctx->stackSize - _numNodes;
    // Cycle to end of list and set it to the current head
    PomCommonNode *head = _ctx->head;
    PomCommonNode *curNode = _ctx->head;
    if( !curNode ){
        _nodes = NULL;
        mtx_unlock( &_ctx->mtx );
        return 0;
    }

    int nCount = 1;
    while( curNode->next && nCount < toCull ){
        nCount++;
        curNode = curNode->next;
    }
    
    _ctx->head = curNode->next;
    
    *_nodes = head;

    mtx_unlock( &_ctx->mtx );
    return nCount;
}

// Push many nodes onto the stack (must be null terminated)
int pomStackTsPushMany( PomStackTsCtx *_ctx, PomCommonNode * _nodes ){
    mtx_lock( &_ctx->mtx );
    // Cycle to end of list and set it to the current head
    PomCommonNode **curNode = &_nodes;
    if( *curNode ){
        _ctx->stackSize++;
        while( (*curNode)->next ){
            _ctx->stackSize++;
            curNode = &(*curNode)->next;
        }
    }

    (*curNode)->next = _ctx->head;
    _ctx->head = _nodes;

    mtx_unlock( &_ctx->mtx );

    return 0;
}