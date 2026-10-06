BUILD_DIR := build
MOD_NAME  := REO_ModernCam

# Allow the user to specify the compiler and linker on macOS
# as Apple Clang does not support MIPS architecture
ifeq ($(shell uname),Darwin)
    CC      ?= clang
    LD      ?= ld.lld
else
    CC      := clang
    LD      := ld.lld
endif

TARGET  := $(BUILD_DIR)/mod.elf
NRM     := $(BUILD_DIR)/$(MOD_NAME).nrm
SYMS    := syms/reo.syms.toml

ifeq ($(OS),Windows_NT)
RECOMP_MOD_TOOL := RecompModTool.exe
else
RECOMP_MOD_TOOL := ./RecompModTool
endif

LDSCRIPT := mod.ld
CFLAGS   := -target mips -mips2 -mabi=32 -O2 -G0 -mno-abicalls -mno-odd-spreg -mno-check-zero-division \
			-fomit-frame-pointer -ffast-math -fno-unsafe-math-optimizations -fno-builtin-memset \
			-Wall -Wextra -Wno-incompatible-library-redeclaration -Wno-unused-parameter -Wno-unknown-pragmas -Wno-unused-variable \
			-Wno-missing-braces -Wno-unsupported-floating-point-opt -Werror=section
CPPFLAGS := -nostdinc -D_LANGUAGE_C -DMIPS -I include
LDFLAGS  := -nostdlib -T $(LDSCRIPT) -Map $(BUILD_DIR)/mod.map --unresolved-symbols=ignore-all --emit-relocs -e 0 --no-nmagic

C_SRCS := $(wildcard src/*.c)
C_OBJS := $(addprefix $(BUILD_DIR)/, $(C_SRCS:.c=.o))
C_DEPS := $(addprefix $(BUILD_DIR)/, $(C_SRCS:.c=.d))

all: $(NRM)

$(NRM): $(TARGET) mod.toml thumb.png $(SYMS)
	$(RECOMP_MOD_TOOL) mod.toml $(BUILD_DIR)

$(SYMS):
	$(error Missing $(SYMS): copy the function reference symbols for this build there (see README))

$(TARGET): $(C_OBJS) $(LDSCRIPT) | $(BUILD_DIR)
	$(LD) $(C_OBJS) $(LDFLAGS) -o $@

$(BUILD_DIR) $(BUILD_DIR)/src:
ifeq ($(OS),Windows_NT)
	mkdir $(subst /,\,$@)
else
	mkdir -p $@
endif

$(C_OBJS): $(BUILD_DIR)/%.o : %.c | $(BUILD_DIR) $(BUILD_DIR)/src
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -MMD -MF $(@:.o=.d) -c -o $@

install: $(NRM)
ifeq ($(MODS_DIR),)
	$(error Set MODS_DIR to the recomp's mods folder, e.g. make install MODS_DIR=path/to/mods)
endif
	mkdir -p "$(MODS_DIR)" && cp $(NRM) "$(MODS_DIR)"

clean:
	rm -rf $(BUILD_DIR)

-include $(C_DEPS)

.PHONY: all install clean
