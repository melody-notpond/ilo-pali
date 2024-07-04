#include "scheduler.h"

#ifdef SCHED_ROUND_ROBIN

int next = 0;
int up_to = 0;

void init_scheduler(pid_t max_pid, void *data) {
    (void) max_pid;
    (void) data;
}

void schedule_task(pid_t pid, task_state_t state, int priority) {
    if (pid > up_to) {
        up_to = pid;
    }

    (void) pid;
    (void) state;
    (void) priority;
}

bool should_switch_now(pid_t pid, int priority) {
    (void) pid;
    (void) priority;
    return false;
}

pid_t next_scheduled_task() {
    int n = next;
    next++;
    if (next > up_to)
        next = 0;
    return n;
}

void unschedule_task(pid_t pid) {
    (void) pid;
}

#endif /* SCHED_ROUND_ROBIN */
