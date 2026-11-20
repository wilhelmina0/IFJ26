CC = clang
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -g
SOURCE := src/main.c \
	src/lex/lex.c \
	src/syn/syn.c \
	src/ast/ast.c \


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