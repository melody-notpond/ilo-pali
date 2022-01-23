#include "core/prelude.h"
#include <stdatomic.h>
#include "sync.h"
#include "syscalls.h"

struct s_mutex {
    atomic_uint_fast8_t lock;
    alloc_t* allocator;
    void* data;
};

// create_mutex(alloc_t*, void*) -> mutex_t*
// Creates a mutex for the given data.
mutex_t* create_mutex(alloc_t* allocator, void* data) {
    mutex_t* mutex = alloc(allocator, sizeof(mutex_t));
    *mutex = (mutex_t) {
        .lock = 0,
        .allocator = allocator,
        .data = data,
    };
    return mutex;
}

// mutex_lock(mutex_t*) -> mutex_guard_t
// Waits until the mutex is able to be locked.
mutex_guard_t mutex_lock(mutex_t* mutex) {
    while (true) {
        for (int i = 0; i < 64; i++) {
            uint_fast8_t expected = 0;
            if (atomic_compare_exchange_weak(&mutex->lock, &expected, 1)) {
                return (mutex_guard_t) {
                    .mutex = mutex,
                    .data = mutex->data,
                };
            } else {
                // TODO: pause opcode
            }
        }

        lock(&mutex->lock, LOCK_WAKE | LOCK_U8, 0);
    }
}

// mutex_unlock(mutex_guard_t*) -> void
// Unlocks a mutex.
void mutex_unlock(mutex_guard_t* guard) {
    guard->mutex->lock = 0;
    guard->mutex = NULL;
    guard->data = NULL;
}

// clean_mutex(mutex_t*) -> void*
// Cleans up a mutex, returning the associated data.
void* clean_mutex(mutex_t* mutex) {
    void* data = mutex->data;
    dealloc(mutex->allocator, mutex);
    return data;
}
