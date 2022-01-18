#include "iter/string.h"
#include "syscalls.h"
#include "format.h"

void printf_writer(str_t s, void* _) {
    uart_write((char*) s.bytes, s.len);
}

// uart_printf(char*, ...)
// Prints data to the uart.
void uart_printf(char* format, ...) {
    va_list va;
    va_start(va, format);
    vformat(NULL, printf_writer, format, va);
    va_end(va);
}

void* page_allocator_alloc(void* _, size_t size) {
    size_t* p = alloc_page(NULL, (size + PAGE_SIZE - 1) / PAGE_SIZE, PERM_READ | PERM_WRITE);
    *p = size;
    return p + 1;
}

void page_allocator_dealloc(void* _, void* p) {
    size_t* page = (size_t*) p;
    page--;
    dealloc_page(page, *page);
}

const alloc_t PAGE_ALLOC = {
    .data = NULL,
    .alloc = page_allocator_alloc,
    .alloc_resize = NULL,
    .dealloc = page_allocator_dealloc,
};

// debug_free_buckets_alloc(free_buckets_alloc_t*) -> void
// Prints out the used buckets.
void debug_free_buckets_alloc(free_buckets_alloc_t* free_buckets) {
    uart_printf("Starting debug\n");
    struct s_free_bucket const* bucket = free_buckets->used;
    while (bucket != NULL) {
        uart_printf("Unfreed allocation originating from 0x%x (%u bytes)\n", bucket->origin, bucket->size);
        bucket = bucket->next;
    }
    uart_printf("Ending debug\n");
}
