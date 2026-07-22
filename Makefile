CC      = clang
BISON   = bison
FLEX    = flex
CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic -g -Iinclude -Ibuild
LDFLAGS =
CODESIGN_IDENTITY ?= -
ENTITLEMENTS      = cdbg.entitlements

SRCS = src/main.c \
       src/debugger.c \
       src/process.c \
       src/breakpoint.c \
       src/watchpoint.c \
       src/threads.c \
       src/memory.c \
       src/regs.c \
       src/lineno.c \
       src/syms.c \
       src/expr_eval.c

OBJS = $(SRCS:src/%.c=build/%.o) build/expr.tab.o build/expr.lex.o \
       build/command.tab.o build/command.lex.o
TARGET = build/cdbg

.PHONY: all clean run sign examples

all: $(TARGET)

examples: $(TARGET) build/target

build/target: examples/target.c | build
	$(CC) -g -c -o build/target.o $<
	$(CC) -g -o $@ build/target.o
	dsymutil -o $@.dSYM $@

$(TARGET): $(OBJS) | build
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	/usr/bin/codesign -s "$(CODESIGN_IDENTITY)" --entitlements $(ENTITLEMENTS) --force $@

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c -o $@ $<

# Generated expression parser (bison/flex). expr.tab.h is a byproduct of the
# bison rule; listing both outputs lets make know a single invocation
# produces them.
build/expr.tab.c build/expr.tab.h: src/expr.y | build
	$(BISON) -d -o build/expr.tab.c $<

build/expr.lex.c: src/expr.l build/expr.tab.h | build
	$(FLEX) -o $@ $<

build/expr.tab.o: build/expr.tab.c build/expr.tab.h | build
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-but-set-variable -c -o $@ $<

build/expr.lex.o: build/expr.lex.c build/expr.tab.h | build
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-parameter -Wno-sign-compare -Wno-unused-but-set-variable -c -o $@ $<

# Generated REPL command parser (bison/flex).
build/command.tab.c build/command.tab.h: src/command.y | build
	$(BISON) -d -o build/command.tab.c $<

build/command.lex.c: src/command.l build/command.tab.h | build
	$(FLEX) -o $@ $<

build/command.tab.o: build/command.tab.c build/command.tab.h | build
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-but-set-variable -c -o $@ $<

build/command.lex.o: build/command.lex.c build/command.tab.h | build
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-parameter -Wno-sign-compare -Wno-unused-but-set-variable -c -o $@ $<

build:
	mkdir -p build

clean:
	rm -rf build

run: $(TARGET)
	./$(TARGET)
