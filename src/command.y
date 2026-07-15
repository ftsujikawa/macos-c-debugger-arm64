%{
#include <stdlib.h>
#include <string.h>

#include "command_ast.h"

char cmd_print_fmt_char; /* set by command.l right before returning KW_PRINT_FMT */

static cdbg_command_t g_cmd_result;

int cmd_lex(void);
void cmd_error(const char *msg);
void cmd__scan_string(const char *text);
void cmd_lex_destroy(void);
int cmd_parse(void);

%}

%name-prefix="cmd_"

%union {
    char *text;
}

%token <text> RAWLINE_TEXT
%token <text> WORD
%token KW_HELP KW_RUN KW_CONTINUE KW_STEP KW_SI KW_NEXT KW_UP KW_REGS
%token KW_PRINT KW_SET KW_SHOW KW_TB KW_LEAKS KW_BREAK KW_DEL
%token KW_WATCH KW_RWATCH KW_AWATCH KW_DELWATCH KW_DIS KW_LIST KW_LINES
%token KW_LISTS KW_SYMS KW_X KW_KILL KW_QUIT
%token KW_PRINT_FMT

%type <text> opt_rawline opt_word

%%

command:
      KW_HELP opt_rawline {
        g_cmd_result.kind = CDBG_CMD_HELP;
        g_cmd_result.arg1 = $2;
    }
    | KW_RUN opt_rawline {
        g_cmd_result.kind = CDBG_CMD_RUN;
        g_cmd_result.arg1 = $2;
    }
    | KW_CONTINUE { g_cmd_result.kind = CDBG_CMD_CONTINUE; }
    | KW_STEP     { g_cmd_result.kind = CDBG_CMD_STEP; }
    | KW_SI       { g_cmd_result.kind = CDBG_CMD_SI; }
    | KW_NEXT     { g_cmd_result.kind = CDBG_CMD_NEXT; }
    | KW_UP       { g_cmd_result.kind = CDBG_CMD_UP; }
    | KW_REGS     { g_cmd_result.kind = CDBG_CMD_REGS; }
    | KW_PRINT opt_rawline {
        g_cmd_result.kind = CDBG_CMD_PRINT;
        g_cmd_result.print_fmt = '\0';
        g_cmd_result.arg1 = $2;
    }
    | KW_PRINT_FMT opt_rawline {
        g_cmd_result.kind = CDBG_CMD_PRINT;
        g_cmd_result.print_fmt = cmd_print_fmt_char;
        g_cmd_result.arg1 = $2;
    }
    | KW_SET opt_rawline {
        g_cmd_result.kind = CDBG_CMD_SET;
        g_cmd_result.arg1 = $2;
    }
    | KW_SHOW opt_rawline {
        g_cmd_result.kind = CDBG_CMD_SHOW;
        g_cmd_result.arg1 = $2;
    }
    | KW_TB    { g_cmd_result.kind = CDBG_CMD_TB; }
    | KW_LEAKS { g_cmd_result.kind = CDBG_CMD_LEAKS; }
    | KW_BREAK opt_word {
        g_cmd_result.kind = CDBG_CMD_BREAK;
        g_cmd_result.arg1 = $2;
    }
    | KW_DEL opt_rawline {
        g_cmd_result.kind = CDBG_CMD_DEL;
        g_cmd_result.arg1 = $2;
    }
    | KW_WATCH opt_rawline {
        g_cmd_result.kind = CDBG_CMD_WATCH;
        g_cmd_result.watch_kind = CDBG_WATCH_WRITE;
        g_cmd_result.arg1 = $2;
    }
    | KW_RWATCH opt_rawline {
        g_cmd_result.kind = CDBG_CMD_WATCH;
        g_cmd_result.watch_kind = CDBG_WATCH_READ;
        g_cmd_result.arg1 = $2;
    }
    | KW_AWATCH opt_rawline {
        g_cmd_result.kind = CDBG_CMD_WATCH;
        g_cmd_result.watch_kind = CDBG_WATCH_ACCESS;
        g_cmd_result.arg1 = $2;
    }
    | KW_DELWATCH opt_rawline {
        g_cmd_result.kind = CDBG_CMD_DELWATCH;
        g_cmd_result.arg1 = $2;
    }
    | KW_DIS opt_rawline {
        g_cmd_result.kind = CDBG_CMD_DIS;
        g_cmd_result.arg1 = $2;
    }
    | KW_LIST opt_rawline {
        g_cmd_result.kind = CDBG_CMD_LIST;
        g_cmd_result.arg1 = $2;
    }
    | KW_LINES opt_word {
        g_cmd_result.kind = CDBG_CMD_LINES;
        g_cmd_result.arg1 = $2;
    }
    | KW_LISTS opt_word {
        g_cmd_result.kind = CDBG_CMD_LISTS;
        g_cmd_result.arg1 = $2;
    }
    | KW_SYMS opt_word {
        g_cmd_result.kind = CDBG_CMD_SYMS;
        g_cmd_result.arg1 = $2;
    }
    | KW_X WORD opt_word {
        g_cmd_result.kind = CDBG_CMD_X;
        g_cmd_result.arg1 = $2;
        g_cmd_result.arg2 = $3;
    }
    | KW_X {
        g_cmd_result.kind = CDBG_CMD_X;
    }
    | KW_KILL { g_cmd_result.kind = CDBG_CMD_KILL; }
    | KW_QUIT { g_cmd_result.kind = CDBG_CMD_QUIT; }
    | WORD {
        g_cmd_result.kind = CDBG_CMD_UNKNOWN;
        g_cmd_result.unknown_text = $1;
    }
    | /* empty line */ { g_cmd_result.kind = CDBG_CMD_EMPTY; }
    ;

opt_rawline: RAWLINE_TEXT { $$ = $1; }
    | /* empty */         { $$ = NULL; }
    ;

opt_word: WORD    { $$ = $1; }
    | /* empty */ { $$ = NULL; }
    ;

%%

void cmd_error(const char *msg)
{
    (void)msg; /* command-specific usage errors are reported by the caller */
}

int cdbg_command_parse(const char *line, cdbg_command_t *out)
{
    memset(&g_cmd_result, 0, sizeof(g_cmd_result));
    cmd_print_fmt_char = '\0';

    cmd__scan_string(line);
    (void)cmd_parse();
    cmd_lex_destroy();

    *out = g_cmd_result;
    memset(&g_cmd_result, 0, sizeof(g_cmd_result));
    return 0;
}

void cdbg_command_free(cdbg_command_t *cmd)
{
    if (cmd == NULL) {
        return;
    }
    free(cmd->arg1);
    free(cmd->arg2);
    free(cmd->unknown_text);
    memset(cmd, 0, sizeof(*cmd));
}
