#include "regs.h"

#include <mach/mach.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "threads.h"

int cdbg_regs_get(pid_t pid, uint64_t tid, cdbg_regs_t *regs)
{
    thread_act_t thread = MACH_PORT_NULL;
    if (cdbg_threads_resolve_port(pid, tid, &thread) != 0) {
        return -1;
    }

    mach_msg_type_number_t count = CDBG_THREAD_FLAVOR_COUNT;
    kern_return_t kr = thread_get_state(thread, CDBG_THREAD_FLAVOR,
                                        (thread_state_t)&regs->native, &count);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "thread_get_state failed: %s (%d)\n", mach_error_string(kr), kr);
        mach_port_deallocate(mach_task_self(), thread);
        return -1;
    }

    mach_msg_type_number_t neon_count = CDBG_NEON_FLAVOR_COUNT;
    kr = thread_get_state(thread, CDBG_NEON_FLAVOR,
                          (thread_state_t)&regs->neon, &neon_count);
    mach_port_deallocate(mach_task_self(), thread);

    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "thread_get_state (fp) failed: %s (%d)\n", mach_error_string(kr), kr);
        return -1;
    }

    return 0;
}

int cdbg_regs_set(pid_t pid, uint64_t tid, const cdbg_regs_t *regs)
{
    thread_act_t thread = MACH_PORT_NULL;
    if (cdbg_threads_resolve_port(pid, tid, &thread) != 0) {
        return -1;
    }

    kern_return_t kr = thread_set_state(thread, CDBG_THREAD_FLAVOR,
                                        (thread_state_t)&regs->native,
                                        CDBG_THREAD_FLAVOR_COUNT);
    mach_port_deallocate(mach_task_self(), thread);

    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "thread_set_state failed: %s (%d)\n", mach_error_string(kr), kr);
        return -1;
    }

    return 0;
}

int cdbg_regs_get_debug_state(pid_t pid, uint64_t tid, arm_debug_state64_t *state)
{
    thread_act_t thread = MACH_PORT_NULL;
    if (cdbg_threads_resolve_port(pid, tid, &thread) != 0) {
        return -1;
    }

    mach_msg_type_number_t count = CDBG_DEBUG_FLAVOR_COUNT;
    kern_return_t kr = thread_get_state(thread, CDBG_DEBUG_FLAVOR,
                                        (thread_state_t)state, &count);
    mach_port_deallocate(mach_task_self(), thread);

    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "thread_get_state (debug) failed: %s (%d)\n", mach_error_string(kr), kr);
        return -1;
    }
    return 0;
}

int cdbg_regs_set_debug_state(pid_t pid, uint64_t tid, const arm_debug_state64_t *state)
{
    thread_act_t thread = MACH_PORT_NULL;
    if (cdbg_threads_resolve_port(pid, tid, &thread) != 0) {
        return -1;
    }

    kern_return_t kr = thread_set_state(thread, CDBG_DEBUG_FLAVOR,
                                        (thread_state_t)state,
                                        CDBG_DEBUG_FLAVOR_COUNT);
    mach_port_deallocate(mach_task_self(), thread);

    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "thread_set_state (debug) failed: %s (%d)\n", mach_error_string(kr), kr);
        return -1;
    }
    return 0;
}

int cdbg_regs_get_exception_state(pid_t pid, uint64_t tid, uint64_t *far_out,
                                  uint32_t *esr_out)
{
    thread_act_t thread = MACH_PORT_NULL;
    if (cdbg_threads_resolve_port(pid, tid, &thread) != 0) {
        return -1;
    }

    arm_exception_state64_t exc;
    mach_msg_type_number_t count = ARM_EXCEPTION_STATE64_COUNT;
    kern_return_t kr = thread_get_state(thread, ARM_EXCEPTION_STATE64,
                                        (thread_state_t)&exc, &count);
    mach_port_deallocate(mach_task_self(), thread);

    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "thread_get_state (exception) failed: %s (%d)\n", mach_error_string(kr), kr);
        return -1;
    }

    *far_out = (uint64_t)exc.__far;
    *esr_out = (uint32_t)exc.__esr;
    return 0;
}

