/*
Simple non-contiguous stack implementation.
Node memory is handled by module (except for popAll).
User data memory is handled by user (i.e. it is not copied)
Not thread safe
*/

typedef struct PomStackNode PomStackNode;
typedef struct PomStackCtx PomStackCtx;

struct PomStackNode{
    void *data;
    PomStackNode *next;
};

struct PomStackCtx{
    PomStackNode *head;
};

// Initialise the stack
int pomStackInit( PomStackCtx *_ctx );

// Clear the stack and free memory
int pomStackClear( PomStackCtx *_ctx );

// Pop all items off the stack (releasing memory ownership to caller)
PomStackNode * pomStackPopAll( PomStackCtx *_ctx );

// Pop a single item off the stack
void * pomStackPop( PomStackCtx *_ctx );

// Push a single item onto the stack
int pomStackPush( PomStackCtx *_ctx, void * _data );

// Push many nodes onto the stack (must be null terminated)
int pomStackPushMany( PomStackCtx *_ctx, PomStackNode * _nodes );