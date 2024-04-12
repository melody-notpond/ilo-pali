#include <stdbool.h>

#include "sync.h"

void spin_lock(spin_t *lock) {
    bool expected = false;
    while (!atomic_compare_exchange_weak(lock, &expected, true))
        expected = false;
}

void spin_unlock(spin_t *lock) {
    atomic_exchange(lock, false);
}
