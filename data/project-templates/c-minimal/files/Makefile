CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Wpedantic
TARGET := ${executable_name}

.PHONY: all clean

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(TARGET)
