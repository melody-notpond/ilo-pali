// #include <stdatomic.h>
// #include <stdbool.h>

// #include "memory.h"

// // memcpy(void*, const void*, unsigned long int) -> void*
// // Copys the data from one pointer to another.
// void* memcpy(void* dest, const void* src, unsigned long int n) {
//     unsigned char* d1 = dest;
//     const unsigned char* s1 = src;
//     unsigned char* end = dest + n;

//     for (; d1 < end; d1++, s1++) {
//         *d1 = *s1;
//     }

//     return dest;
// }

// // memset(void*, int, unsigned long int) -> void*
// // Sets a value over a space. Returns the original pointer.
// void* memset(void* p, int i, unsigned long int n) {
//     unsigned char c = i;
//     for (unsigned char* p1 = p; (void*) p1 < p + n; p1++) {
//         *p1 = c;
//     }
//     return p;
// }

// // memeq(void*, void*, size_t) -> bool
// // Returns true if the two pointers have identical data.
// bool memeq(void* p, void* q, size_t size) {
//     uint8_t* p1 = p;
//     uint8_t* q1 = q;

//     for (size_t i = 0; i < size; i++, p1++, q1++) {
//         if (*p1 != *q1)
//             return false;
//     }

//     return true;
// }
