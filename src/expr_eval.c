#include "expr.h"

#include "debugger.h"
#include "expr_ast.h"
#include "expr_shared.h"
#include "memory.h"
#include "regs.h"
#include "syms.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cdbg_expr_node_t *cdbg_expr_node_new(cdbg_expr_kind_t kind)
{
    cdbg_expr_node_t *node = calloc(1, sizeof(*node));
    node->kind = kind;
    return node;
}

void cdbg_expr_node_free(cdbg_expr_node_t *node)
{
    if (node == NULL) {
        return;
    }
    cdbg_expr_node_free(node->lhs);
    cdbg_expr_node_free(node->rhs);
    free(node);
}

typedef struct {
    uintptr_t addr;
    size_t size;
    bool is_signed;
    char type[128];
} lvalue_t;

static int eval_lvalue(cdbg_t *dbg, cdbg_expr_node_t *node, lvalue_t *out);
static int eval_value(cdbg_t *dbg, cdbg_expr_node_t *node, cdbg_expr_result_t *out);

static void make_pointer_type(const char *base_type, char *out, size_t out_len)
{
    if (base_type == NULL || base_type[0] == '\0') {
        out[0] = '\0';
        return;
    }

    size_t len = strlen(base_type);
    while (len > 0 && base_type[len - 1] == ' ') {
        len--;
    }

    if (len >= out_len - 3) {
        len = out_len - 4;
    }
    memcpy(out, base_type, len);
    out[len] = '\0';
    snprintf(out + len, out_len - len, " *");
}

static void strip_pointer_star(const char *type, char *out, size_t out_len)
{
    const char *star = strchr(type, '*');
    if (star == NULL) {
        out[0] = '\0';
        return;
    }

    size_t len = (size_t)(star - type);
    while (len > 0 && type[len - 1] == ' ') {
        len--;
    }
    if (len >= out_len) {
        len = out_len - 1;
    }
    memcpy(out, type, len);
    out[len] = '\0';
}

static int combine_binary(uint64_t left, uint64_t right, cdbg_expr_op_t op, uint64_t *out)
{
    switch (op) {
    case CDBG_OP_ADD: *out = left + right; return 0;
    case CDBG_OP_SUB: *out = left - right; return 0;
    case CDBG_OP_MUL: *out = left * right; return 0;
    case CDBG_OP_DIV:
        if (right == 0) {
            fputs("Division by zero\n", stderr);
            return -1;
        }
        *out = left / right;
        return 0;
    case CDBG_OP_MOD:
        if (right == 0) {
            fputs("Division by zero\n", stderr);
            return -1;
        }
        *out = left % right;
        return 0;
    case CDBG_OP_SHL: *out = left << right; return 0;
    case CDBG_OP_SHR: *out = left >> right; return 0;
    case CDBG_OP_LT: *out = left < right ? 1 : 0; return 0;
    case CDBG_OP_GT: *out = left > right ? 1 : 0; return 0;
    case CDBG_OP_LE: *out = left <= right ? 1 : 0; return 0;
    case CDBG_OP_GE: *out = left >= right ? 1 : 0; return 0;
    case CDBG_OP_EQ: *out = left == right ? 1 : 0; return 0;
    case CDBG_OP_NE: *out = left != right ? 1 : 0; return 0;
    case CDBG_OP_BAND: *out = left & right; return 0;
    case CDBG_OP_BXOR: *out = left ^ right; return 0;
    case CDBG_OP_BOR: *out = left | right; return 0;
    case CDBG_OP_LAND: *out = (left && right) ? 1 : 0; return 0;
    case CDBG_OP_LOR: *out = (left || right) ? 1 : 0; return 0;
    default:
        fputs("Invalid operator\n", stderr);
        return -1;
    }
}

/* Evaluates `node` as a pointer: returns its numeric pointer value and (when
 * known) the pointee's type string. Used by DEREF, ARROW, and pointer-typed
 * INDEX bases. */
