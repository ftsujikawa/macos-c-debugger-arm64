#ifndef CDBG_WATCHPOINT_H
#define CDBG_WATCHPOINT_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#define CDBG_MAX_WATCHPOINTS 4
#define CDBG_WATCH_EXPR_LEN  128

typedef enum {
    CDBG_WATCH_READ   = 1,
    CDBG_WATCH_WRITE  = 2,
    CDBG_WATCH_ACCESS = 3, /* read | write */
} cdbg_watch_kind_t;

typedef struct cdbg_watchpoint {
    uintptr_t addr;
    size_t size;
    cdbg_watch_kind_t kind;
    bool enabled;
    int slot;
    char expr[CDBG_WATCH_EXPR_LEN];
    uint64_t last_value;
    bool has_last_value;
} cdbg_watchpoint_t;

/* Programs hardware watchpoint register `slot` (0..CDBG_MAX_WATCHPOINTS-1) on the
 * debuggee's primary thread to trap on accesses of `kind` to [addr, addr+size).
 * Returns -1 if size/alignment is unsupported or the slot does not exist in
 * hardware. */
int cdbg_watch_arm(pid_t pid, int slot, uintptr_t addr, size_t size,
                   cdbg_watch_kind_t kind);
int cdbg_watch_disarm(pid_t pid, int slot);

bool cdbg_watch_addr_hit(const cdbg_watchpoint_t *wp, uintptr_t fault_addr);

/* Decodes an ARM64 ESR value; returns true and sets *is_write_out if it
 * describes a watchpoint debug exception. */
bool cdbg_watch_is_watchpoint_esr(uint32_t esr, bool *is_write_out);

#endif /* CDBG_WATCHPOINT_H */