uintptr_t cdbg_regs_pc(const cdbg_regs_t *regs)
{
    return (uintptr_t)regs->native.__pc;
}

int cdbg_regs_set_pc(cdbg_regs_t *regs, uintptr_t pc)
{
    regs->native.__pc = pc;
    return 0;
}

uintptr_t cdbg_regs_fp(const cdbg_regs_t *regs)
{
    return (uintptr_t)regs->native.__fp;
}

static int ret_addr_valid(uint64_t addr)
{
    return addr >= 0x1000;
}

static int unwind_from_stack_top(pid_t pid, cdbg_regs_t *regs, uintptr_t sp)
{
    uint64_t ret_addr = regs->native.__lr;
    if (!ret_addr_valid(ret_addr)) {
        return -1;
    }
    if (cdbg_regs_set_pc(regs, (uintptr_t)ret_addr) != 0) {
        return -1;
    }
    uintptr_t fp = cdbg_regs_fp(regs);
    if (fp != 0) {
        uint64_t saved_fp = 0;
        if (cdbg_mem_read_u64(pid, fp, &saved_fp) == 0 && saved_fp > fp) {
            regs->native.__fp = saved_fp;
        }
    }
    regs->native.__sp = sp + 16;
    return 0;
}

int cdbg_regs_frame_up(pid_t pid, cdbg_regs_t *regs)
{
    uintptr_t fp = cdbg_regs_fp(regs);
    uintptr_t sp = (uintptr_t)regs->native.__sp;

    if (fp != 0) {
        uint64_t saved_fp = 0;
        uint64_t ret_addr = 0;
        if (cdbg_mem_read_u64(pid, fp, &saved_fp) == 0 &&
            cdbg_mem_read_u64(pid, fp + 8, &ret_addr) == 0 &&
            saved_fp > fp && ret_addr_valid(ret_addr)) {
            if (cdbg_regs_set_pc(regs, (uintptr_t)ret_addr) != 0) {
                return -1;
            }
            regs->native.__fp = saved_fp;
            regs->native.__sp = fp + 16;
            return 0;
        }
    }

    return unwind_from_stack_top(pid, regs, sp);
}

void cdbg_regs_print(const cdbg_regs_t *regs)
{
    printf("--- General-Purpose Registers ---\n");
    for (int i = 0; i < 28; i += 2) {
        printf("x%-2d = 0x%016llx  x%-2d = 0x%016llx\n",
               i,     (unsigned long long)regs->native.__x[i],
               i + 1, (unsigned long long)regs->native.__x[i + 1]);
    }
    printf("x28 = 0x%016llx\n", (unsigned long long)regs->native.__x[28]);
    printf("fp  = 0x%016llx  lr  = 0x%016llx\n",
           (unsigned long long)regs->native.__fp,
           (unsigned long long)regs->native.__lr);
    printf("sp  = 0x%016llx  pc  = 0x%016llx\n",
           (unsigned long long)regs->native.__sp,
           (unsigned long long)regs->native.__pc);
    printf("cpsr = 0x%08x\n", (unsigned)regs->native.__cpsr);

    printf("\n--- Floating-Point / NEON Registers ---\n");
    printf("fpsr = 0x%08x  fpcr = 0x%08x\n",
           (unsigned)regs->neon.__fpsr, (unsigned)regs->neon.__fpcr);
    for (int i = 0; i < 32; i++) {
        uint64_t lo = (uint64_t)regs->neon.__v[i];
        uint64_t hi = (uint64_t)(regs->neon.__v[i] >> 64);
        double d;
        memcpy(&d, &lo, sizeof(d));
        printf("v%-2d = 0x%016llx%016llx  (f64: %g)\n",
               i, (unsigned long long)hi, (unsigned long long)lo, d);
    }
}

