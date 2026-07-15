#ifndef CDBG_EXPR_AST_H
#define CDBG_EXPR_AST_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    CDBG_EXPR_NUM,      /* literal number (int or float) */
    CDBG_EXPR_IDENT,    /* bare identifier: variable or symbol name */
    CDBG_EXPR_UNARY,    /* op applied to lhs */
    CDBG_EXPR_BINARY,   /* lhs op rhs */
    CDBG_EXPR_MEMBER,   /* lhs . ident */
    CDBG_EXPR_ARROW,    /* lhs -> ident */
    CDBG_EXPR_INDEX,    /* lhs [ rhs ] */
    CDBG_EXPR_ADDR,     /* & lhs   (lhs must be lvalue-capable) */
    CDBG_EXPR_DEREF,    /* * lhs */
} cdbg_expr_kind_t;

typedef enum {
    CDBG_OP_ADD, CDBG_OP_SUB, CDBG_OP_MUL, CDBG_OP_DIV, CDBG_OP_MOD,
    CDBG_OP_SHL, CDBG_OP_SHR,
    CDBG_OP_LT, CDBG_OP_GT, CDBG_OP_LE, CDBG_OP_GE, CDBG_OP_EQ, CDBG_OP_NE,
    CDBG_OP_BAND, CDBG_OP_BXOR, CDBG_OP_BOR, CDBG_OP_LAND, CDBG_OP_LOR,
    CDBG_OP_POS, CDBG_OP_NEG, CDBG_OP_NOT, CDBG_OP_BNOT,
} cdbg_expr_op_t;

typedef struct cdbg_expr_node {
    cdbg_expr_kind_t kind;
    cdbg_expr_op_t op;          /* valid for UNARY / BINARY */
    uint64_t number;            /* integer value, or float bit pattern */
    double fvalue;
    bool is_float;
    char ident[128];            /* IDENT name, or MEMBER/ARROW member name */
    struct cdbg_expr_node *lhs; /* binary left; unary/deref/addr operand;
                                 * member/arrow/index base */
    struct cdbg_expr_node *rhs; /* binary right; index subscript expr */
} cdbg_expr_node_t;

cdbg_expr_node_t *cdbg_expr_node_new(cdbg_expr_kind_t kind);
void cdbg_expr_node_free(cdbg_expr_node_t *node);

/* Parses `text` into an AST. Returns NULL on syntax error (message already
 * printed to stderr). Caller owns the result and must free it with
 * cdbg_expr_node_free(). */
cdbg_expr_node_t *cdbg_expr_parse(const char *text);

#endif /* CDBG_EXPR_AST_H */
