#include <malloc.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>

#include "HazardPointer.h"

thread_local int _thread_id = -1;
int _num_threads = -1;

void HazardPointer_register(int thread_id, int num_threads)
{
    _thread_id = thread_id;
    _num_threads = num_threads;
}

/*****************************************************************************/

void HazardPointer_initialize(HazardPointer* hp)
{
    for (int i = 0; i < MAX_THREADS; i++) {
        atomic_store(&hp->reservation[i], NULL);
        hp->retired_list[i] = NULL;
        atomic_store(&hp->retire_count[i], 0);
    }
}

void HazardPointer_finalize(HazardPointer* hp)
{
    for (int i = 0; i < MAX_THREADS; i++) {
        struct RetiredNode* current = hp->retired_list[i];
        while (current != NULL) {
            struct RetiredNode* temp = current;
            current = current->next;
            free(temp->ptr);
            free(temp);
        }
        hp->retired_list[i] = NULL;
        atomic_store(&hp->retire_count[i], 0);
    }
}

/*****************************************************************************/

void* HazardPointer_protect(HazardPointer* hp, const _Atomic(void*)* atom)
{
    void* ptr = NULL;
    do {
        ptr = atomic_load(atom);
        atomic_store(&hp->reservation[_thread_id], ptr);
    } while (ptr != atomic_load(atom)); // between load and store atom
    // might have been changed by another thread
    return ptr;
}

void HazardPointer_clear(HazardPointer* hp)
{
    atomic_store(&hp->reservation[_thread_id], NULL);
}

/*****************************************************************************/

void HazardPointer_retire(HazardPointer* hp, void* ptr)
{
    int tid = _thread_id;

    // add ptr to the beginning of retired nodes for tid
    struct RetiredNode* node = (struct RetiredNode*)malloc(sizeof(struct RetiredNode));
    node->ptr = ptr;
    node->next = hp->retired_list[tid];
    hp->retired_list[tid] = node;
    atomic_fetch_add(&hp->retire_count[tid], 1);

    // now delete unused nodes (if there are too many of them)
    if (atomic_load(&hp->retire_count[tid]) >= RETIRED_THRESHOLD) {
        struct RetiredNode* current = hp->retired_list[tid];
        struct RetiredNode** prev_next = &hp->retired_list[tid]; // for deleting from list
        while (current != NULL) {
            // check if current is reserved
            bool is_protected = false;
            for (int i = 0; i < MAX_THREADS; i++) {
                if (atomic_load(&hp->reservation[i]) == current->ptr) {
                    is_protected = true;
                    break;
                }
            }

            if (!is_protected) { // then it can be safely freed
                *prev_next = current->next;
                struct RetiredNode* temp = current;
                current = current->next;
                free(temp->ptr);
                free(temp);
                atomic_fetch_sub(&hp->retire_count[tid], 1);
            } else { // just check next node
                prev_next = &current->next;
                current = current->next;
            }
        }
    }
}
