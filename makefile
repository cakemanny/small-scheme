
CC=clang
CFLAGS=-std=gnu11 -g -fsanitize=address
LDFLAGS=
LDLIBS=

small-scheme: grammar.tab.o lexer.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

grammar.tab.c grammar.tab.h: grammar.y
	bison -v -d $<

lex.yy.c: lexer.l
	flex $<

.PHONY: clean

clean:
	rm -f *.o small-scheme *.tab.{c,h} lex.yy.c

