#ifndef SYNC_H
#define SYNC_H

#include "alloc.h"

typedef struct s_mutex mutex_t;

typedef struct {
    mutex_t* mutex;
    void* data;
} mutex_guard_t;

// create_mutex(alloc_t*, void*) -> mutex_t*
// Creates a mutex for the given data.
mutex_t* create_mutex(alloc_t* allocator, void* data);

// mutex_lock(mutex_t*) -> mutex_guard_t
// Waits until the mutex is able to be locked.
mutex_guard_t mutex_lock(mutex_t* mutex);

// mutex_unlock(mutex_guard_t*) -> void
// Unlocks a mutex.
void mutex_unlock(mutex_guard_t* guard);

// clean_mutex(mutex_t*) -> void*
// Cleans up a mutex, returning the associated data.
void* clean_mutex(mutex_t* mutex);

#endif /* SYNC_H */
