# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude

# Executable name
EXEC = database

# Source files
SRCS = \
	src/main.cpp \
	src/Btree.cpp \
	src/db/table.cpp \
	src/db/database.cpp

# Object files (replace .cpp with .o)
OBJS = $(SRCS:src/%.cpp=build/%.o)

# Default target: build executable
all: $(EXEC)

# Link object files into executable
$(EXEC): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Rule to build object files from source files
build/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up generated files
clean:
	rm -rf $(OBJS) $(EXEC)

# Phony targets (not actual files)
.PHONY: all clean
