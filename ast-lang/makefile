
CC=clang
CFLAGS=-std=gnu11
ifndef NDEBUG
  CFLAGS += -g -DYYDEBUG
else
  CFLAGS += -O2
endif

TARGETS := ast-lang

all: $(TARGETS)

ast-lang: grammar.tab.o symbol.o ast.o lex.yy.o writer.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c symbol.h ast.h
	$(CC) $(CFLAGS) -c $<

%.tab.c: %.y
	bison -v -d -o $@ $<

lex.yy.c: lexer.l
	flex $<

.PHONY: clean

clean:
	rm -f *.o *.tab.{c,h} *.output $(TARGETS)

