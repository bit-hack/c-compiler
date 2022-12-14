CFLAGS=-O0 -g

TEST_CFLAGS=\
 -Wno-implicit-function-declaration\
 -Wno-overflow

.PHONY: all clean

all: parse exec dasm

parse: parse.c util.c defs.h
	gcc parse.c util.c ${CFLAGS} -o $@

exec: exec.c util.c defs.h
	gcc exec.c util.c ${CFLAGS} -o $@

dasm: dasm.c util.c defs.h
	gcc dasm.c util.c ${CFLAGS} -o $@

# note: the @ prefix stops echoing
test: parse exec
	@for FILE in tests/*.c; do \
		echo "Testing $$FILE"; \
		gcc ${TEST_CFLAGS} $$FILE; \
		./a.out; \
		echo "ref  $$?"; \
		./parse $$FILE | ./exec; \
		echo "test $$?"; \
	done

# note: the @ prefix stops echoing
fuzz: parse exec
	@for FILE in fuzz/*.c; do \
		echo "Testing $$FILE"; \
		gcc ${TEST_CFLAGS} $$FILE; \
		./parse $$FILE | ./exec 1 > /dev/null; \
	done

clean:
	rm -rf parse exec dasm a.out
