#include "string.h"

// strcmp(const char*, const char*) -> int
// Returns 0 if the strings are equal, 1 if the first string is greater than the second, and -1 otherwise.
int strcmp(const char* s1, const char* s2) {
    for (; *s1 && *s2; s1++, s2++) {
        if (*s1 < *s2)
            return -1;
        else if (*s1 > *s2)
            return 1;
    }

    if (*s1 < *s2)
        return -1;
    else if (*s1 > *s2)
        return 1;
    else
        return 0;
}

