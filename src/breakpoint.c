#include "breakpoint.h"

#include "memory.h"

#include <stdio.h>

#define CDBG_TRAP_INST 0xd4200000U  /* BRK #0 */

int cdbg_bp_enable(cdbg_breakpoint_t *bp, pid_t pid, uintptr_t addr)
{
    uint32_t inst = 0;

    if (cdbg_mem_read(pid, addr, &inst, sizeof(inst)) != 0) {
        return -1;
    }

    bp->addr = addr;
    bp->saved_inst = inst;
    bp->enabled = true;

    uint32_t trap = CDBG_TRAP_INST;
    if (cdbg_mem_write(pid, addr, &trap, sizeof(trap)) != 0) {
        bp->enabled = false;
        return -1;
    }

    return 0;
}

int cdbg_bp_disable(cdbg_breakpoint_t *bp, pid_t pid)
{
    if (!bp->enabled) {
        return 0;
    }

    if (cdbg_mem_write(pid, bp->addr, &bp->saved_inst, sizeof(bp->saved_inst)) != 0) {
        return -1;
    }

    bp->enabled = false;
    return 0;
}

bool cdbg_bp_matches_pc(uintptr_t pc, const cdbg_breakpoint_t *bp)
{
    if (!bp->enabled) {
        return false;
    }
    return bp->addr == pc;
}

bool cdbg_bp_is_trap(uintptr_t pc, const cdbg_breakpoint_t *bps, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (cdbg_bp_matches_pc(pc, &bps[i])) {
            return true;
        }
    }
    return false;
}
