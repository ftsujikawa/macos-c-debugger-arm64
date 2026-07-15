%{
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "expr_ast.h"
#include "expr.tab.h"

static void set_number_token(const char *text)
{
    char *end_i = NULL;
    unsigned long long uval = strtoull(text, &end_i, 0);

    if (end_i != text && (*end_i == '.' || *end_i == 'e' || *end_i == 'E')) {
        char *end_d = NULL;
        double dval = strtod(text, &end_d);
        uint64_t bits;
        memcpy(&bits, &dval, sizeof(bits));
        expr_lval.numtok.ival = bits;
        expr_lval.numtok.fval = dval;
        expr_lval.numtok.is_float = 1;
        return;
    }

    expr_lval.numtok.ival = (uint64_t)uval;
    expr_lval.numtok.fval = 0.0;
    expr_lval.numtok.is_float = 0;
}

static void set_float_token(const char *text)
{
    double dval = strtod(text, NULL);
    uint64_t bits;
    memcpy(&bits, &dval, sizeof(bits));
    expr_lval.numtok.ival = bits;
    expr_lval.numtok.fval = dval;
    expr_lval.numtok.is_float = 1;
}
%}

%option prefix="expr_"
%option noyywrap nounput noinput

DIGIT   [0-9]
IDSTART [A-Za-z_]
IDCHAR  [A-Za-z0-9_]

%%

[ \t]+                          ; /* skip whitespace */

"->"                             { return ARROW; }
"&&"                             { return ANDAND; }
"||"                             { return OROR; }
"=="                             { return TOK_EQ; }
"!="                             { return TOK_NE; }
"<="                             { return LE; }
">="                             { return GE; }
"<<"                             { return SHL; }
">>"                             { return SHR; }

0[xX][0-9a-fA-F]+                { set_number_token(expr_text); return NUMBER; }
{DIGIT}+"."{DIGIT}*([eE][+-]?{DIGIT}+)?[fFlL]?  { set_float_token(expr_text); return NUMBER; }
"."{DIGIT}+[fF]?                 { set_float_token(expr_text); return NUMBER; }
{DIGIT}+[eE][+-]?{DIGIT}+[fFlL]? { set_float_token(expr_text); return NUMBER; }
{DIGIT}+                         { set_number_token(expr_text); return NUMBER; }

{IDSTART}{IDCHAR}*               {
                                    snprintf(expr_lval.ident, sizeof(expr_lval.ident),
                                             "%s", expr_text);
                                    return IDENT;
                                 }

[-+*/%&|^!~().\[\]<>]           { return expr_text[0]; }

.                                { /* unrecognized character: let the parser
                                    * report a syntax error via expr_error */
                                    return expr_text[0];
                                 }

%%