static int cdbg_regs_set_fp(pid_t pid, uint64_t tid, const cdbg_regs_t *regs)
{
    thread_act_t thread = MACH_PORT_NULL;
    if (cdbg_threads_resolve_port(pid, tid, &thread) != 0) {
        return -1;
    }

    kern_return_t kr = thread_set_state(thread, CDBG_NEON_FLAVOR,
                                        (thread_state_t)&regs->neon,
                                        CDBG_NEON_FLAVOR_COUNT);
    mach_port_deallocate(mach_task_self(), thread);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "thread_set_state (fp) failed: %s (%d)\n", mach_error_string(kr), kr);
        return -1;
    }
    return 0;
}

static int set_gpr_field(cdbg_regs_t *regs, const char *name, uint64_t value)
{
    if (strcmp(name, "pc") == 0)  { regs->native.__pc = value; return 0; }
    if (strcmp(name, "sp") == 0)  { regs->native.__sp = value; return 0; }
    if (strcmp(name, "fp") == 0 || strcmp(name, "x29") == 0)
                                  { regs->native.__fp = value; return 0; }
    if (strcmp(name, "lr") == 0 || strcmp(name, "x30") == 0)
                                  { regs->native.__lr = value; return 0; }
    if (strcmp(name, "cpsr") == 0){ regs->native.__cpsr = (uint32_t)value; return 0; }
    if (name[0] == 'x') {
        char *endp;
        long idx = strtol(name + 1, &endp, 10);
        if (*endp == '\0' && idx >= 0 && idx <= 28) {
            regs->native.__x[idx] = value;
            return 0;
        }
    }
    return -1;
}


static int set_fp_field(cdbg_regs_t *regs, const char *name,
                         uint64_t value, bool is_float, double fvalue)
{
    (void)is_float; (void)fvalue;
    if (strcmp(name, "fpsr") == 0) { regs->neon.__fpsr = (uint32_t)value; return 0; }
    if (strcmp(name, "fpcr") == 0) { regs->neon.__fpcr = (uint32_t)value; return 0; }
    if (name[0] == 'v') {
        char *endp;
        long idx = strtol(name + 1, &endp, 10);
        if (*endp == '\0' && idx >= 0 && idx <= 31) {
            regs->neon.__v[idx] = (__uint128_t)value;
            return 0;
        }
    }
    return -1;
}

int cdbg_regs_get_by_name(const cdbg_regs_t *regs, const char *name, uint64_t *out)
{
    if (strcmp(name, "pc") == 0)   { *out = (uint64_t)regs->native.__pc;   return 0; }
    if (strcmp(name, "sp") == 0)   { *out = (uint64_t)regs->native.__sp;   return 0; }
    if (strcmp(name, "fp") == 0 || strcmp(name, "x29") == 0)
                                   { *out = (uint64_t)regs->native.__fp;   return 0; }
    if (strcmp(name, "lr") == 0 || strcmp(name, "x30") == 0)
                                   { *out = (uint64_t)regs->native.__lr;   return 0; }
    if (strcmp(name, "cpsr") == 0) { *out = (uint64_t)regs->native.__cpsr; return 0; }
    if (strcmp(name, "fpsr") == 0) { *out = (uint64_t)regs->neon.__fpsr;   return 0; }
    if (strcmp(name, "fpcr") == 0) { *out = (uint64_t)regs->neon.__fpcr;   return 0; }
    if (name[0] == 'x') {
        char *endp;
        long idx = strtol(name + 1, &endp, 10);
        if (*endp == '\0' && idx >= 0 && idx <= 28) {
            *out = (uint64_t)regs->native.__x[idx];
            return 0;
        }
    }
    if (name[0] == 'v') {
        char *endp;
        long idx = strtol(name + 1, &endp, 10);
        if (*endp == '\0' && idx >= 0 && idx <= 31) {
            *out = (uint64_t)regs->neon.__v[idx];
            return 0;
        }
    }
    return -1;
}

int cdbg_regs_set_by_name(pid_t pid, uint64_t tid, cdbg_regs_t *regs, const char *name,
                           uint64_t value, bool is_float, double fvalue)
{
    if (set_gpr_field(regs, name, value) == 0) {
        return cdbg_regs_set(pid, tid, regs);
    }
    if (set_fp_field(regs, name, value, is_float, fvalue) == 0) {
        return cdbg_regs_set_fp(pid, tid, regs);
    }
    return -1;
}
