CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Iinclude -O2
BUILD    := build
TARGET   := cviz

SRCS := $(wildcard src/*.cpp)
OBJS := $(patsubst src/%.cpp,$(BUILD)/%.o,$(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILD)/%.o: src/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

test: $(TARGET)
	sh tools/evaluate.sh

clean:
	rm -rf $(BUILD) $(TARGET)

.PHONY: all clean test
