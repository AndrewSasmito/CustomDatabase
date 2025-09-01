CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude
SRCDIR = src
OBJDIR = obj

# Source files (only B-tree related files)
SOURCES = src/Btree.cpp src/main.cpp src/page_manager.cpp src/page_cache.cpp src/writer_queue.cpp src/wal.cpp src/job_scheduler.cpp src/checkpoint_manager.cpp
OBJECTS = $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

# Demo source files
DEMO_SOURCES = src/Btree.cpp src/content_hash_demo.cpp src/page_manager.cpp src/page_cache.cpp src/writer_queue.cpp src/wal.cpp src/job_scheduler.cpp src/checkpoint_manager.cpp
DEMO_OBJECTS = $(DEMO_SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

# Content addressable demo
ADDRESSABLE_SOURCES = src/Btree.cpp src/content_addressable_demo.cpp src/page_manager.cpp src/page_cache.cpp src/writer_queue.cpp src/wal.cpp src/job_scheduler.cpp src/checkpoint_manager.cpp
ADDRESSABLE_OBJECTS = $(ADDRESSABLE_SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

# Deduplication demo
DEDUP_SOURCES = src/Btree.cpp src/deduplication_demo.cpp src/page_manager.cpp src/page_cache.cpp src/writer_queue.cpp src/wal.cpp src/job_scheduler.cpp src/checkpoint_manager.cpp
DEDUP_OBJECTS = $(DEDUP_SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

# Cache performance demo
CACHE_PERF_SOURCES = src/Btree.cpp src/cache_performance_demo.cpp src/page_manager.cpp src/page_cache.cpp src/writer_queue.cpp src/wal.cpp src/job_scheduler.cpp src/checkpoint_manager.cpp
CACHE_PERF_OBJECTS = $(CACHE_PERF_SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

# Job scheduler demo
JOB_SCHED_SOURCES = src/Btree.cpp src/job_scheduler_demo.cpp src/page_manager.cpp src/page_cache.cpp src/writer_queue.cpp src/wal.cpp src/job_scheduler.cpp src/checkpoint_manager.cpp
JOB_SCHED_OBJECTS = $(JOB_SCHED_SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

# Target executables
TARGET = btree_test
DEMO_TARGET = content_hash_demo
ADDRESSABLE_TARGET = content_addressable_demo
DEDUP_TARGET = deduplication_demo
CACHE_PERF_TARGET = cache_performance_demo
JOB_SCHED_TARGET = job_scheduler_demo

# Default target
all: $(TARGET) $(DEMO_TARGET) $(ADDRESSABLE_TARGET) $(DEDUP_TARGET) $(CACHE_PERF_TARGET) $(JOB_SCHED_TARGET)

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

# Link addressable demo executable
$(ADDRESSABLE_TARGET): $(ADDRESSABLE_OBJECTS)
	$(CXX) $(ADDRESSABLE_OBJECTS) -o $(ADDRESSABLE_TARGET)

# Link deduplication demo executable
$(DEDUP_TARGET): $(DEDUP_OBJECTS)
	$(CXX) $(DEDUP_OBJECTS) -o $(DEDUP_TARGET)

# Link cache performance demo executable
$(CACHE_PERF_TARGET): $(CACHE_PERF_OBJECTS)
	$(CXX) $(CACHE_PERF_OBJECTS) -o $(CACHE_PERF_TARGET)

# Link job scheduler demo executable
$(JOB_SCHED_TARGET): $(JOB_SCHED_OBJECTS)
	$(CXX) $(JOB_SCHED_OBJECTS) -o $(JOB_SCHED_TARGET)

# Clean build files
clean:
	rm -rf $(OBJDIR) $(TARGET) $(DEMO_TARGET) $(ADDRESSABLE_TARGET) $(DEDUP_TARGET) $(CACHE_PERF_TARGET) $(JOB_SCHED_TARGET)

# Run the test
run: $(TARGET)
	./$(TARGET)

# Run the demo
demo: $(DEMO_TARGET)
	./$(DEMO_TARGET)

# Run the addressable demo
addressable: $(ADDRESSABLE_TARGET)
	./$(ADDRESSABLE_TARGET)

# Run the deduplication demo
dedup: $(DEDUP_TARGET)
	./$(DEDUP_TARGET)

# Run the cache performance demo
cache_perf: $(CACHE_PERF_TARGET)
	./$(CACHE_PERF_TARGET)

# Run the job scheduler demo
job_sched: $(JOB_SCHED_TARGET)
	./$(JOB_SCHED_TARGET)

# Run all tests (for CI/CD compatibility)
tests: $(TARGET) $(DEMO_TARGET) $(ADDRESSABLE_TARGET) $(DEDUP_TARGET)
	@echo "=== Running Content Hash Demo ==="
	./$(DEMO_TARGET)
	@echo ""
	@echo "=== Running Content Addressable Demo ==="
	./$(ADDRESSABLE_TARGET)
	@echo ""
	@echo "=== Running Deduplication Demo ==="
	./$(DEDUP_TARGET)
	@echo ""
	@echo "=== Testing Basic B-tree Operations ==="
	@echo -e "insert 1 apple\ninsert 2 banana\nsearch 1\nsearch 2\nquit" | ./$(TARGET) > /dev/null
	@echo "All tests passed!"

.PHONY: all clean run demo addressable dedup cache_perf job_sched tests
