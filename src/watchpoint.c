#include "watchpoint.h"

#include <string.h>

#include "regs.h"

#define WCR_ENABLE    (1ULL << 0)
#define WCR_PAC_EL0   (2ULL << 1)  /* match EL0 (user) accesses only */
#define WCR_LSC_LOAD  (1ULL << 3)
#define WCR_LSC_STORE (2ULL << 3)
#define WCR_BAS_SHIFT 5

#define ESR_EC_SHIFT              26
#define ESR_EC_MASK               0x3fu
#define ESR_EC_WATCHPOINT_LOWER_EL 0x34u
#define ESR_EC_WATCHPOINT_SAME_EL  0x35u
#define ESR_WNR_BIT               (1u << 6)

int cdbg_watch_arm(pid_t pid, int slot, uintptr_t addr, size_t size,
                   cdbg_watch_kind_t kind)
{
    if (slot < 0 || slot >= CDBG_MAX_WATCHPOINTS) {
        return -1;
    }
    if (size == 0 || size > 8) {
        return -1;
    }

    unsigned offset = (unsigned)(addr & 0x7);
    if (offset + size > 8) {
        /* Crosses an 8-byte granule; a single BAS mask cannot cover it. */
        return -1;
    }

    arm_debug_state64_t state;
    if (cdbg_regs_get_debug_state(pid, &state) != 0) {
        return -1;
    }

    uint64_t bas = ((1ULL << size) - 1) << offset;
    uint64_t wcr = WCR_ENABLE | WCR_PAC_EL0 | (bas << WCR_BAS_SHIFT);
    if (kind & CDBG_WATCH_READ) {
        wcr |= WCR_LSC_LOAD;
    }
    if (kind & CDBG_WATCH_WRITE) {
        wcr |= WCR_LSC_STORE;
    }

    state.__wvr[slot] = (uint64_t)(addr & ~0x7ULL);
    state.__wcr[slot] = wcr;

    if (cdbg_regs_set_debug_state(pid, &state) != 0) {
        return -1;
    }

    /* Some hardware implements fewer than CDBG_MAX_WATCHPOINTS slots; a
     * read-back mismatch means this slot doesn't really exist. */
    arm_debug_state64_t verify;
    if (cdbg_regs_get_debug_state(pid, &verify) != 0 || verify.__wcr[slot] != wcr) {
        return -1;
    }

    return 0;
}

int cdbg_watch_disarm(pid_t pid, int slot)
{
    if (slot < 0 || slot >= CDBG_MAX_WATCHPOINTS) {
        return -1;
    }

    arm_debug_state64_t state;
    if (cdbg_regs_get_debug_state(pid, &state) != 0) {
        return -1;
    }

    state.__wcr[slot] &= ~(uint64_t)WCR_ENABLE;
    return cdbg_regs_set_debug_state(pid, &state);
}

bool cdbg_watch_addr_hit(const cdbg_watchpoint_t *wp, uintptr_t fault_addr)
{
    if (!wp->enabled) {
        return false;
    }
    return fault_addr >= wp->addr && fault_addr < wp->addr + wp->size;
}

bool cdbg_watch_is_watchpoint_esr(uint32_t esr, bool *is_write_out)
{
    uint32_t ec = (esr >> ESR_EC_SHIFT) & ESR_EC_MASK;
    if (ec != ESR_EC_WATCHPOINT_LOWER_EL && ec != ESR_EC_WATCHPOINT_SAME_EL) {
        return false;
    }
    if (is_write_out != NULL) {
        *is_write_out = (esr & ESR_WNR_BIT) != 0;
    }
    return true;
}
