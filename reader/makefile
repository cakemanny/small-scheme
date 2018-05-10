
SHELL=/bin/bash
PLATFORM=$(shell uname)

CC=clang
CFLAGS=-std=gnu11 -g -Wall # -fsanitize=address
LDFLAGS=
LDLIBS=

ifeq "$(PLATFORM)" "Linux"
  LDLIBS+=-lbsd
endif

HEADERS := symbol.h tokens.h ast.h runtime.h evaluator.h eval2.h

reader: lexer.o reader.o symbol.o runtime.o ast.o evaluator.o misc.o eval2.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ $<

lex.yy.c: lexer.l
	flex $<

.PHONY: clean

clean:
	rm -f *.o reader lex.yy.c

