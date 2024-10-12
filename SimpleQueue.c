#include <malloc.h>
#include <pthread.h>

#include "HazardPointer.h"
#include "SimpleQueue.h"

struct SimpleQueueNode;
typedef struct SimpleQueueNode SimpleQueueNode;

/*****************************************************************************/

struct SimpleQueueNode {
    _Atomic(SimpleQueueNode*) next;
    Value item;
};

SimpleQueueNode* SimpleQueueNode_new(Value item)
{
    SimpleQueueNode* node = (SimpleQueueNode*)malloc(sizeof(SimpleQueueNode));
    atomic_init(&node->next, NULL);
    node->item = item;
    return node;
}

/*****************************************************************************/

// head is the last element that was popped (and no longer is in the queue),
// head->next is the real beginning
struct SimpleQueue {
    SimpleQueueNode* head;
    SimpleQueueNode* tail;
    pthread_mutex_t head_mtx;
    pthread_mutex_t tail_mtx;
};

SimpleQueue* SimpleQueue_new(void)
{
    SimpleQueue* queue = (SimpleQueue*)malloc(sizeof(SimpleQueue));
    SimpleQueueNode* dummy = SimpleQueueNode_new(EMPTY_VALUE);
    queue->head = dummy;
    queue->tail = dummy;
    pthread_mutex_init(&queue->head_mtx, NULL);
    pthread_mutex_init(&queue->tail_mtx, NULL);
    return queue;
}

void SimpleQueue_delete(SimpleQueue* queue)
{
    while (!SimpleQueue_is_empty(queue)) {
        SimpleQueue_pop(queue);
    }
    free(queue->head); // remove dummy head
    pthread_mutex_destroy(&queue->head_mtx);
    pthread_mutex_destroy(&queue->tail_mtx);
    free(queue);
}

/*****************************************************************************/

void SimpleQueue_push(SimpleQueue* queue, Value item)
{
    SimpleQueueNode* node = SimpleQueueNode_new(item);
    pthread_mutex_lock(&queue->tail_mtx);
    atomic_store(&queue->tail->next, node);
    queue->tail = node;
    pthread_mutex_unlock(&queue->tail_mtx);
}

Value SimpleQueue_pop(SimpleQueue* queue)
{
    pthread_mutex_lock(&queue->head_mtx);
    SimpleQueueNode* old_head = queue->head;
    SimpleQueueNode* new_head = atomic_load(&old_head->next);
    if (new_head == NULL) {
        pthread_mutex_unlock(&queue->head_mtx);
        return EMPTY_VALUE;
    }
    queue->head = new_head;
    Value item = new_head->item;
    new_head->item = EMPTY_VALUE;
    pthread_mutex_unlock(&queue->head_mtx);
    free(old_head);
    return item;
}

bool SimpleQueue_is_empty(SimpleQueue* queue)
{
    pthread_mutex_lock(&(queue->head_mtx));
    bool is_empty = (atomic_load(&queue->head->next) == NULL);
    pthread_mutex_unlock(&(queue->head_mtx));
    return is_empty;
}
