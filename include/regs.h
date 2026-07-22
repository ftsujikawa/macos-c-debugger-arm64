#ifndef CDBG_REGS_H
#define CDBG_REGS_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#include <mach/arm/thread_status.h>

typedef struct cdbg_regs {
    arm_thread_state64_t native;
    arm_neon_state64_t   neon;
} cdbg_regs_t;

#define CDBG_THREAD_FLAVOR       ARM_THREAD_STATE64
#define CDBG_THREAD_FLAVOR_COUNT ARM_THREAD_STATE64_COUNT
#define CDBG_NEON_FLAVOR         ARM_NEON_STATE64
#define CDBG_NEON_FLAVOR_COUNT   ARM_NEON_STATE64_COUNT
#define CDBG_DEBUG_FLAVOR        ARM_DEBUG_STATE64
#define CDBG_DEBUG_FLAVOR_COUNT  ARM_DEBUG_STATE64_COUNT

/* Every function below takes `tid` (a thread_id from cdbg_threads_list, or 0
 * for "the primary thread") to identify which thread of `pid` to operate on. */

int  cdbg_regs_get(pid_t pid, uint64_t tid, cdbg_regs_t *regs);
int  cdbg_regs_set(pid_t pid, uint64_t tid, const cdbg_regs_t *regs);
int  cdbg_regs_get_debug_state(pid_t pid, uint64_t tid, arm_debug_state64_t *state);
int  cdbg_regs_set_debug_state(pid_t pid, uint64_t tid, const arm_debug_state64_t *state);
int  cdbg_regs_get_exception_state(pid_t pid, uint64_t tid, uint64_t *far_out,
                                   uint32_t *esr_out);
void cdbg_regs_print(const cdbg_regs_t *regs);
uintptr_t cdbg_regs_pc(const cdbg_regs_t *regs);
uintptr_t cdbg_regs_fp(const cdbg_regs_t *regs);
int cdbg_regs_set_pc(cdbg_regs_t *regs, uintptr_t pc);
int cdbg_regs_frame_up(pid_t pid, cdbg_regs_t *regs);
int cdbg_regs_get_by_name(const cdbg_regs_t *regs, const char *name, uint64_t *out);
int cdbg_regs_set_by_name(pid_t pid, uint64_t tid, cdbg_regs_t *regs, const char *name,
                           uint64_t value, bool is_float, double fvalue);

#endif /* CDBG_REGS_H */
