#ifndef CDBG_THREADS_H
#define CDBG_THREADS_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include <mach/mach.h>

#define CDBG_MAX_THREADS 256

typedef struct {
    uint64_t tid;   /* stable THREAD_IDENTIFIER_INFO.thread_id */
    uintptr_t pc;   /* snapshot at enumeration time, for display */
} cdbg_thread_entry_t;

/* Enumerates all threads of the traced process, ordered as task_threads()
 * returns them (index 0 is the "primary" thread). */
int cdbg_threads_list(pid_t pid, cdbg_thread_entry_t *out, size_t max,
                      size_t *count_out);

/* Resolves `tid` to a send right for that thread. tid == 0 means "the
 * primary thread" (task_threads()[0]), preserving the single-thread
 * default used throughout the debugger before a thread is explicitly
 * selected. Caller must mach_port_deallocate(mach_task_self(), *port_out). */
int cdbg_threads_resolve_port(pid_t pid, uint64_t tid, thread_act_t *port_out);

int cdbg_threads_suspend_others(pid_t pid, uint64_t keep_tid);
int cdbg_threads_resume_others(pid_t pid, uint64_t keep_tid);

#endif /* CDBG_THREADS_H */
