%{
#include <stdlib.h>
#include <string.h>

#include "command.tab.h"

extern char cmd_print_fmt_char;

/* yytext points into flex's scan buffer and is only valid until the next
 * token is lexed, so string-carrying tokens must copy it immediately. */
static char *cmd_strdup(const char *text)
{
    size_t len = strlen(text);
    char *copy = malloc(len + 1);
    memcpy(copy, text, len + 1);
    return copy;
}
%}

%option prefix="cmd_"
%option noyywrap nounput noinput
%x RAWLINE
%x TOKENS
%x IGNORE_REST

%%

<INITIAL>{
[ \t]+                     ; /* skip leading whitespace */

"print/"[^ \t\n]*          { cmd_print_fmt_char = cmd_text[6];
                             BEGIN(RAWLINE); return KW_PRINT_FMT; }
"p/"[^ \t\n]*              { cmd_print_fmt_char = cmd_text[2];
                             BEGIN(RAWLINE); return KW_PRINT_FMT; }
"print"                    { BEGIN(RAWLINE); return KW_PRINT; }
"p"                        { BEGIN(RAWLINE); return KW_PRINT; }
"help"                     { BEGIN(RAWLINE); return KW_HELP; }
"h"                        { BEGIN(RAWLINE); return KW_HELP; }
"run"                      { BEGIN(RAWLINE); return KW_RUN; }
"set"                      { BEGIN(RAWLINE); return KW_SET; }
"show"                     { BEGIN(RAWLINE); return KW_SHOW; }
"del"                      { BEGIN(RAWLINE); return KW_DEL; }
"delete"                   { BEGIN(RAWLINE); return KW_DEL; }
"watch"                    { BEGIN(RAWLINE); return KW_WATCH; }
"rwatch"                   { BEGIN(RAWLINE); return KW_RWATCH; }
"awatch"                   { BEGIN(RAWLINE); return KW_AWATCH; }
"delwatch"                 { BEGIN(RAWLINE); return KW_DELWATCH; }
"dw"                       { BEGIN(RAWLINE); return KW_DELWATCH; }
"dis"                      { BEGIN(RAWLINE); return KW_DIS; }
"list"                     { BEGIN(RAWLINE); return KW_LIST; }
"l"                        { BEGIN(RAWLINE); return KW_LIST; }

"break"                    { BEGIN(TOKENS); return KW_BREAK; }
"b"                        { BEGIN(TOKENS); return KW_BREAK; }
"x"                        { BEGIN(TOKENS); return KW_X; }
"lines"                    { BEGIN(TOKENS); return KW_LINES; }
"lists"                    { BEGIN(TOKENS); return KW_LISTS; }
"syms"                     { BEGIN(TOKENS); return KW_SYMS; }
"sym"                      { BEGIN(TOKENS); return KW_SYMS; }
"thread"                   { BEGIN(TOKENS); return KW_THREAD; }

"continue"                 { BEGIN(IGNORE_REST); return KW_CONTINUE; }
"c"                        { BEGIN(IGNORE_REST); return KW_CONTINUE; }
"step"                     { BEGIN(IGNORE_REST); return KW_STEP; }
"s"                        { BEGIN(IGNORE_REST); return KW_STEP; }
"si"                       { BEGIN(IGNORE_REST); return KW_SI; }
"next"                     { BEGIN(IGNORE_REST); return KW_NEXT; }
"n"                        { BEGIN(IGNORE_REST); return KW_NEXT; }
"up"                       { BEGIN(IGNORE_REST); return KW_UP; }
"regs"                     { BEGIN(IGNORE_REST); return KW_REGS; }
"r"                        { BEGIN(IGNORE_REST); return KW_REGS; }
"tb"                       { BEGIN(IGNORE_REST); return KW_TB; }
"leaks"                    { BEGIN(IGNORE_REST); return KW_LEAKS; }
"kill"                     { BEGIN(IGNORE_REST); return KW_KILL; }
"quit"                     { BEGIN(IGNORE_REST); return KW_QUIT; }
"q"                        { BEGIN(IGNORE_REST); return KW_QUIT; }

[^ \t\n]+                  { BEGIN(IGNORE_REST);
                             cmd_lval.text = cmd_strdup(cmd_text);
                             return WORD; }
}

<RAWLINE>{
[ \t]+                     ; /* skip leading whitespace before the argument */
.+                         { cmd_lval.text = cmd_strdup(cmd_text); return RAWLINE_TEXT; }
}

<TOKENS>{
[ \t]+                     ; /* skip */
[^ \t\n]+                  { cmd_lval.text = cmd_strdup(cmd_text); return WORD; }
}

<IGNORE_REST>.*            ; /* trailing garbage on the line is ignored,
                              * matching the previous strtok-based dispatch */

%%
