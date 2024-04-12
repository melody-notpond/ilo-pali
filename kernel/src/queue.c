#include "memory.h"
#include "queue.h"

struct s_queue {
    size_t item_size;
    size_t len;
    size_t cap;
    size_t start;
    size_t end;
    size_t padding;
    uint8_t bytes[];
};

#define QUEUE_INIT_SIZE 128

// create_queue(size_t) -> queue_t*
// Creates a new queue.
queue_t* create_queue(size_t item_size) {
    queue_t* queue = malloc(sizeof(queue_t));
    *queue = malloc(sizeof(struct s_queue) + item_size * QUEUE_INIT_SIZE);
    **queue = (struct s_queue) {
        .item_size = item_size,
        .len = 0,
        .cap = QUEUE_INIT_SIZE,
        .start = 0,
        .end = 0,
    };
    return queue;
}

void grow_queue(queue_t* queue) {
    (*queue)->cap <<= 1;
    *queue = realloc(*queue, sizeof(struct s_queue) + (*queue)->cap * (*queue)->item_size);
}

// queue_len(queue_t*) -> size_t
// Returns the length of the queue.
size_t queue_len(queue_t* queue) {
    return (*queue)->len;
}

// queue_enqueue(queue_t*, void*) -> void
// Enqueues data onto a queue.
void queue_enqueue(queue_t* queue, void* data) {
    size_t item_size = (*queue)->item_size;

    if ((*queue)->len >= (*queue)->cap)
        grow_queue(queue);

    memcpy((*queue)->bytes + (*queue)->end * item_size, data, item_size);
    (*queue)->end++;
    (*queue)->len++;
    if ((*queue)->end >= (*queue)->cap)
        (*queue)->end = 0;
}

// queue_dequeue(queue_t*, void*) -> bool
// Dequeues data from the queue. Returns true if there was something in the queue.
bool queue_dequeue(queue_t* queue, void* data) {
    size_t item_size = (*queue)->item_size;

    if ((*queue)->len == 0)
        return false;

    if (data != NULL)
        memcpy(data, (*queue)->bytes + (*queue)->start * item_size, item_size);
    (*queue)->start++;
    (*queue)->len--;
    if ((*queue)->start >= (*queue)->cap)
        (*queue)->start = 0;
    return true;
}
