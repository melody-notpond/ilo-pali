#ifndef STRING_H
#define STRING_H

#include <stddef.h>

// strcmp(const char*, const char*) -> int
// Returns 0 if the strings are equal, 1 if the first string is greater than the second, and -1 otherwise.
int strcmp(const char* s1, const char* s2);

// strlen(const char*) -> size_t
// Returns the length of the string.
size_t strlen(const char* s);

#endif /* _STRING_H */

