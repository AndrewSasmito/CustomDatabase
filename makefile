CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude
SRCDIR = src
OBJDIR = obj

# Source files (only B-tree related files)
SOURCES = src/Btree.cpp src/main.cpp
OBJECTS = $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

# Target executable
TARGET = btree_test

# Default target
all: $(TARGET)

# Create object directory if it doesn't exist
$(OBJDIR):
	mkdir -p $(OBJDIR)

# Compile object files
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link executable
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET)

# Clean build files
clean:
	rm -rf $(OBJDIR) $(TARGET)

# Run the test
run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
