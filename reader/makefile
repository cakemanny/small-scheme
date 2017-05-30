
CC=clang
CFLAGS=-std=gnu11 -g -fsanitize=address
LDFLAGS=
LDLIBS=

HEADERS := symbol.h tokens.h ast.h runtime.h

reader: lexer.o reader.o symbol.o runtime.o ast.o evaluator.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ $<

lex.yy.c: lexer.l
	flex $<

.PHONY: clean

clean:
	rm -f *.o reader lex.yy.c

