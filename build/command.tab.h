/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton interface for Bison's Yacc-like parsers in C

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     RAWLINE_TEXT = 258,
     WORD = 259,
     KW_HELP = 260,
     KW_RUN = 261,
     KW_CONTINUE = 262,
     KW_STEP = 263,
     KW_SI = 264,
     KW_NEXT = 265,
     KW_UP = 266,
     KW_REGS = 267,
     KW_PRINT = 268,
     KW_SET = 269,
     KW_SHOW = 270,
     KW_TB = 271,
     KW_LEAKS = 272,
     KW_BREAK = 273,
     KW_DEL = 274,
     KW_WATCH = 275,
     KW_RWATCH = 276,
     KW_AWATCH = 277,
     KW_DELWATCH = 278,
     KW_DIS = 279,
     KW_LIST = 280,
     KW_LINES = 281,
     KW_LISTS = 282,
     KW_SYMS = 283,
     KW_X = 284,
     KW_KILL = 285,
     KW_QUIT = 286,
     KW_PRINT_FMT = 287
   };
#endif
/* Tokens.  */
#define RAWLINE_TEXT 258
#define WORD 259
#define KW_HELP 260
#define KW_RUN 261
#define KW_CONTINUE 262
#define KW_STEP 263
#define KW_SI 264
#define KW_NEXT 265
#define KW_UP 266
#define KW_REGS 267
#define KW_PRINT 268
#define KW_SET 269
#define KW_SHOW 270
#define KW_TB 271
#define KW_LEAKS 272
#define KW_BREAK 273
#define KW_DEL 274
#define KW_WATCH 275
#define KW_RWATCH 276
#define KW_AWATCH 277
#define KW_DELWATCH 278
#define KW_DIS 279
#define KW_LIST 280
#define KW_LINES 281
#define KW_LISTS 282
#define KW_SYMS 283
#define KW_X 284
#define KW_KILL 285
#define KW_QUIT 286
#define KW_PRINT_FMT 287




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 21 "src/command.y"
{
    char *text;
}
/* Line 1529 of yacc.c.  */
#line 117 "build/command.tab.h"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE cmd_lval;

