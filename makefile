# Paths and flags
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -Iinclude `pkg-config --cflags botan-3`
LDLIBS = `pkg-config --libs botan-3`

# Main executable
MAIN_EXEC = build/database

# Test source files
TEST_SRC = $(wildcard tests/*/Test_*.cpp)
TEST_EXECS = $(patsubst %.cpp, build/tests/%, $(notdir $(TEST_SRC)))

# Core sources
CORE_SRCS = \
	src/Btree.cpp \
	src/db/database.cpp \
	src/hash_util.cpp \
	src/page_manager.cpp

MAIN_SRC = src/main.cpp

# Objects
CORE_OBJS = $(CORE_SRCS:src/%.cpp=build/%.o)
MAIN_OBJ = $(MAIN_SRC:src/%.cpp=build/%.o)

# Targets
all: $(MAIN_EXEC)

$(MAIN_EXEC): $(CORE_OBJS) $(MAIN_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

# Build test executables
tests: $(TEST_EXECS)

build/tests/%: tests/*/%.cpp $(CORE_OBJS)
	@mkdir -p build/tests
	$(CXX) $(CXXFLAGS) -o $@ $(CORE_OBJS) $< $(LDLIBS)

# Compile object files
build/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean
clean:
	rm -rf build/ $(MAIN_EXEC)

.PHONY: all clean tests
