CC := gcc
PROJECT := orarend
PREFIX ?= /usr/local
CFLAGS := -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic $(shell mariadb_config --cflags)
LDFLAGS := $(shell mariadb_config --libs)
INCLUDES := -Iinclude
SRC_DIR := src
SRC := $(wildcard $(SRC_DIR)/*.c)
ifeq ($(SRC),)
$(error No source files found in $(SRC_DIR))
endif

ifeq ($(MODE),release)
BUILD_DIR := build/release
BUILD_ARGS ?= -O3 -DNDEBUG
else
BUILD_DIR := build
BUILD_ARGS ?= -g -DDEBUG
endif
OBJ := $(SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
DEP := $(OBJ:.o=.d)

GREEN := $(shell printf '[0;32m')
CYAN := $(shell printf '[0;36m')
RESET := $(shell printf '[0m')
ECHO = @echo

.DEFAULT_GOAL := all
.PHONY: all build release clean help install uninstall test

all: build

build: $(BUILD_DIR)/$(PROJECT)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(ECHO) "$(CYAN)[BUILD]$(RESET) Compiling $<..."
	@$(CC) $(CFLAGS) -MMD -MP -c "$<" -o "$@" $(INCLUDES) $(BUILD_ARGS)

$(BUILD_DIR)/$(PROJECT): $(OBJ)
	$(ECHO) "$(CYAN)[LINK]$(RESET) Creating binary at $@"
	@$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(BUILD_ARGS)
	$(ECHO) "$(GREEN)[OK]$(RESET) Build complete: $@"

release:
	$(ECHO) "$(CYAN)[RELEASE]$(RESET) Building release version..."
	@$(MAKE) MODE=release build

install: build
	install -Dm755 $(BUILD_DIR)/$(PROJECT) $(DESTDIR)$(PREFIX)/bin/$(PROJECT)

uninstall:
	$(RM) $(DESTDIR)$(PREFIX)/bin/$(PROJECT)

test: tests/test_print.c src/print.c
	@mkdir -p build
	@$(CC) $(CFLAGS) $(INCLUDES) $^ -o build/test_print $(LDFLAGS)
	@./build/test_print

clean:
	$(ECHO) "$(CYAN)[CLEAN]$(RESET) Removing build directory..."
	@$(RM) -r build
	$(ECHO) "$(GREEN)[OK]$(RESET) Clean complete."

help:
	$(ECHO) "$(CYAN)[HELP]$(RESET) Available targets:"
	$(ECHO) "$(CYAN)[HELP]$(RESET)   build     - Compile the project (build/$(PROJECT))"
	$(ECHO) "$(CYAN)[HELP]$(RESET)   release   - Optimized build (build/release/$(PROJECT))"
	$(ECHO) "$(CYAN)[HELP]$(RESET)   clean     - Remove build files"

-include $(DEP)
