CC = clang
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -g
SOURCE := src/main.c \
	src/lex.c \
	src/syn.c \
	src/ast.c \
	src/sem.c \
	src/gen.c \


BUILD = build/ifj26

.PHONY: all clean test

all: $(BUILD)

$(BUILD): $(SOURCE) Makefile
	mkdir -p build
	$(CC) $(CFLAGS) $(SOURCE) -o $(BUILD)

clean:
	rm -rf build

test: $(BUILD)
	python3 tests/test.py