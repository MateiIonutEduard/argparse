# Makefile for argparse static library

# Detect OS
UNAME_S := $(shell uname -s)
ifeq ($(OS),Windows_NT)
    DETECTED_OS := Windows
else
    ifeq ($(UNAME_S),Linux)
        DETECTED_OS := Linux
    endif
    ifeq ($(UNAME_S),Darwin)
        DETECTED_OS := macOS
    endif
    ifeq ($(UNAME_S),FreeBSD)
        DETECTED_OS := FreeBSD
    endif
endif

# Default compiler - will be overridden by OS-specific settings
CC ?= gcc
AR ?= ar
RANLIB ?= ranlib

# Compiler and archiver flags
CFLAGS := -Wall -Wextra -pedantic -std=c99 -O2 -I.
ARFLAGS := rcs

# Library name
LIBRARY_NAME := libargparse
STATIC_LIB := $(LIBRARY_NAME).a

# Source files
SRCS := argparse.c
OBJS := $(SRCS:.c=.o)

# OS-specific settings
ifeq ($(DETECTED_OS),Windows)
    # Windows settings
    CC := gcc
    AR := ar
    RANLIB := ranlib
    # Additional Windows-specific flags if needed
    CFLAGS += -D_WIN32
else ifeq ($(DETECTED_OS),Linux)
    # Linux settings
    CC := gcc
    AR := ar
    RANLIB := ranlib
    CFLAGS += -D_LINUX
else ifeq ($(DETECTED_OS),macOS)
    # macOS settings
    CC := clang
    AR := ar
    RANLIB := ranlib
    CFLAGS += -D_DARWIN
else
    # Fallback for unknown OS
    $(warning Unknown OS: $(UNAME_S). Using default compiler settings.)
endif

# Detect if we're using clang and adjust warnings accordingly
COMPILER_VERSION := $(shell $(CC) --version 2>/dev/null)
ifneq (,$(findstring clang,$(COMPILER_VERSION)))
    CFLAGS += -Wno-unused-parameter
else
    # GCC specific flags
    CFLAGS += -Wno-unused-but-set-parameter
endif

# Default target - build static library
all: $(STATIC_LIB)

# Create static library
$(STATIC_LIB): $(OBJS)
	@echo "Building static library for $(DETECTED_OS)..."
	$(AR) $(ARFLAGS) $@ $^
	$(RANLIB) $@
	@echo "Built: $@"

# Compile source files
%.o: %.c argparse.h
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJS) $(STATIC_LIB)

# Debug build
debug: CFLAGS += -g -DDEBUG -O0
debug: clean all

# Release build (default is already with -O2)
release: clean all

# Show build information
info:
	@echo "Build Information:"
	@echo "  OS: $(DETECTED_OS)"
	@echo "  Compiler: $(CC)"
	@echo "  Archiver: $(AR)"
	@echo "  CFLAGS: $(CFLAGS)"
	@echo "  Source files: $(SRCS)"
	@echo "  Object files: $(OBJS)"
	@echo "  Static library: $(STATIC_LIB)"

# Phony targets
.PHONY: all clean debug release info

# Help target
help:
	@echo "Available targets:"
	@echo "  all     - Build the static library (default)"
	@echo "  clean   - Remove build artifacts"
	@echo "  debug   - Build with debug symbols"
	@echo "  release - Build with optimization (default)"
	@echo "  info    - Show build information"
	@echo "  help    - Show this help message"
