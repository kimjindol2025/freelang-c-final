# FreeLang C Runtime - Makefile
# Build system for C-based FreeLang interpreter

CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c11 \
          -I./include \
          -I./lib/src \
          -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lm -lpthread -ldl
DEBUG   = -g -O0 -DDEBUG

# Directories
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
TEST_DIR = test
BENCH_DIR = bench

# Targets
TARGET  = $(BIN_DIR)/fl
LIB_TARGET = $(OBJ_DIR)/libfl.a
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
TEST_SOURCES = $(wildcard $(TEST_DIR)/test_*.c)
TEST_TARGETS = $(TEST_SOURCES:$(TEST_DIR)/%.c=$(BIN_DIR)/%)

# Default target
.PHONY: all clean test debug run install help setup

all: setup $(TARGET)
	@echo "✅ Build complete: $(TARGET)"

# Setup directories
setup:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR)
	@echo "📁 Created build directories"

# Main executable
$(TARGET): $(OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "🔧 Linked: $@"

# Object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@
	@echo "⚙️  Compiled: $<"

# Static library
$(LIB_TARGET): $(OBJECTS)
	@mkdir -p $(OBJ_DIR)
	ar rcs $@ $^
	@echo "📦 Created library: $@"

lib: $(LIB_TARGET)

# Unit tests
test: setup debug bin/test_typechecker_simple
	@echo "🧪 Running simple typechecker tests..."
	@if [ -f bin/test_typechecker_simple ]; then \
		echo "\n📝 Running: bin/test_typechecker_simple"; \
		bin/test_typechecker_simple; \
	fi
	@echo "✅ Simple typechecker tests completed"

test-full: setup debug bin/test_typechecker
	@echo "🧪 Running full typechecker tests..."
	@if [ -f bin/test_typechecker ]; then \
		echo "\n📝 Running: bin/test_typechecker"; \
		bin/test_typechecker; \
	fi
	@echo "✅ Full typechecker tests completed"

$(BIN_DIR)/test_%: $(TEST_DIR)/test_%.c $(filter-out $(OBJ_DIR)/main.o, $(OBJECTS))
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(DEBUG) -o $@ $^ $(LDFLAGS)
	@echo "🔨 Built: $@"

# Debug build with symbols
debug: CFLAGS += $(DEBUG)
debug: clean setup $(TARGET)
	@echo "🐛 Debug build complete"

# Release build with optimizations
release: CFLAGS += -O3 -DNDEBUG
release: clean setup $(TARGET)
	@echo "⚡ Release build complete"

# Run example
run: all
	@if [ -f examples/hello_world.fl ]; then \
		./$(TARGET) examples/hello_world.fl; \
	else \
		echo "❌ Example file not found: examples/hello_world.fl"; \
	fi

# Interactive REPL
repl: all
	./$(TARGET) --repl

# Benchmarks
bench: all
	@echo "🏃 Running benchmarks..."
	@if [ -f $(BENCH_DIR)/benchmark.c ]; then \
		$(CC) $(CFLAGS) -O3 -o $(BIN_DIR)/benchmark $(BENCH_DIR)/benchmark.c $(filter-out $(OBJ_DIR)/main.o, $(OBJECTS)) $(LDFLAGS); \
		./$(BIN_DIR)/benchmark; \
	else \
		echo "⚠️  No benchmarks found"; \
	fi

# Install to system
install: all
	@echo "📥 Installing to /usr/local/bin/..."
	@sudo cp $(TARGET) /usr/local/bin/fl
	@echo "✅ Installed: /usr/local/bin/fl"

# Clean build artifacts
clean:
	@rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "🧹 Cleaned build files"

# Deep clean (remove everything)
distclean: clean
	@find . -name "*.o" -delete
	@find . -name "*.a" -delete
	@echo "🗑️  Full cleanup complete"

# Show help
help:
	@echo "FreeLang C Runtime - Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  make all      - Build main executable (default)"
	@echo "  make lib      - Build static library"
	@echo "  make test     - Run unit tests"
	@echo "  make debug    - Build with debug symbols"
	@echo "  make release  - Optimized release build"
	@echo "  make run      - Build and run example"
	@echo "  make repl     - Start interactive REPL"
	@echo "  make bench    - Run benchmarks"
	@echo "  make install  - Install to /usr/local/bin/"
	@echo "  make clean    - Remove build files"
	@echo "  make help     - Show this help message"
	@echo ""
	@echo "Examples:"
	@echo "  make              # Build with defaults"
	@echo "  make clean all    # Clean rebuild"
	@echo "  make test debug   # Build with symbols and run tests"

# Phony targets
.PHONY: all lib test debug release run repl bench install clean distclean help setup
