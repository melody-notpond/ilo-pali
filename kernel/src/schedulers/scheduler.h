#ifndef SCHEDULE_H
#define SCHEDULE_H

#include "../process.h"

#define SCHED_ROUND_ROBIN

// Initialise the scheduler.
void init_scheduler(pid_t max_pid, void *data);

// Schedules a new process. Higher priority value is higher priority.
void schedule_task(pid_t pid, task_state_t state, int priority);

// Returns true if a different task should be scheduled right now,
// regardless of time quantum.
bool should_switch_now(pid_t pid, int priority);

// Returns the next scheduled task and removes the task from the
// scheduler.
pid_t next_scheduled_task();

// Remove a task from the scheduler without dequeueing it.
void unschedule_task(pid_t pid);

#endif /* SCHEDULE_H */
