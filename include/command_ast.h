#ifndef CDBG_COMMAND_AST_H
#define CDBG_COMMAND_AST_H

#include "watchpoint.h"

typedef enum {
    CDBG_CMD_UNKNOWN,
    CDBG_CMD_EMPTY,
    CDBG_CMD_HELP,
    CDBG_CMD_RUN,
    CDBG_CMD_CONTINUE,
    CDBG_CMD_STEP,
    CDBG_CMD_SI,
    CDBG_CMD_NEXT,
    CDBG_CMD_UP,
    CDBG_CMD_REGS,
    CDBG_CMD_PRINT,
    CDBG_CMD_SET,
    CDBG_CMD_SHOW,
    CDBG_CMD_TB,
    CDBG_CMD_LEAKS,
    CDBG_CMD_BREAK,
    CDBG_CMD_DEL,
    CDBG_CMD_WATCH,
    CDBG_CMD_DELWATCH,
    CDBG_CMD_DIS,
    CDBG_CMD_LIST,
    CDBG_CMD_LINES,
    CDBG_CMD_LISTS,
    CDBG_CMD_SYMS,
    CDBG_CMD_X,
    CDBG_CMD_KILL,
    CDBG_CMD_QUIT,
    CDBG_CMD_THREAD,
} cdbg_cmd_kind_t;

typedef struct {
    cdbg_cmd_kind_t kind;
    char print_fmt;               /* PRINT only: char after '/', or '\0' */
    cdbg_watch_kind_t watch_kind; /* WATCH only */
    char *arg1;                   /* owned; NULL if omitted */
    char *arg2;                   /* owned; X's optional count */
    char *unknown_text;           /* owned; set when kind == CDBG_CMD_UNKNOWN */
} cdbg_command_t;

/* Parses one REPL input line into a structured command. Always succeeds
 * (returns 0); unrecognized keywords yield CDBG_CMD_UNKNOWN and a blank line
 * yields CDBG_CMD_EMPTY, matching the previous strtok-based dispatcher's
 * tolerance for bad input. */
int cdbg_command_parse(const char *line, cdbg_command_t *out);
void cdbg_command_free(cdbg_command_t *cmd);

#endif /* CDBG_COMMAND_AST_H */
