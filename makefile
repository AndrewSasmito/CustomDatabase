# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -Iinclude `pkg-config --cflags botan-3`
LDLIBS = `pkg-config --libs botan-3`

# Executables
MAIN_EXEC = database
TEST_EXEC = test

# Source files (shared between both)
CORE_SRCS = \
	src/Btree.cpp \
	src/db/table.cpp \
	src/db/database.cpp \
	src/hash_util.cpp

MAIN_SRC = src/main.cpp
TEST_SRC = src/test.cpp

# Object files
CORE_OBJS = $(CORE_SRCS:src/%.cpp=build/%.o)
MAIN_OBJ = $(MAIN_SRC:src/%.cpp=build/%.o)
TEST_OBJ = $(TEST_SRC:src/%.cpp=build/%.o)

# Targets
all: $(MAIN_EXEC)

$(MAIN_EXEC): $(CORE_OBJS) $(MAIN_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

$(TEST_EXEC): $(CORE_OBJS) $(TEST_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

# Generic rule for building .o files
build/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean
clean:
	rm -rf build/ $(MAIN_EXEC) $(TEST_EXEC)

.PHONY: all clean
