# lexis-nds — NDS/DSi Greek text reader (BlocksDS)
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#
# Build inside the BlocksDS Docker container:
#   docker compose run --rm blocksds make
#
# Or interactively:
#   docker compose run --rm blocksds
#   make

# ── BlocksDS paths (pre-set in Docker container) ────────────

BLOCKSDS            ?= /opt/blocksds/core
BLOCKSDSEXT         ?= /opt/blocksds/external
WONDERFUL_TOOLCHAIN ?= /opt/wonderful

# ── Project ─────────────────────────────────────────────────

NAME            := lexis-nds

GAME_TITLE      := lexis-nds
GAME_SUBTITLE   := Greek Text Reader
GAME_AUTHOR     := lexis-nds

# ── Sources ─────────────────────────────────────────────────
# Explicit file list so we don't pick up the desktop main.c

SOURCES_C       := source/main.c \
                   source/palette.c \
                   source/save.c \
                   source/lookup.c \
                   source/settings.c \
                   source/text_render.c \
                   source/keyboard.c \
                   source/notes.c \
                   source/drawing.c \
                   source/reader.c

INCLUDEDIRS     := source

# ── Defines ─────────────────────────────────────────────────
# SQLite: minimal footprint for NDS (no threads, no extensions)
# picolibc: integer-only printf (saves ~4KB, no floats needed)

# NDS build flag + picolibc integer-only printf

DEFINES         :=

# ── Libraries ───────────────────────────────────────────────

ARM7ELF         := $(BLOCKSDS)/sys/arm7/main_core/arm7_minimal.elf
LIBS            := -lnds9 -lc
LIBDIRS         := $(BLOCKSDS)/libs/libnds

# ── Build paths ─────────────────────────────────────────────

BUILDDIR        := build
ELF             := $(BUILDDIR)/$(NAME).elf
MAP             := $(BUILDDIR)/$(NAME).map
ROM             := $(NAME).nds

# ── Toolchain ───────────────────────────────────────────────

PREFIX          := $(WONDERFUL_TOOLCHAIN)/toolchain/gcc-arm-none-eabi/bin/arm-none-eabi-
CC              := $(PREFIX)gcc
LD              := $(PREFIX)gcc
OBJCOPY         := $(PREFIX)objcopy
NDSTOOL         := $(BLOCKSDS)/tools/ndstool/ndstool

# ── Compiler flags ──────────────────────────────────────────

ARCH            := -mthumb -mcpu=arm946e-s+nofp
SPECS           := $(BLOCKSDS)/sys/crts/dsi_arm9.specs

INCLUDEFLAGS    := $(foreach dir,$(INCLUDEDIRS),-I$(dir)) \
                   $(foreach dir,$(LIBDIRS),-isystem $(dir)/include)

LIBDIRSFLAGS    := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

CFLAGS          := -std=gnu11 \
                   -Wall -Wextra -Wpedantic -Werror \
                   -O2 \
                   $(ARCH) \
                   $(INCLUDEFLAGS) \
                   $(DEFINES) \
                   -ffunction-sections -fdata-sections \
                   -specs=$(SPECS)

LDFLAGS         := $(ARCH) $(LIBDIRSFLAGS) \
                   $(DEFINES) \
                   -Wl,-Map,$(MAP) \
                   -Wl,--gc-sections \
                   -Wl,--start-group $(LIBS) -Wl,--end-group \
                   -specs=$(SPECS)

# ── Object files ────────────────────────────────────────────

OBJS            := $(addprefix $(BUILDDIR)/,$(notdir $(SOURCES_C:.c=.o)))
VPATH           := source

# ── Targets ─────────────────────────────────────────────────

.PHONY: all clean

all: $(ROM)
	@echo ""
	@echo "  Built: $(ROM)"
	@echo "  Data embedded via NitroFS (romfs/)"
	@echo ""

$(ROM): $(ELF) $(ARM7ELF)
	$(NDSTOOL) -c $@ \
		-7 $(ARM7ELF) \
		-9 $(ELF) \
		-b $(BLOCKSDS)/sys/icon.bmp \
		"$(GAME_TITLE);$(GAME_SUBTITLE);$(GAME_AUTHOR)" \
		-h 0x4000 \
		-uc 2 \
		-d romfs

$(ELF): $(OBJS)
	$(LD) -o $@ $(OBJS) $(LDFLAGS)

$(BUILDDIR)/%.o: %.c
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILDDIR) $(ROM)
