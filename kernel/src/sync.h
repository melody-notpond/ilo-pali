#ifndef SYNC_H
#define SYNC_H

#include <stdatomic.h>

typedef volatile atomic_bool spin_t;

#define DEFINE_SPINLOCK(name) \
    static spin_t name = false

void spin_lock(spin_t *lock);

void spin_unlock(spin_t *lock);

#endif /* SYNC_H */
