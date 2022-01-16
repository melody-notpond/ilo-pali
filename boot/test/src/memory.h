#ifndef MEMORY_H
#define MEMORY_H

#define PAGE_SIZE 4096

// memcpy(void*, const void*, unsigned long int) -> void*
// Copys the data from one pointer to another.
void* memcpy(void* dest, const void* src, unsigned long int n);

// memset(void*, int, unsigned long int) -> void*
// Sets a value over a space. Returns the original pointer.
void* memset(void* p, int i, unsigned long int n);

#endif /* MEMORY_H */
