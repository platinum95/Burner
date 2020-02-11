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

int pomStackTsCull( PomStackTsCtx *_ctx, PomStackNode ** _nodes, int _numNodes ){
    mtx_lock( &_ctx->mtx );
    if( _numNodes <= _ctx->stackSize ){
        mtx_unlock( &_ctx->mtx );
        return 0;
    }
    int toCull = _ctx->stackSize - _numNodes;
    // Cycle to end of list and set it to the current head
    PomStackNode *head = _ctx->head;
    PomStackNode *curNode = _ctx->head;
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
int pomStackTsPushMany( PomStackTsCtx *_ctx, PomStackNode * _nodes ){
    mtx_lock( &_ctx->mtx );
    // Cycle to end of list and set it to the current head
    PomStackNode **curNode = &_nodes;
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




/*******************
*  Lockfree version
********************/

// TODO - add proper memory management here. Currently just leaking data
// but need proper hazard-pointer integration for nice node reuse and 
// memory management

// Initialise the stack
int pomStackLfInit( PomStackLfCtx *_ctx ){
    // Create a dummy node for the heads
    // TODO - consider using a single dummy node here
    _ctx->stackDummy = (PomStackLfNode*) malloc( sizeof( PomStackLfNode ) );
    atomic_init( &_ctx->stackDummy->next, NULL );
    atomic_init( &_ctx->stackDummy->data, NULL );
    atomic_init( &_ctx->head, _ctx->stackDummy );

    _ctx->retireDummy = (PomStackLfNode*) malloc( sizeof( PomStackLfNode ) );
    atomic_init( &_ctx->retireDummy->next, NULL );
    atomic_init( &_ctx->retireDummy->data, NULL );
    atomic_init( &_ctx->retiredList, _ctx->retireDummy );

    atomic_init( &_ctx->head, NULL );
    atomic_init( &_ctx->stackSize, 0 );
    atomic_init( &_ctx->retiredList, NULL );
    atomic_init( &_ctx->retireSize, 0 );
    return 0;
}

// Clear the stack and free memory. Call once from a single thread.
// Stored data (i.e. not the stack node) is not freed.
int pomStackLfClear( PomStackLfCtx *_ctx ){
    // Empty the stack
    PomStackLfNode *cNode;
    do{
        cNode = atomic_load( &_ctx->head );
    }while( !atomic_compare_exchange_weak( &_ctx->head, &cNode, NULL ) );
    
    // Free any nodes
    while( cNode ){
        PomStackLfNode *nNode = cNode->next;
        free( cNode );
        cNode = nNode;
    }
    
    //PomStackLfNode *cNode = pomStackLfPopAll( _ctx );
    return 0;
}

// TODO - change this
static int someThreshold = 20;

// TODO - consider combining commonality of checkRetireList and requestNode since
// they kind of do the same thing

// Check if the retire-list can be cleaned up a bit.
// Weak function - basically run until we've freed
// enough nodes, or we fail to remove a once.
int checkRetireList( PomStackLfCtx *_ctx ){
    int currSize = atomic_load( &_ctx->retireSize );
    if( currSize < someThreshold ){
        // Nothing to do, return.
        return 0;
    }
    int toRemove = ( someThreshold - currSize ) / 2;
    
    int iter = 0;
    while( iter < toRemove ){
        // Try remove `toRemove` number of nodes from retired list.

        // First make sure we're still over the threshold
        currSize = atomic_load( &_ctx->retireSize );
        if( currSize < someThreshold ){
            // Nothing to do, return.
            return 0;
        }
        // Next try make the head-retired node inaccessible
        int expVal = 0;

        PomStackLfNode *headRet = _ctx->retiredList;
        if( headRet == _ctx->retireDummy ){
            // Empty list, so return
            break;
        }
        // TODO - this could fail, so maybe add dummy node
        if( !atomic_compare_exchange_strong( &headRet->inUse, &expVal, -1 ) ){
            // If we fail here, just return (weak function)
            return iter;
        }
        // If we get here, we've claimed ownership of the retired list's head node
        // (or at least, the copy we have of what should be the head node)
        
        // Try remove it from the list
        if( !atomic_compare_exchange_strong( &_ctx->retiredList, &headRet, headRet->next ) ){
            // Failed to remove from list, so return
            atomic_store( &headRet->inUse, 0 );
            return iter;
        }
        // If we get here, we've successfully removed the head node from the retired node list,
        // so we can now free it.
        free( headRet );
        iter++;
    }
    return iter;

}

// Retires a given node
int retireNode( PomStackLfCtx *_ctx, PomStackLfNode *_node ){
    PomStackLfNode *retHead;
    do{
        retHead = _ctx->head;
        _node->next = retHead;
    }while( atomic_compare_exchange_weak( &_ctx->retiredList, &retHead, _node ) );
    atomic_fetch_add( &_ctx->retireSize, 1 );
    return 0;
}

// Returns a node with the inUse pointer set to 1
PomStackLfNode *requestNode( PomStackLfCtx *_ctx ){
    PomStackLfNode *newNode = _ctx->retiredList;
    while( 1 ){
        newNode = _ctx->retiredList;
        if( !newNode ){
            break;
        }
        int expVal = 0;
        if( atomic_compare_exchange_weak( &newNode->inUse, &expVal, -1 ) ){
            // We've grabbed hold of the head retired-list node.
            // No other threads have a local copy of this node.
            // Try remove this node from the list. If it fails,
            // another node has been added to the list in the meantime.
            // In that case, reset our inUse value and start again.
            expVal = -1;
            if( !atomic_compare_exchange_weak( &_ctx->retiredList, &newNode, newNode->next ) ){
                // If we get here, the CAS failed. Reset our node ptr and restart the attempt
                atomic_fetch_add( &newNode->inUse, 1 );
                if( atomic_fetch_add( &newNode->inUse, 1 ) != -1 ){
                    // If we get here, the inUse counter of our node had changed at some point.
                    // This shouldn't occur since we should have sole ownership of nodes with
                    // inUse value -1.
                    // TODO - error handling here
                    newNode = NULL;
                    break;
                }
                else{
                    // Retry the node acquisition
                    continue;
                }
            }
            else{
                // If we get here, the CAS succeeded and we've reclaimed a node from the retired list.
                // Set the in-use ptr to 1 to show we're using the node, and return it
                if( atomic_fetch_add( &newNode->inUse, 2 ) != -1 ){
                    // Some error occurred, inUse counter was not -1 which it should have been.
                    newNode = NULL;
                    break;
                }
                else{
                    atomic_fetch_add( &_ctx->retireSize, -1 );
                    return newNode;
                }
            }
        }
    };

    // If we get here, we weren't able to reclaim a node, so create a new one
    newNode = (PomStackLfNode*) malloc( sizeof( PomStackLfNode ) );
    atomic_store( &newNode->inUse, 1 );
    return newNode;
}

// Push a single item onto the stack
int pomStackLfPush( PomStackLfCtx *_ctx, void * _data ){
    PomStackLfNode *newNode = requestNode( _ctx );// (PomStackLfNode*) malloc( sizeof( PomStackLfNode ) );
    newNode->data = _data;
    atomic_store( &newNode->next, NULL );
    
    PomStackLfNode *head;
    do{
        head = atomic_load( &_ctx->head );
        newNode->next = head;
    }while( !atomic_compare_exchange_weak( &_ctx->head, &head, newNode ) );

    atomic_fetch_add( &_ctx->stackSize, 1 );
    
    // We can decrement the inUse counter now that we're exiting.
    // Note that we don't set this to 0 here as another thread may have
    // obtained a copy since we set the stack head
    atomic_fetch_add( &newNode->inUse, -1 );
    return 0;
}

// Pop a single item off the stack
void * pomStackLfPop( PomStackLfCtx *_ctx ){
    PomStackLfNode *head, *next;
    do{
        head = atomic_load( &_ctx->head );
        if( !head || head == _ctx->stackDummy ){
            return NULL;
        }
        int cUse = head->inUse;
        if( cUse == -1 ){
            continue;
        }
        if( !atomic_compare_exchange_strong( &head->inUse, &cUse, cUse + 1 ) ){
            // The node we've loaded is either inaccessible or another node claimed 
            // access to it since we started, so restart the loop to be safe
            continue;
        }
        // TODO - This could fail if the node is freed between the atomic load and the dereference.
        // Do something about that. Maybe dummy node
        next = atomic_load( &head->next );
    }while( !atomic_compare_exchange_weak( &_ctx->head, &head, next ) );

    atomic_store( &head->next, NULL );
    atomic_fetch_add( &_ctx->stackSize, -1 );
    void *data = head->data;
    // Add node to the retired list
    retireNode( _ctx, head );
    // Decrement the access counter
    atomic_fetch_add( &head->inUse, -1 );
    
    // Now try downsize the retired list if required
    checkRetireList( _ctx );
    return data;
}

/*
// Pop many items off the stack. Caller assumes responsibility for freeing the node memory
int pomStackLfPopMany( PomStackLfCtx *_ctx, PomStackLfNode ** _nodes, int _numNodes ){
    *_nodes = NULL;

    int retCount;
    // Loop for all required nodes, or until the stack is empty
    PomStackLfNode *head, *curr;
    do{
        head = atomic_load( &_ctx->head );
        if( !head || head == _ctx->dummy ){
            // Empty list
            return 0;
        }
        curr = head;
        retCount = 1;
        // Find either as many nodes as we need, or the last node
        for( int i = 1; i < _numNodes && curr->next && curr->next != _ctx->dummy; i++ ){
            curr = atomic_load( &curr->next );
            if( !curr ){
                // Data collision, repeat loop
                head = NULL;
                curr = _ctx->dummy;
                break;
            }
            retCount++;
        }
    }while( !atomic_compare_exchange_weak( &_ctx->head, &head, curr->next ) );

    atomic_fetch_add( &_ctx->stackSize, -retCount );
    
    // Find the last node and nullify its next ptr
    //curr = head;
    //for( int i = 1; i < retCount; i++ ){
    //    curr = curr->next;
   // }
    atomic_store( &curr->next, NULL );
    *_nodes = head;
    return retCount;
}

// Cull the stack to a certain size. Caller assumes responsibility for freeing the node memory
int pomStackLfCull( PomStackLfCtx *_ctx, PomStackLfNode ** _nodes, int _numNodes ){
    PomStackLfNode *toKeep = NULL;
    *_nodes = NULL;

    int toCull = atomic_load( &_ctx->stackSize ) - _numNodes;
    if( toCull <= 0 ){
        // Stack is already smaller than required
        return 0;
    }
    // Pop off as many nodes as required
    int newSize = pomStackLfPopMany( _ctx, &toKeep, _numNodes );
    // Add the dummy node to the end of our list
    PomStackLfNode *cNode = toKeep;
    if( !cNode ){
        cNode = _ctx->dummy;
    }
    else{
        while( cNode->next ){
            cNode = cNode->next;
        }
        atomic_store( &cNode->next, _ctx->dummy );
    }
    // Now swap the stacks head pointer to what we want to keep
    PomStackLfNode *currHead;
    do{
        currHead = atomic_load( &_ctx->head );
    }while( !atomic_compare_exchange_weak( &_ctx->head, &currHead, toKeep ) );
    
    atomic_store( &_ctx->stackSize, newSize );
    // currHead now points to data we return. Should be null-terminated by design
    *_nodes = currHead;
    return 0;
}

// Push many nodes onto the stack (must be null terminated)
int pomStackLfPushMany( PomStackLfCtx *_ctx, PomStackLfNode * _nodes ){
    // Find the last node in the parameter list
    PomStackLfNode *lNode = _nodes;
    if( !lNode ){
        // No nodes in list
        return 1;
    }
    int nCount = 1;
    while( lNode->next ){
        lNode = lNode->next;
        nCount++;
    }
    PomStackLfNode *head;
    do{
        head = atomic_load( &_ctx->head );
        lNode->next = head;
    }while( atomic_compare_exchange_weak( &_ctx->head, &head, lNode) );

    atomic_fetch_add( &_ctx->stackSize, nCount );

    return 0;

}

// Pop all items off the stack (releasing memory ownership to caller)
PomStackLfNode * pomStackLfPopAll( PomStackLfCtx *_ctx ){
    PomStackLfNode *head;
    do{
        head = atomic_load( &_ctx->head );
    }while( !atomic_compare_exchange_weak( &_ctx->head, &head, _ctx->dummy ) );//NULL ) );
    // TODO - change this so it's actually thread-safe
    atomic_store( &_ctx->stackSize, 0 );

    // Find the last item (dummy) and remove it from the returned list
    PomStackLfNode *cNode = head;
    if( cNode == _ctx->dummy || !cNode ){
        return NULL;
    }
    while( cNode->next != _ctx->dummy ){
        cNode = cNode->next;
        if( !cNode ){
            // Some error has occurred
            return NULL;
        }
    }
    atomic_store( &cNode->next, NULL );
    return head;
}


*/