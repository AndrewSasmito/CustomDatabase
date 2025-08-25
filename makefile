CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude
SRCDIR = src
OBJDIR = obj

# Source files (only B-tree related files)
SOURCES = src/Btree.cpp src/main.cpp src/page_manager.cpp
OBJECTS = $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

# Demo source files
DEMO_SOURCES = src/Btree.cpp src/content_hash_demo.cpp src/page_manager.cpp
DEMO_OBJECTS = $(DEMO_SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

# Target executables
TARGET = btree_test
DEMO_TARGET = content_hash_demo

# Default target
all: $(TARGET) $(DEMO_TARGET)

# Create object directory if it doesn't exist
$(OBJDIR):
	mkdir -p $(OBJDIR)

# Compile object files
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link main executable
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET)

# Link demo executable
$(DEMO_TARGET): $(DEMO_OBJECTS)
	$(CXX) $(DEMO_OBJECTS) -o $(DEMO_TARGET)

# Clean build files
clean:
	rm -rf $(OBJDIR) $(TARGET) $(DEMO_TARGET)

# Run the test
run: $(TARGET)
	./$(TARGET)

# Run the demo
demo: $(DEMO_TARGET)
	./$(DEMO_TARGET)

.PHONY: all clean run demo
