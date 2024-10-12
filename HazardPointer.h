#pragma once

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_THREADS 128
static const int RETIRED_THRESHOLD = MAX_THREADS;

struct RetiredNode { // not atomic - retired_list[i] is accessed only by thread i
    void* ptr;
    struct RetiredNode* next;
};

struct HazardPointer {
    _Atomic(void*) reservation[MAX_THREADS]; // current reservation for every thread
    struct RetiredNode* retired_list[MAX_THREADS]; // list of popped by every thread (to delete)
    _Atomic(int) retire_count[MAX_THREADS]; // length of retired_list < RETIRED_THRESHOLD
};

typedef struct HazardPointer HazardPointer;

void HazardPointer_register(int thread_id, int num_threads);
void HazardPointer_initialize(HazardPointer* hp);
void HazardPointer_finalize(HazardPointer* hp);
void* HazardPointer_protect(HazardPointer* hp, const _Atomic(void*)* atom);
void HazardPointer_clear(HazardPointer* hp);
void HazardPointer_retire(HazardPointer* hp, void* ptr);