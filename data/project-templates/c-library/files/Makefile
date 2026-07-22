CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Wpedantic -Iinclude
AR ?= ar
TARGET := lib${project_slug}.a
SOURCES := src/${project_ident}.c
OBJECTS := $(SOURCES:.c=.o)

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(AR) rcs $@ $^

test: $(TARGET)
	$(CC) $(CFLAGS) tests/test_${project_ident}.c $(TARGET) -o tests/test_${project_ident}
	./tests/test_${project_ident}

clean:
	rm -f $(TARGET) $(OBJECTS) tests/test_${project_ident}
