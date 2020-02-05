#ifndef LINKEDLIST_H
#define LINKEDLIST_H

#include <stddef.h>
#include <stdint.h>

// TODO - implement this module in a nicer way (cache friendliness, speed, data management etc)

typedef struct PomLinkedListNode PomLinkedListNode;
typedef struct PomLinkedListCtx PomLinkedListCtx;
typedef uintptr_t PllKeyType;

struct PomLinkedListNode{
    PllKeyType key;
    PomLinkedListNode *next;
    PomLinkedListNode *prev;
};

struct PomLinkedListCtx{
    PomLinkedListNode *head;
    PomLinkedListNode *tail;
    size_t size;
};

int pomLinkedListInit( PomLinkedListCtx *_ctx );

int pomLinkedListClear( PomLinkedListCtx *_ctx );

int pomLinkedListAdd( PomLinkedListCtx *_ctx, PllKeyType key );

PomLinkedListNode * pomLinkedListFind( PomLinkedListCtx *_ctx, PllKeyType _key );

#endif //LINKEDLIST_H