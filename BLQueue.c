#include <malloc.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "BLQueue.h"
#include "HazardPointer.h"

struct BLNode;
typedef struct BLNode BLNode;
typedef _Atomic(BLNode*) AtomicBLNodePtr;

/*****************************************************************************/

struct BLNode {
    AtomicBLNodePtr next;
    _Atomic(Value) buffer[BUFFER_SIZE];
    _Atomic(size_t) push_idx;
    _Atomic(size_t) pop_idx;
};

BLNode* BLNode_new(void)
{
    BLNode* node = (BLNode*)malloc(sizeof(BLNode));
    atomic_init(&node->next, NULL);
    atomic_init(&node->push_idx, 0);
    atomic_init(&node->pop_idx, 0);
    for (int i = 0; i < BUFFER_SIZE; i++) {
        atomic_init(&node->buffer[i], EMPTY_VALUE);
    }
    return node;
}

/*****************************************************************************/

struct BLQueue {
    AtomicBLNodePtr head;
    AtomicBLNodePtr tail;
    HazardPointer hp;
};

BLQueue* BLQueue_new(void)
{
    BLQueue* queue = (BLQueue*)malloc(sizeof(BLQueue));
    BLNode* dummy = BLNode_new();
    atomic_init(&queue->head, dummy);
    atomic_init(&queue->tail, dummy);
    HazardPointer_initialize(&queue->hp);
    return queue;
}

void BLQueue_delete(BLQueue* queue)
{
    BLNode* node;
    while ((node = atomic_load(&queue->head)) != NULL) {
        BLNode* next = atomic_load(&node->next);
        free(node);
        atomic_store(&queue->head, next);
    }
    HazardPointer_finalize(&queue->hp);
    free(queue);
}

/*****************************************************************************/

void BLQueue_push(BLQueue* queue, Value item)
{
    while (true) {
        BLNode* tail = (BLNode*)HazardPointer_protect(&queue->hp, &queue->tail);
        _Atomic(size_t) idx = atomic_fetch_add(&tail->push_idx, 1);

        if (idx < BUFFER_SIZE) {
            Value expected = EMPTY_VALUE;
            if (atomic_compare_exchange_strong(&tail->buffer[idx], &expected, item)) {
                break;  // success, was inserted
            }
            // otherwise another thread already changed idx, try again
        } else {
            BLNode* new_node = BLNode_new();
            atomic_store(&new_node->buffer[0], item);
            atomic_store(&new_node->push_idx, 1);

            BLNode* expected_tail = tail;
            if (atomic_compare_exchange_strong(&queue->tail, &expected_tail, new_node)) {
                atomic_store(&tail->next, new_node);
                break; // just attached a new tail
            } else {
                free(new_node); // another thread already made a new empty tail, try again
            }
        }
    }
    HazardPointer_clear(&queue->hp);
}

Value BLQueue_pop(BLQueue* queue)
{
    Value result = EMPTY_VALUE;
    while (true) {
        BLNode* head = (BLNode*)HazardPointer_protect(&queue->hp, &queue->head);
        _Atomic(size_t) idx = atomic_fetch_add(&head->pop_idx, 1);

        if (idx < BUFFER_SIZE) {
            result = atomic_exchange(&head->buffer[idx], TAKEN_VALUE);
            break; // returns previous value before the change to TAKEN
        } else {
            BLNode* next = atomic_load(&head->next);
            if (next == NULL) {
                result = EMPTY_VALUE;
                break;
            }

            BLNode* expected_head = head;
            if (atomic_compare_exchange_strong(&queue->head, &expected_head, next)) {
                HazardPointer_retire(&queue->hp, head);
            }
        }
    }
    HazardPointer_clear(&queue->hp);
    return result;
}

bool BLQueue_is_empty(BLQueue* queue)
{
    BLNode* head = (BLNode*)HazardPointer_protect(&queue->hp, &queue->head);
    size_t pop_idx = atomic_load(&head->pop_idx);
    size_t push_idx = atomic_load(&head->push_idx);
    bool result = ((pop_idx == push_idx) && (atomic_load(&head->next) == NULL));
    HazardPointer_clear(&queue->hp);
    return result;
}
