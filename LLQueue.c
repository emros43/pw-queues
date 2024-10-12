#include <malloc.h>
#include <stdatomic.h>
#include <stdbool.h>

#include "HazardPointer.h"
#include "LLQueue.h"

struct LLNode;
typedef struct LLNode LLNode;
typedef _Atomic(LLNode*) AtomicLLNodePtr;

/*****************************************************************************/

struct LLNode {
    AtomicLLNodePtr next;
    Value value;
};

LLNode* LLNode_new(Value item)
{
    LLNode* node = (LLNode*)malloc(sizeof(LLNode));
    atomic_init(&node->next, NULL);
    node->value = item;
    return node;
}

/*****************************************************************************/

struct LLQueue {
    AtomicLLNodePtr head;
    AtomicLLNodePtr tail;
    HazardPointer hp;
};

LLQueue* LLQueue_new(void)
{
    LLQueue* queue = (LLQueue*)malloc(sizeof(LLQueue));
    LLNode* dummy = LLNode_new(EMPTY_VALUE);
    atomic_init(&queue->head, dummy);
    atomic_init(&queue->tail, dummy);
    HazardPointer_initialize(&queue->hp);
    return queue;
}

void LLQueue_delete(LLQueue* queue)
{
    LLNode* node;
    while ((node = atomic_load(&queue->head)) != NULL) {
        LLNode* next = atomic_load(&node->next);
        free(node);
        atomic_store(&queue->head, next);
    }
    HazardPointer_finalize(&queue->hp);
    free(queue);
}

/*****************************************************************************/

void LLQueue_push(LLQueue* queue, Value item)
{
    LLNode* new_node = LLNode_new(item);
    LLNode* tail;
    while (true) {
        tail = (LLNode*)HazardPointer_protect(&queue->hp, &queue->tail);
        LLNode* next = atomic_load(&tail->next);
        if (tail == atomic_load(&queue->tail)) {
            if (next == NULL) {
                if (atomic_compare_exchange_strong(&tail->next, &next, new_node)) {
                    break;
                }
            } else {
                atomic_compare_exchange_strong(&queue->tail, &tail, next);
            }
        }
    }
    atomic_compare_exchange_strong(&queue->tail, &tail, new_node);
    HazardPointer_clear(&queue->hp);
}

Value LLQueue_pop(LLQueue* queue)
{
    Value value;
    LLNode* head;
    while (true) {
        head = (LLNode*)HazardPointer_protect(&queue->hp, &queue->head);
        LLNode* tail = atomic_load(&queue->tail);
        LLNode* next = atomic_load(&head->next);
        if (head == atomic_load(&queue->head)) {
            if (head == tail) {
                if (next == NULL) {
                    HazardPointer_clear(&queue->hp);
                    return EMPTY_VALUE;
                }
                atomic_compare_exchange_strong(&queue->tail, &tail, next);
            } else {
                value = next->value;
                if (atomic_compare_exchange_strong(&queue->head, &head, next)) {
                    break;
                }
            }
        }
    }
    HazardPointer_retire(&queue->hp, head);
    HazardPointer_clear(&queue->hp);
    return value;
}

bool LLQueue_is_empty(LLQueue* queue)
{
    LLNode* head = (LLNode*)HazardPointer_protect(&queue->hp, &queue->head);
    LLNode* next = atomic_load(&head->next);
    bool isEmpty = (next == NULL);
    HazardPointer_clear(&queue->hp);
    return isEmpty;
}
