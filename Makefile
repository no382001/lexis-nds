BLOCKSDS            ?= /opt/blocksds/core
BLOCKSDSEXT         ?= /opt/blocksds/external
WONDERFUL_TOOLCHAIN ?= /opt/wonderful

-include corpus.mk
CORPUS_WORK  ?= iliad
CORPUS_LABEL ?= Homer, Iliad

NAME            := lexis-$(CORPUS_WORK)

GAME_TITLE      := lexis-nds
GAME_SUBTITLE   := Greek Text Reader - $(CORPUS_LABEL)
GAME_AUTHOR     := lexis-nds

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

DEFINES         := -include source/corpus_auto.h

ARM7ELF         := $(BLOCKSDS)/sys/arm7/main_core/arm7_minimal.elf
LIBS            := -lnds9 -lc
LIBDIRS         := $(BLOCKSDS)/libs/libnds

BUILDDIR        := build
ELF             := $(BUILDDIR)/$(NAME).elf
MAP             := $(BUILDDIR)/$(NAME).map
ROM             := $(NAME).nds

PREFIX          := $(WONDERFUL_TOOLCHAIN)/toolchain/gcc-arm-none-eabi/bin/arm-none-eabi-
CC              := $(PREFIX)gcc
LD              := $(PREFIX)gcc
OBJCOPY         := $(PREFIX)objcopy
NDSTOOL         := $(BLOCKSDS)/tools/ndstool/ndstool

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

OBJS            := $(addprefix $(BUILDDIR)/,$(notdir $(SOURCES_C:.c=.o)))
VPATH           := source

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
