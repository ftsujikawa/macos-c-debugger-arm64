#ifndef CDBG_EXPR_SHARED_H
#define CDBG_EXPR_SHARED_H

/* Internal helpers shared between debugger.c (print/set/watch command
 * handlers) and expr_eval.c (the bison/flex expression evaluator). Not part
 * of the public API in debugger.h. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

typedef struct cdbg cdbg_t;

typedef struct cdbg_struct_member {
    char name[64];
    char type[64];
    size_t offset;
    size_t size;
    bool is_signed;
} cdbg_struct_member_t;

typedef enum cdbg_var_base {
    CDBG_VAR_BASE_FB,
    CDBG_VAR_BASE_SP,
    CDBG_VAR_BASE_X29,
} cdbg_var_base_t;

typedef struct cdbg_var_info {
    char name[128];
    char type[128];
    char element_type[128];
    int64_t fbreg_offset;
    cdbg_var_base_t base;
    size_t size;
    size_t element_size;
    size_t pointee_size;
    size_t array_count;
    bool has_location;
    bool is_pointer;
    bool is_signed;
    bool is_array;
} cdbg_var_info_t;

#define CDBG_MAX_STRUCT_MEMBERS 32

size_t type_scalar_size(const char *type);
size_t type_pointee_size(const char *type);
bool   type_is_scalar(const char *type);
int    parse_array_type(const char *type, char *elem_type, size_t elem_len,
                        size_t *count_out);

int    dwarf_lookup_struct(cdbg_t *dbg, const char *name,
                           cdbg_struct_member_t *members, size_t max_members,
                           size_t *member_count_out, size_t *byte_size_out);
int    lookup_struct_member(cdbg_t *dbg, const char *struct_type,
                            const char *member_name, size_t *offset_out,
                            size_t *size_out, bool *signed_out,
                            char *type_out, size_t type_out_len);

size_t type_element_size(cdbg_t *dbg, const char *type);
void   complete_var_type(cdbg_t *dbg, cdbg_var_info_t *var);
uintptr_t var_base_addr(cdbg_t *dbg, cdbg_var_base_t base);

int  read_scalar_value(pid_t pid, uintptr_t addr, size_t size, uint64_t *out);
int  write_scalar_value(pid_t pid, uintptr_t addr, size_t size, uint64_t value);

int  resolve_variable_address(cdbg_t *dbg, const char *name, cdbg_var_info_t *var,
                              uintptr_t *addr, bool *found_local);
void pointer_pointee_type(const cdbg_var_info_t *var, char *out, size_t out_len);
int  resolve_subscript_element(cdbg_t *dbg, const char *var_name, size_t index,
                               cdbg_var_info_t *elem_view, uintptr_t *elem_addr);

#endif /* CDBG_EXPR_SHARED_H */
