#include <limits.h>

#include "hashmap.h"
#include "memory.h"

const uint32_t ROTATE = 5;
const uint64_t SEED64 = 0x517cc1b727220a95;

// from https://stackoverflow.com/a/776523
static inline uint64_t rotl64 (uint64_t n, unsigned int c) {
    const unsigned int mask = (CHAR_BIT*sizeof(n) - 1);
    c &= mask;
    return (n<<c) | (n>>( (-c)&mask ));
}

static inline uint64_t hash_word64(uint64_t a, uint64_t b) {
    return (rotl64(a, ROTATE) ^ b) * SEED64;
}

// hash implementation based on https://github.com/cbreeden/fxhash/blob/master/lib.rs
static inline uint64_t hash_write(uint64_t hash, uint8_t* bytes, size_t len) {
    while (len >= 8) {
        hash = hash_word64(hash, *(uint64_t*) bytes);
        bytes += 8;
        len -= 8;
    }

    if (len >= 4) {
        hash = hash_word64(hash, *(uint32_t*) bytes);
        bytes += 4;
        len -= 4;
    }

    if (len >= 2) {
        hash = hash_word64(hash, *(uint16_t*) bytes);
        bytes += 2;
        len -= 2;
    }

    if (len >= 1) {
        hash = hash_word64(hash, *bytes);
        bytes += 1;
        len -= 1;
    }

    return hash;
}

struct s_hashmap_bucket {
    void* keys;
    void* values;
    size_t len;
};

struct s_hashmap {
    size_t cap;
    size_t key_size;
    size_t val_size;
    struct s_hashmap_bucket ring[];
};

#define INITIAL_HASHMAP_SIZE            16
#define HASHMAP_MAX_COLLISION_COUNT     5

// create_hashmap(size_t, size_t) -> hashmap_t*
// Creates an empty hashmap.
hashmap_t* create_hashmap(size_t key_size, size_t val_size) {
    hashmap_t* hashmap = malloc(sizeof(hashmap_t));
    *hashmap = malloc(sizeof(struct s_hashmap) + INITIAL_HASHMAP_SIZE * sizeof(struct s_hashmap_bucket));
    **hashmap = (struct s_hashmap) {
        .cap = INITIAL_HASHMAP_SIZE,
        .key_size = key_size,
        .val_size = val_size,
    };

    for (size_t i = 0; i < INITIAL_HASHMAP_SIZE; i++) {
        (*hashmap)->ring[i] = (struct s_hashmap_bucket) {
            .keys = NULL,
            .values = NULL,
            .len = 0,
        };
    }

    return hashmap;
}

void hashmap_grow(hashmap_t* hashmap) {
    size_t key_size = (*hashmap)->key_size;
    size_t val_size = (*hashmap)->val_size;
    hashmap_t new = malloc(sizeof(struct s_hashmap) + sizeof(struct s_hashmap_bucket) * (*hashmap)->cap * 2);
    *new = (struct s_hashmap) {
        .cap = (*hashmap)->cap,
        .key_size = key_size,
        .val_size = val_size,
    };

    for (size_t i = 0; i < new->cap; i++) {
        new->ring[i] = (struct s_hashmap_bucket) {
            .keys = NULL,
            .values = NULL,
            .len = 0,
        };
    }

    for (size_t i = 0; i < (*hashmap)->cap; i++) {
        struct s_hashmap_bucket* bucket = &(*hashmap)->ring[i];
        for (size_t j = 0; j < bucket->len; j++) {
            uint64_t hash = hash_write(0, bucket->keys + j * key_size, key_size);
            size_t index = hash & (new->cap - 1);

            struct s_hashmap_bucket* new_bucket = &new->ring[index];
            if (new_bucket->len == 0) {
                new_bucket->keys = malloc(key_size * HASHMAP_MAX_COLLISION_COUNT);
                new_bucket->values = malloc(val_size * HASHMAP_MAX_COLLISION_COUNT);
            }

            memcpy(new_bucket->keys + new_bucket->len * key_size, bucket->keys + j * key_size, key_size);
            memcpy(new_bucket->values + new_bucket->len * val_size, bucket->values + j * key_size, val_size);
            new_bucket->len++;
        }
    }

    for (size_t i = 0; i < (*hashmap)->cap; i++) {
        free((*hashmap)->ring[i].keys);
        free((*hashmap)->ring[i].values);
    }

    free(*hashmap);
    *hashmap = new;
}

