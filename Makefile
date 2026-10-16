# ==============================================================================
# Makefile: High-Performance O(1) Cache Simulator (C11)
# ==============================================================================

CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wpointer-arith -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes -O3
INCLUDES = -Iinclude

SRCDIR = src
OBJDIR = obj
BINDIR = bin

SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SOURCES))
TARGET = $(BINDIR)/cache_sim

# OS-specific executable extension and command adjustments
ifeq ($(OS),Windows_NT)
    TARGET_BIN = $(TARGET).exe
    MKDIR = if not exist "$(subst /,\,$(1))" mkdir "$(subst /,\,$(1))"
    RM = del /Q /F
    FIXPATH = $(subst /,\,$1)
else
    TARGET_BIN = $(TARGET)
    MKDIR = mkdir -p $(1)
    RM = rm -rf
    FIXPATH = $1
endif

.PHONY: all clean run

all: $(TARGET_BIN)

$(TARGET_BIN): $(OBJECTS) | $(BINDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(OBJECTS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(OBJDIR):
	@$(call MKDIR,$(OBJDIR))

$(BINDIR):
	@$(call MKDIR,$(BINDIR))

run: $(TARGET_BIN)
	@$(TARGET_BIN)

clean:
ifeq ($(OS),Windows_NT)
	@if exist $(OBJDIR) $(RM) $(OBJDIR)\*.o
	@if exist $(BINDIR) $(RM) $(BINDIR)\*.exe
else
	@$(RM) $(OBJDIR) $(BINDIR)
endif
