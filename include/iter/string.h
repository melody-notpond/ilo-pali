#ifndef STRING_H
#define STRING_H

#include "alloc.h"
#include "core/prelude.h"

// Represents an allocated, mutable string on the heap.
typedef struct s_string string_t;

// S(char*) -> str_t
// Converts a null terminated char* into a str_t.
#define S(s) ({ size_t len = 0; for (char* p = s; *p; p++, len++); (str_t) { .len = len, .bytes = s, }; })

// Represents a reference to some string data.
typedef struct {
    size_t len;
    char const* bytes;
} str_t;

static const str_t STREMPTY = {
    .len = 0,
    .bytes = NULL,
};

// empty_string(alloc_t*) -> string_t
// Allocates an empty string.
string_t empty_string(alloc_t* A);

// string_from_str(alloc_t*, str_t) -> string_t
// Creates a string from a str.
string_t string_from_str(alloc_t* A, str_t str);

// str_from_string(string_t*) -> str_t
// Creates a str from a string.
str_t str_from_string(string_t* s);

// str_slice(str_t, size_t, size_t) -> str_t
// Creates a str slice from another str.
str_t str_slice(str_t s, size_t start, size_t end);

// string_push_byte(string_t*, char) -> void
// Pushes a byte to the given string.
void string_push_byte(string_t* s, char c);

// string_push_str(string_t*, str_t) -> void
// Pushes a str to the given string.
void string_push_str(string_t* s, str_t str);

// str_equals(str_t, str_t) -> bool
// Determines whether two strs are equal.
bool str_equals(str_t a, str_t b);

// str_is_empty(str_t) -> bool
// Returns true if the given str is empty.
bool str_is_empty(str_t s);

// str_split(str_t, str_t, str_t) -> str_t
// Splits the first argument based on the second argument, starting off on the last argument's offset.
str_t str_split(str_t s, str_t spliter, str_t last);

// dealloc_string(string_t*) -> void
// Deallocates a string.
void dealloc_string(string_t* s);

#endif /* STRING_H */
