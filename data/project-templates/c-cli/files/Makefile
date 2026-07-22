CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Wpedantic -Iinclude
TARGET := ${project_slug}
SOURCES := src/main.c

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(SOURCES) include/${project_ident}.h
	$(CC) $(CFLAGS) $(SOURCES) -o $@

test:
	$(CC) $(CFLAGS) tests/test_main.c -o tests/test_main
	./tests/test_main

clean:
	rm -f $(TARGET) tests/test_main
