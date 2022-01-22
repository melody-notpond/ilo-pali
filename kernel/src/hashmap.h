#ifndef HASHMAP_H
#define HASHMAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct s_hashmap *hashmap_t;

// create_hashmap(size_t, size_t) -> hashmap_t*
// Creates an empty hashmap.
hashmap_t* create_hashmap(size_t key_size, size_t val_size);

// hashmap_insert(hashmap_t*, void*, void*) -> void
// Inserts a key value pair into a hashmap.
void hashmap_insert(hashmap_t* hashmap, void* key, void* value);

// hashmap_empty(hashmap_t* hashmap) -> bool
// Returns true if the hashmap is empty.
bool hashmap_empty(hashmap_t* hashmap);

// hashmap_get(hashmap_t*, void*) -> void*
// Gets the value associated with the given key. Returns NULL on failure.
void* hashmap_get(hashmap_t* hashmap, void* key);

// hashmap_remove(hashmap_t*, void*) -> void
// Removes a key value pair from a hashmap.
void hashmap_remove(hashmap_t* hashmap, void* key);

// hashmap_find(hashmap_t*, void*, fn(void*, void*) -> bool) -> void*
// Finds a hashmap value that matches the given function. Returns NULL on failure.
void* hashmap_find(hashmap_t* hashmap, void* data, bool (*fn)(void*, void*, void*));

// TODO: hashmap_clean

#endif /* HASHMAP_H */