static int eval_as_pointer(cdbg_t *dbg, cdbg_expr_node_t *node, uint64_t *ptr_value_out,
                           char *pointee_type_out, size_t pointee_type_len)
{
    pointee_type_out[0] = '\0';

    if (node->kind == CDBG_EXPR_IDENT) {
        cdbg_var_info_t var = {0};
        uintptr_t addr = 0;
        bool found_local = false;
        if (resolve_variable_address(dbg, node->ident, &var, &addr, &found_local) != 0) {
            fprintf(stderr, "Unknown variable: %s\n", node->ident);
            return -1;
        }
        if (!var.is_pointer) {
            fprintf(stderr, "%s is not a pointer\n", node->ident);
            return -1;
        }
        uint64_t ptr = 0;
        if (read_scalar_value(dbg->pid, addr, sizeof(uintptr_t), &ptr) != 0) {
            return -1;
        }
        *ptr_value_out = ptr;
        pointer_pointee_type(&var, pointee_type_out, pointee_type_len);
        return 0;
    }

    if (node->kind == CDBG_EXPR_MEMBER || node->kind == CDBG_EXPR_ARROW ||
        node->kind == CDBG_EXPR_INDEX || node->kind == CDBG_EXPR_DEREF) {
        lvalue_t lv;
        if (eval_lvalue(dbg, node, &lv) != 0) {
            return -1;
        }
        if (strchr(lv.type, '*') == NULL) {
            fputs("Not a pointer\n", stderr);
            return -1;
        }
        uint64_t ptr = 0;
        if (read_scalar_value(dbg->pid, lv.addr, sizeof(uintptr_t), &ptr) != 0) {
            return -1;
        }
        *ptr_value_out = ptr;
        strip_pointer_star(lv.type, pointee_type_out, pointee_type_len);
        return 0;
    }

    /* Arbitrary expression (literal address, pointer arithmetic result, ...):
     * treat the numeric result as a raw address with no type information. */
    cdbg_expr_result_t val = {0};
    if (eval_value(dbg, node, &val) != 0) {
        return -1;
    }
    *ptr_value_out = val.value;
    return 0;
}