// hashmap_insert(hashmap_t*, void*, void*) -> void
// Inserts a key value pair into a hashmap.
void hashmap_insert(hashmap_t* hashmap, void* key, void* value) {
    size_t key_size = (*hashmap)->key_size;
    size_t val_size = (*hashmap)->val_size;
    uint64_t hash = hash_write(0, key, key_size);

    while (true) {
        size_t index = hash & ((*hashmap)->cap - 1);
        struct s_hashmap_bucket* bucket = &(*hashmap)->ring[index];

        if (bucket->len < HASHMAP_MAX_COLLISION_COUNT) {
            if (bucket->len == 0) {
                bucket->keys = malloc(key_size * HASHMAP_MAX_COLLISION_COUNT);
                bucket->values = malloc(val_size * HASHMAP_MAX_COLLISION_COUNT);
            }

            memcpy(bucket->keys + bucket->len * key_size, key, key_size);
            memcpy(bucket->values + bucket->len * val_size, value, val_size);
            bucket->len++;
            return;
        } else {
            hashmap_grow(hashmap);
        }
    }
}

// hashmap_empty(hashmap_t* hashmap) -> bool
// Returns true if the hashmap is empty.
bool hashmap_empty(hashmap_t* hashmap) {
    for (size_t i = 0; i < (*hashmap)->cap; i++) {
        if ((*hashmap)->ring[i].len != 0)
            return false;
    }

    return true;
}

// hashmap_get(hashmap_t*, void*) -> void*
// Gets the value associated with the given key. Returns NULL on failure.
void* hashmap_get(hashmap_t* hashmap, void* key) {
    size_t key_size = (*hashmap)->key_size;
    size_t val_size = (*hashmap)->val_size;
    uint64_t hash = hash_write(0, key, key_size);
    size_t index = hash & ((*hashmap)->cap - 1);
    struct s_hashmap_bucket* bucket = &(*hashmap)->ring[index];

    for (size_t i = 0; i < bucket->len; i++) {
        if (memeq(bucket->keys + i * key_size, key, key_size)) {
            return bucket->values + i * val_size;
        }
    }

    return NULL;
}

// hashmap_remove(hashmap_t*, void*) -> void
// Removes a key value pair from a hashmap.
void hashmap_remove(hashmap_t* hashmap, void* key) {
    size_t key_size = (*hashmap)->key_size;
    size_t val_size = (*hashmap)->val_size;
    uint64_t hash = hash_write(0, key, key_size);
    size_t index = hash & ((*hashmap)->cap - 1);
    struct s_hashmap_bucket* bucket = &(*hashmap)->ring[index];

    for (size_t i = 0; i < bucket->len; i++) {
        if (memeq(bucket->keys + i * key_size, key, key_size)) {
            for (size_t j = i; j < bucket->len; j++) {
                memcpy(bucket->keys + j * key_size, bucket->keys + (j + 1) * key_size, key_size);
                memcpy(bucket->values + j * val_size, bucket->values + (j + 1) * val_size, val_size);
            }

            bucket->len--;
            break;
        }
    }

    if (bucket->len == 0) {
        free(bucket->keys);
        bucket->keys = NULL;
        free(bucket->values);
        bucket->values = NULL;
    }
}

// hashmap_find(hashmap_t*, void*, fn(void*, void*) -> bool, size_t*, size_t*) -> void*
// Finds a hashmap value that matches the given function. Returns NULL on failure.
void* hashmap_find(hashmap_t* hashmap, void* data, bool (*fn)(void*, void*, void*), size_t* ip, size_t* jp) {
    size_t key_size = (*hashmap)->key_size;
    size_t val_size = (*hashmap)->val_size;

    for (size_t i = ip ? *ip : 0; i < (*hashmap)->cap; i++) {
        struct s_hashmap_bucket* bucket = &(*hashmap)->ring[i];
        for (size_t j = jp ? *jp : 0; j < bucket->len; j++) {
            if (fn(data, bucket->keys + j * key_size, bucket->values + j * val_size)) {
                *ip = i;
                *jp = j;
                return bucket->values + j * val_size;
            }
        }
    }

    return NULL;
}
