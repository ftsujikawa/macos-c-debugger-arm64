#include "threads.h"

#include <mach/arm/thread_status.h>
#include <stdio.h>

static mach_port_t task_for_traced_pid(pid_t pid)
{
    mach_port_t task = MACH_PORT_NULL;
    kern_return_t kr = task_for_pid(mach_task_self(), pid, &task);

    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "task_for_pid failed: %s (%d)\n", mach_error_string(kr), kr);
        return MACH_PORT_NULL;
    }

    return task;
}

static int thread_id_for_port(thread_act_t thread, uint64_t *tid_out)
{
    thread_identifier_info_data_t info;
    mach_msg_type_number_t info_count = THREAD_IDENTIFIER_INFO_COUNT;
    if (thread_info(thread, THREAD_IDENTIFIER_INFO, (thread_info_t)&info,
                    &info_count) != KERN_SUCCESS) {
        return -1;
    }
    *tid_out = info.thread_id;
    return 0;
}

static int enumerate_threads(pid_t pid, thread_act_array_t *threads_out,
                             mach_msg_type_number_t *count_out)
{
    mach_port_t task = task_for_traced_pid(pid);
    if (task == MACH_PORT_NULL) {
        return -1;
    }

    kern_return_t kr = task_threads(task, threads_out, count_out);
    mach_port_deallocate(mach_task_self(), task);

    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "task_threads failed: %s (%d)\n", mach_error_string(kr), kr);
        return -1;
    }
    return 0;
}

static void deallocate_threads(thread_act_array_t threads, mach_msg_type_number_t count)
{
    for (mach_msg_type_number_t i = 0; i < count; i++) {
        mach_port_deallocate(mach_task_self(), threads[i]);
    }
    vm_deallocate(mach_task_self(), (vm_address_t)threads,
                 count * sizeof(thread_act_t));
}

int cdbg_threads_list(pid_t pid, cdbg_thread_entry_t *out, size_t max, size_t *count_out)
{
    thread_act_array_t threads = NULL;
    mach_msg_type_number_t count = 0;
    if (enumerate_threads(pid, &threads, &count) != 0) {
        return -1;
    }

    size_t written = 0;
    for (mach_msg_type_number_t i = 0; i < count && written < max; i++) {
        uint64_t tid = 0;
        if (thread_id_for_port(threads[i], &tid) != 0) {
            continue;
        }

        arm_thread_state64_t native;
        mach_msg_type_number_t state_count = ARM_THREAD_STATE64_COUNT;
        uintptr_t pc = 0;
        if (thread_get_state(threads[i], ARM_THREAD_STATE64,
                             (thread_state_t)&native, &state_count) == KERN_SUCCESS) {
            pc = (uintptr_t)native.__pc;
        }

        out[written].tid = tid;
        out[written].pc = pc;
        written++;
    }

    deallocate_threads(threads, count);
    *count_out = written;
    return 0;
}

int cdbg_threads_resolve_port(pid_t pid, uint64_t tid, thread_act_t *port_out)
{
    thread_act_array_t threads = NULL;
    mach_msg_type_number_t count = 0;
    if (enumerate_threads(pid, &threads, &count) != 0) {
        return -1;
    }
    if (count == 0) {
        deallocate_threads(threads, count);
        return -1;
    }

    if (tid == 0) {
        *port_out = threads[0];
        for (mach_msg_type_number_t i = 1; i < count; i++) {
            mach_port_deallocate(mach_task_self(), threads[i]);
        }
        vm_deallocate(mach_task_self(), (vm_address_t)threads,
                     count * sizeof(thread_act_t));
        return 0;
    }

    thread_act_t found = MACH_PORT_NULL;
    for (mach_msg_type_number_t i = 0; i < count; i++) {
        uint64_t this_tid = 0;
        if (found == MACH_PORT_NULL && thread_id_for_port(threads[i], &this_tid) == 0 &&
            this_tid == tid) {
            found = threads[i];
            continue;
        }
        mach_port_deallocate(mach_task_self(), threads[i]);
    }
    vm_deallocate(mach_task_self(), (vm_address_t)threads, count * sizeof(thread_act_t));

    if (found == MACH_PORT_NULL) {
        return -1;
    }
    *port_out = found;
    return 0;
}

static int suspend_or_resume_others(pid_t pid, uint64_t keep_tid, int suspend)
{
    thread_act_array_t threads = NULL;
    mach_msg_type_number_t count = 0;
    if (enumerate_threads(pid, &threads, &count) != 0) {
        return -1;
    }

    if (keep_tid == 0 && count > 0) {
        uint64_t primary_tid = 0;
        if (thread_id_for_port(threads[0], &primary_tid) == 0) {
            keep_tid = primary_tid;
        }
    }

    int rc = 0;
    for (mach_msg_type_number_t i = 0; i < count; i++) {
        uint64_t this_tid = 0;
        if (thread_id_for_port(threads[i], &this_tid) == 0 && this_tid == keep_tid) {
            continue;
        }
        kern_return_t kr = suspend ? thread_suspend(threads[i]) : thread_resume(threads[i]);
        if (kr != KERN_SUCCESS) {
            rc = -1;
        }
    }

    deallocate_threads(threads, count);
    return rc;
}

int cdbg_threads_suspend_others(pid_t pid, uint64_t keep_tid)
{
    return suspend_or_resume_others(pid, keep_tid, 1);
}

int cdbg_threads_resume_others(pid_t pid, uint64_t keep_tid)
{
    return suspend_or_resume_others(pid, keep_tid, 0);
}