static int eval_lvalue(cdbg_t *dbg, cdbg_expr_node_t *node, lvalue_t *out)
{
    memset(out, 0, sizeof(*out));

    switch (node->kind) {
    case CDBG_EXPR_IDENT: {
        cdbg_var_info_t var = {0};
        uintptr_t addr = 0;
        bool found_local = false;
        if (resolve_variable_address(dbg, node->ident, &var, &addr, &found_local) != 0) {
            fprintf(stderr, "Unknown variable: %s\n", node->ident);
            return -1;
        }
        if (var.is_array) {
            fprintf(stderr, "Cannot use array \"%s\" directly; index it first\n",
                    node->ident);
            return -1;
        }
        out->addr = addr;
        out->size = var.size;
        out->is_signed = var.is_signed;
        snprintf(out->type, sizeof(out->type), "%s", var.type);
        return 0;
    }

    case CDBG_EXPR_DEREF: {
        uint64_t ptr_value = 0;
        char pointee_type[128] = {0};
        if (eval_as_pointer(dbg, node->lhs, &ptr_value, pointee_type,
                            sizeof(pointee_type)) != 0) {
            return -1;
        }
        out->addr = (uintptr_t)ptr_value;
        if (pointee_type[0] != '\0') {
            snprintf(out->type, sizeof(out->type), "%s", pointee_type);
            out->size = type_element_size(dbg, pointee_type);
            out->is_signed = strstr(pointee_type, "unsigned") == NULL;
        } else {
            out->type[0] = '\0';
            out->size = sizeof(uint64_t);
            out->is_signed = false;
        }
        return 0;
    }

    case CDBG_EXPR_MEMBER:
    case CDBG_EXPR_ARROW: {
        char base_type[128] = {0};
        uintptr_t base_addr = 0;

        if (node->kind == CDBG_EXPR_ARROW) {
            uint64_t ptr_value = 0;
            if (eval_as_pointer(dbg, node->lhs, &ptr_value, base_type,
                                sizeof(base_type)) != 0) {
                return -1;
            }
            base_addr = (uintptr_t)ptr_value;
        } else {
            lvalue_t base_lv;
            if (eval_lvalue(dbg, node->lhs, &base_lv) != 0) {
                return -1;
            }
            base_addr = base_lv.addr;
            snprintf(base_type, sizeof(base_type), "%s", base_lv.type);
        }

        size_t offset = 0;
        size_t size = 0;
        bool is_signed = false;
        char member_type[128] = {0};
        if (lookup_struct_member(dbg, base_type, node->ident, &offset, &size, &is_signed,
                                 member_type, sizeof(member_type)) != 0) {
            fprintf(stderr, "Unknown member: %s\n", node->ident);
            return -1;
        }
        out->addr = base_addr + offset;
        out->size = size;
        out->is_signed = is_signed;
        snprintf(out->type, sizeof(out->type), "%s",
                member_type[0] != '\0' ? member_type : base_type);
        return 0;
    }

    case CDBG_EXPR_INDEX: {
        cdbg_expr_result_t idx = {0};
        if (eval_value(dbg, node->rhs, &idx) != 0) {
            return -1;
        }
        size_t index = (size_t)idx.value;

        if (node->lhs->kind == CDBG_EXPR_IDENT) {
            cdbg_var_info_t elem_view = {0};
            uintptr_t elem_addr = 0;
            if (resolve_subscript_element(dbg, node->lhs->ident, index, &elem_view,
                                          &elem_addr) != 0) {
                return -1;
            }
            out->addr = elem_addr;
            out->size = elem_view.element_size;
            snprintf(out->type, sizeof(out->type), "%s", elem_view.element_type);
            out->is_signed = strstr(elem_view.element_type, "unsigned") == NULL;
            return 0;
        }

        uint64_t ptr_value = 0;
        char pointee_type[128] = {0};
        if (eval_as_pointer(dbg, node->lhs, &ptr_value, pointee_type,
                            sizeof(pointee_type)) != 0) {
            return -1;
        }
        size_t elem_size = pointee_type[0] != '\0' ? type_element_size(dbg, pointee_type)
                                                    : sizeof(uint64_t);
        out->addr = (uintptr_t)ptr_value + index * elem_size;
        out->size = elem_size;
        snprintf(out->type, sizeof(out->type), "%s", pointee_type);
        out->is_signed = strstr(pointee_type, "unsigned") == NULL;
        return 0;
    }

    default:
        /* Not an lvalue-shaped expression (a literal, unary/binary arithmetic
         * result, or &expr). Silent, like the old resolve_lvalue: callers
         * either fall back to value-mode evaluation (print) or report their
         * own contextual error (set, watch). */
        return -1;
    }
}

