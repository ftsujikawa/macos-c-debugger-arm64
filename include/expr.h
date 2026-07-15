#ifndef CDBG_EXPR_H
#define CDBG_EXPR_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct cdbg cdbg_t;

typedef struct cdbg_expr_result {
    uint64_t value;
    double   fvalue;
    bool     is_float;
    bool     is_address;
    char     type[128];
} cdbg_expr_result_t;

int cdbg_expr_eval(cdbg_t *dbg, const char *text, cdbg_expr_result_t *out);

/* Resolves `text` to an lvalue: an address, size, signedness, and type
 * string. Used by print/set/watch to locate their target (a variable,
 * struct member, array element, or dereferenced pointer). Fails if `text`
 * does not denote a storage location (e.g. a literal or `&expr`). */
int cdbg_expr_eval_lvalue(cdbg_t *dbg, const char *text, uintptr_t *addr_out,
                         size_t *size_out, bool *signed_out, char *type_out,
                         size_t type_out_len, bool *whole_struct_out);

#endif /* CDBG_EXPR_H */
