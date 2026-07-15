%{
#include <stdio.h>
#include <string.h>

#include "expr_ast.h"

static cdbg_expr_node_t *g_cdbg_expr_result;
static int g_cdbg_expr_had_error;

int expr_lex(void);
void expr_error(const char *msg);
void expr__scan_string(const char *text);
void expr_lex_destroy(void);
int expr_parse(void);

static cdbg_expr_node_t *make_binary(cdbg_expr_op_t op, cdbg_expr_node_t *lhs,
                                     cdbg_expr_node_t *rhs)
{
    cdbg_expr_node_t *node = cdbg_expr_node_new(CDBG_EXPR_BINARY);
    node->op = op;
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

static cdbg_expr_node_t *make_unary(cdbg_expr_op_t op, cdbg_expr_node_t *operand)
{
    cdbg_expr_node_t *node = cdbg_expr_node_new(CDBG_EXPR_UNARY);
    node->op = op;
    node->lhs = operand;
    return node;
}
%}

%name-prefix="expr_"

%union {
    struct {
        uint64_t ival;
        double fval;
        int is_float;
    } numtok;
    char ident[128];
    cdbg_expr_node_t *node;
}

%token <numtok> NUMBER
%token <ident> IDENT
%token ARROW ANDAND OROR TOK_EQ TOK_NE LE GE SHL SHR

%type <node> expr unary_expr postfix_expr primary_expr

%left OROR
%left ANDAND
%left '|'
%left '^'
%left '&'
%left TOK_EQ TOK_NE
%left '<' '>' LE GE
%left SHL SHR
%left '+' '-'
%left '*' '/' '%'
%right UNARY_PREC

%%

start: expr { g_cdbg_expr_result = $1; }
     ;

expr: expr OROR expr    { $$ = make_binary(CDBG_OP_LOR, $1, $3); }
    | expr ANDAND expr  { $$ = make_binary(CDBG_OP_LAND, $1, $3); }
    | expr '|' expr     { $$ = make_binary(CDBG_OP_BOR, $1, $3); }
    | expr '^' expr     { $$ = make_binary(CDBG_OP_BXOR, $1, $3); }
    | expr '&' expr     { $$ = make_binary(CDBG_OP_BAND, $1, $3); }
    | expr TOK_EQ expr  { $$ = make_binary(CDBG_OP_EQ, $1, $3); }
    | expr TOK_NE expr  { $$ = make_binary(CDBG_OP_NE, $1, $3); }
    | expr '<' expr     { $$ = make_binary(CDBG_OP_LT, $1, $3); }
    | expr '>' expr     { $$ = make_binary(CDBG_OP_GT, $1, $3); }
    | expr LE expr      { $$ = make_binary(CDBG_OP_LE, $1, $3); }
    | expr GE expr      { $$ = make_binary(CDBG_OP_GE, $1, $3); }
    | expr SHL expr     { $$ = make_binary(CDBG_OP_SHL, $1, $3); }
    | expr SHR expr     { $$ = make_binary(CDBG_OP_SHR, $1, $3); }
    | expr '+' expr     { $$ = make_binary(CDBG_OP_ADD, $1, $3); }
    | expr '-' expr     { $$ = make_binary(CDBG_OP_SUB, $1, $3); }
    | expr '*' expr     { $$ = make_binary(CDBG_OP_MUL, $1, $3); }
    | expr '/' expr     { $$ = make_binary(CDBG_OP_DIV, $1, $3); }
    | expr '%' expr     { $$ = make_binary(CDBG_OP_MOD, $1, $3); }
    | unary_expr        { $$ = $1; }
    ;

unary_expr: '+' unary_expr %prec UNARY_PREC { $$ = make_unary(CDBG_OP_POS, $2); }
    | '-' unary_expr %prec UNARY_PREC { $$ = make_unary(CDBG_OP_NEG, $2); }
    | '!' unary_expr %prec UNARY_PREC { $$ = make_unary(CDBG_OP_NOT, $2); }
    | '~' unary_expr %prec UNARY_PREC { $$ = make_unary(CDBG_OP_BNOT, $2); }
    | '&' unary_expr %prec UNARY_PREC {
        cdbg_expr_node_t *node = cdbg_expr_node_new(CDBG_EXPR_ADDR);
        node->lhs = $2;
        $$ = node;
    }
    | '*' unary_expr %prec UNARY_PREC {
        cdbg_expr_node_t *node = cdbg_expr_node_new(CDBG_EXPR_DEREF);
        node->lhs = $2;
        $$ = node;
    }
    | postfix_expr { $$ = $1; }
    ;

postfix_expr: primary_expr { $$ = $1; }
    | postfix_expr '.' IDENT {
        cdbg_expr_node_t *node = cdbg_expr_node_new(CDBG_EXPR_MEMBER);
        node->lhs = $1;
        snprintf(node->ident, sizeof(node->ident), "%s", $3);
        $$ = node;
    }
    | postfix_expr ARROW IDENT {
        cdbg_expr_node_t *node = cdbg_expr_node_new(CDBG_EXPR_ARROW);
        node->lhs = $1;
        snprintf(node->ident, sizeof(node->ident), "%s", $3);
        $$ = node;
    }
    | postfix_expr '[' expr ']' {
        cdbg_expr_node_t *node = cdbg_expr_node_new(CDBG_EXPR_INDEX);
        node->lhs = $1;
        node->rhs = $3;
        $$ = node;
    }
    ;

primary_expr: NUMBER {
        cdbg_expr_node_t *node = cdbg_expr_node_new(CDBG_EXPR_NUM);
        node->number = $1.ival;
        node->fvalue = $1.fval;
        node->is_float = $1.is_float;
        $$ = node;
    }
    | IDENT {
        cdbg_expr_node_t *node = cdbg_expr_node_new(CDBG_EXPR_IDENT);
        snprintf(node->ident, sizeof(node->ident), "%s", $1);
        $$ = node;
    }
    | '(' expr ')' { $$ = $2; }
    ;

%%

void expr_error(const char *msg)
{
    g_cdbg_expr_had_error = 1;
    fprintf(stderr, "%s\n", msg);
}

cdbg_expr_node_t *cdbg_expr_parse(const char *text)
{
    g_cdbg_expr_result = NULL;
    g_cdbg_expr_had_error = 0;

    expr__scan_string(text);
    int rc = expr_parse();
    expr_lex_destroy();

    if (rc != 0 || g_cdbg_expr_had_error || g_cdbg_expr_result == NULL) {
        if (g_cdbg_expr_result != NULL) {
            cdbg_expr_node_free(g_cdbg_expr_result);
        }
        return NULL;
    }
    return g_cdbg_expr_result;
}