static int eval_value(cdbg_t *dbg, cdbg_expr_node_t *node, cdbg_expr_result_t *out)
{
    memset(out, 0, sizeof(*out));

    switch (node->kind) {
    case CDBG_EXPR_NUM:
        out->value = node->number;
        out->fvalue = node->fvalue;
        out->is_float = node->is_float;
        return 0;

    case CDBG_EXPR_IDENT: {
        cdbg_var_info_t var = {0};
        uintptr_t addr = 0;
        bool found_local = false;
        if (resolve_variable_address(dbg, node->ident, &var, &addr, &found_local) != 0) {
            fprintf(stderr, "Unknown variable: %s\n", node->ident);
            return -1;
        }
        uint64_t value = 0;
        if (read_scalar_value(dbg->pid, addr, var.size, &value) != 0) {
            return -1;
        }
        out->value = value;
        snprintf(out->type, sizeof(out->type), "%s", var.type);
        return 0;
    }

    case CDBG_EXPR_MEMBER:
    case CDBG_EXPR_ARROW:
    case CDBG_EXPR_INDEX:
    case CDBG_EXPR_DEREF: {
        lvalue_t lv;
        if (eval_lvalue(dbg, node, &lv) != 0) {
            return -1;
        }
        uint64_t value = 0;
        if (read_scalar_value(dbg->pid, lv.addr, lv.size, &value) != 0) {
            return -1;
        }
        out->value = value;
        snprintf(out->type, sizeof(out->type), "%s", lv.type);
        return 0;
    }

    case CDBG_EXPR_ADDR: {
        lvalue_t lv;
        if (eval_lvalue(dbg, node->lhs, &lv) != 0) {
            return -1;
        }
        out->value = lv.addr;
        out->is_address = true;
        make_pointer_type(lv.type, out->type, sizeof(out->type));
        return 0;
    }

    case CDBG_EXPR_UNARY: {
        cdbg_expr_result_t inner = {0};
        if (eval_value(dbg, node->lhs, &inner) != 0) {
            return -1;
        }
        switch (node->op) {
        case CDBG_OP_POS:
            *out = inner;
            return 0;
        case CDBG_OP_NEG:
            if (inner.is_float) {
                out->fvalue = -inner.fvalue;
                out->is_float = true;
                memcpy(&out->value, &out->fvalue, sizeof(out->value));
            } else {
                out->value = (uint64_t)(-(int64_t)inner.value);
            }
            return 0;
        case CDBG_OP_NOT:
            out->value = inner.value ? 0 : 1;
            return 0;
        case CDBG_OP_BNOT:
            out->value = ~inner.value;
            return 0;
        default:
            fputs("Invalid operator\n", stderr);
            return -1;
        }
    }

    case CDBG_EXPR_BINARY: {
        cdbg_expr_result_t l = {0};
        cdbg_expr_result_t r = {0};
        if (eval_value(dbg, node->lhs, &l) != 0 || eval_value(dbg, node->rhs, &r) != 0) {
            return -1;
        }
        if (combine_binary(l.value, r.value, node->op, &out->value) != 0) {
            return -1;
        }
        return 0;
    }

    default:
        fputs("Invalid expression\n", stderr);
        return -1;
    }
}

int cdbg_expr_eval(cdbg_t *dbg, const char *text, cdbg_expr_result_t *out)
{
    if (dbg == NULL || text == NULL || out == NULL) {
        return -1;
    }

    if (cdbg_language_check_expr(dbg) != 0) {
        return -1;
    }
    if (cdbg_refresh_regs(dbg) != 0) {
        return -1;
    }

    cdbg_expr_node_t *root = cdbg_expr_parse(text);
    if (root == NULL) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    int rc = eval_value(dbg, root, out);
    cdbg_expr_node_free(root);
    return rc;
}

int cdbg_expr_eval_lvalue(cdbg_t *dbg, const char *text, uintptr_t *addr_out,
                         size_t *size_out, bool *signed_out, char *type_out,
                         size_t type_out_len, bool *whole_struct_out)
{
    if (dbg == NULL || text == NULL || addr_out == NULL) {
        return -1;
    }

    cdbg_expr_node_t *root = cdbg_expr_parse(text);
    if (root == NULL) {
        return -1;
    }

    if (root->kind == CDBG_EXPR_ADDR) {
        fputs("Cannot use an address expression as an lvalue\n", stderr);
        cdbg_expr_node_free(root);
        return -1;
    }

    lvalue_t lv;
    int rc = eval_lvalue(dbg, root, &lv);
    if (rc == 0) {
        *addr_out = lv.addr;
        if (size_out != NULL) {
            *size_out = lv.size;
        }
        if (signed_out != NULL) {
            *signed_out = lv.is_signed;
        }
        if (type_out != NULL && type_out_len > 0) {
            snprintf(type_out, type_out_len, "%s", lv.type);
        }
        if (whole_struct_out != NULL) {
            bool is_terminal = root->kind == CDBG_EXPR_IDENT ||
                               root->kind == CDBG_EXPR_DEREF ||
                               root->kind == CDBG_EXPR_INDEX;
            *whole_struct_out = is_terminal && lv.type[0] != '\0' &&
                               !type_is_scalar(lv.type);
        }
    }

    cdbg_expr_node_free(root);
    return rc;
}
