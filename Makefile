CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Iinclude -g
BUILD    := build
TARGET   := cviz.exe

SRCS := $(wildcard src/*.cpp)
OBJS := $(patsubst src/%.cpp,$(BUILD)/%.o,$(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILD)/%.o: src/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD):
	@if not exist $(BUILD) mkdir $(BUILD)

clean:
	@if exist $(BUILD) rmdir /s /q $(BUILD)
	@if exist $(TARGET) del $(TARGET)

.PHONY: all clean