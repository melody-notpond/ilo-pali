#ifndef QUEUE_H
#define QUEUE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct s_queue *queue_t;

// create_queue(size_t) -> queue_t*
// Creates a new queue.
queue_t* create_queue(size_t item_size);

// queue_len(queue_t*) -> size_t
// Returns the length of the queue.
size_t queue_len(queue_t* queue);

// queue_enqueue(queue_t*, void*) -> void
// Enqueues data onto a queue.
void queue_enqueue(queue_t* queue, void* data);

// queue_dequeue(queue_t*, void*) -> bool
// Dequeues data from the queue. Returns true if there was something in the queue.
bool queue_dequeue(queue_t* queue, void* data);

// TODO: cleanup

#endif /* QUEUE_H */
