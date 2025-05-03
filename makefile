# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude

# Executables
MAIN_EXEC = database
TEST_EXEC = test

# Source files (shared between both)
CORE_SRCS = \
	src/Btree.cpp \
	src/db/table.cpp \
	src/db/database.cpp

MAIN_SRC = src/main.cpp
TEST_SRC = src/test.cpp

# Object files
CORE_OBJS = $(CORE_SRCS:src/%.cpp=build/%.o)
MAIN_OBJ = $(MAIN_SRC:src/%.cpp=build/%.o)
TEST_OBJ = $(TEST_SRC:src/%.cpp=build/%.o)

# Targets
all: $(MAIN_EXEC)

$(MAIN_EXEC): $(CORE_OBJS) $(MAIN_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TEST_EXEC): $(CORE_OBJS) $(TEST_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Generic rule for building .o files
build/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean
clean:
	rm -rf build/ $(MAIN_EXEC) $(TEST_EXEC)

.PHONY: all clean
