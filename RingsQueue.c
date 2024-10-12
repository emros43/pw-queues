#include <malloc.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

#include "HazardPointer.h"
#include "RingsQueue.h"

struct RingsQueueNode;
typedef struct RingsQueueNode RingsQueueNode;

/*****************************************************************************/

struct RingsQueueNode {
    _Atomic(RingsQueueNode*) next;
    Value buffer[RING_SIZE];
    _Atomic(size_t) push_idx;
    _Atomic(size_t) pop_idx;
};

RingsQueueNode* RingsQueueNode_new(void)
{
    RingsQueueNode* node = (RingsQueueNode*)malloc(sizeof(RingsQueueNode));
    atomic_init(&node->next, NULL);
    atomic_init(&node->push_idx, 0);
    atomic_init(&node->pop_idx, 0);
    for (int i = 0; i < RING_SIZE; i++) {
        node->buffer[i] = EMPTY_VALUE;
    }
    return node;
}

/*****************************************************************************/

struct RingsQueue {
    RingsQueueNode* head;
    RingsQueueNode* tail;
    pthread_mutex_t pop_mtx;
    pthread_mutex_t push_mtx;
};

RingsQueue* RingsQueue_new(void)
{
    RingsQueue* queue = (RingsQueue*)malloc(sizeof(RingsQueue));
    RingsQueueNode* dummy = RingsQueueNode_new();
    queue->head = dummy;
    queue->tail = dummy;
    pthread_mutex_init(&queue->pop_mtx, NULL);
    pthread_mutex_init(&queue->push_mtx, NULL);
    return queue;
}

void RingsQueue_delete(RingsQueue* queue)
{
    RingsQueueNode* current = queue->head;
    while (current != NULL) {
        RingsQueueNode* next = atomic_load(&current->next);
        free(current);
        current = next;
    }
    pthread_mutex_destroy(&queue->pop_mtx);
    pthread_mutex_destroy(&queue->push_mtx);
    free(queue);
}

/*****************************************************************************/

void RingsQueue_push(RingsQueue* queue, Value item)
{
    pthread_mutex_lock(&queue->push_mtx);
    RingsQueueNode* tail = queue->tail;
    size_t push_idx = atomic_load(&tail->push_idx);

    size_t next_push_idx = (push_idx + 1) % RING_SIZE;
    if (next_push_idx != atomic_load(&tail->pop_idx)) {
        tail->buffer[push_idx] = item;
        atomic_store(&tail->push_idx, next_push_idx);
    } else { // tail node is full - new node
        RingsQueueNode* new_node = RingsQueueNode_new();
        new_node->buffer[0] = item;
        atomic_store(&new_node->push_idx, 1);
        atomic_store(&tail->next, new_node);
        queue->tail = new_node;
    }
    pthread_mutex_unlock(&queue->push_mtx);
}

Value RingsQueue_pop(RingsQueue* queue)
{
    pthread_mutex_lock(&queue->pop_mtx);
    RingsQueueNode* head = queue->head;
    size_t pop_idx = atomic_load(&head->pop_idx);

    if (pop_idx != atomic_load(&head->push_idx)) {
        Value item = head->buffer[pop_idx];
        head->buffer[pop_idx] = TAKEN_VALUE;

        size_t next_pop_idx = (pop_idx + 1) % RING_SIZE;
        atomic_store(&head->pop_idx, next_pop_idx);

        pthread_mutex_unlock(&queue->pop_mtx);
        return item;
    } else { // head node is empty, check for next node
        RingsQueueNode* next = atomic_load(&head->next);
        if (next != NULL) {
            queue->head = next;
            free(head);
            pthread_mutex_unlock(&queue->pop_mtx);
            return RingsQueue_pop(queue); // try popping from the new head node
        }
    }

    pthread_mutex_unlock(&queue->pop_mtx);
    return EMPTY_VALUE;
}

bool RingsQueue_is_empty(RingsQueue* queue)
{
    pthread_mutex_lock(&queue->pop_mtx);
    RingsQueueNode* head = queue->head;
    bool empty = (atomic_load(&head->pop_idx) ==
            atomic_load(&head->push_idx) && atomic_load(&head->next) == NULL);
    pthread_mutex_unlock(&queue->pop_mtx);
    return empty;
}
