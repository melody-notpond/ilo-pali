#include "scheduler.h"

#ifdef SCHED_ROUND_ROBIN

void schedule_task(pid_t pid, task_state_t state, int priority) {
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
    return 0;
}

void unschedule_task(pid_t pid) {
    (void) pid;
}

#endif /* SCHED_ROUND_ROBIN */
