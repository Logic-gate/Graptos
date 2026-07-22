CXX ?= c++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic -Iinclude
BUILD_DIR ?= build
TARGET := $(BUILD_DIR)/${executable_name}
TEST_TARGET := $(BUILD_DIR)/test_${executable_name}

.PHONY: all run test clean

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): src/main.cpp include/${project_ident}.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) src/main.cpp -o $@

$(TEST_TARGET): tests/test_main.cpp include/${project_ident}.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) tests/test_main.cpp -o $@

run: $(TARGET)
	$(TARGET)

test: $(TEST_TARGET)
	$(TEST_TARGET)

clean:
	rm -rf $(BUILD_DIR)
