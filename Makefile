CC = clang
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -g
SOURCE = src/main.c
BUILD = build/ifj26

.PHONY: all clean

all: $(BUILD)

$(BUILD): $(SOURCE) Makefile
	mkdir -p build
	$(CC) $(CFLAGS) $(SOURCE) -o $(BUILD)

clean:
	rm -rf build