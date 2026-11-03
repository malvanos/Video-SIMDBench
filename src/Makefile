# Legacy Makefile build for VideoBench.
#
# CMake is the primary build system (see ../CMakeLists.txt):
#     cmake -S .. -B ../build && cmake --build ../build -j8 && ../build/vbench --bench
#
# This Makefile remains as a portable alternative:
#     make              # C kernels only (works on any POSIX platform)
#     make USE_ASM=1    # also assemble the x86 asm kernels on x86_64 Linux
#                       # (requires nasm or yasm; ignored elsewhere)

UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

CC      ?= cc
USE_ASM ?= 0

OPT      ?= -O3
WARN      = -Wall -Wno-sign-compare
CFLAGS   += $(OPT) $(WARN) --std=gnu99 -I./
LDFLAGS  += -lm

# x86_64 Linux is the only target where this Makefile assembles asm
IS_X86_64_LINUX := $(if $(filter Linux,$(UNAME_S)),$(if $(filter x86_64 amd64,$(UNAME_M)),1,),)

CSOURCES := $(wildcard *.c) $(wildcard c_kernels/*.c)
OBJECTS  := $(CSOURCES:.c=.o)
EXECUTABLE := bench

ASMSOURCES :=
ASMOBJECTS :=
ifneq ($(USE_ASM),0)
ifneq ($(IS_X86_64_LINUX),)
ASM_PROG := $(shell command -v nasm 2>/dev/null || command -v yasm 2>/dev/null)
ifneq ($(ASM_PROG),)
ASMSOURCES := $(wildcard asm/x86/*.asm)
ASMOBJECTS := $(ASMSOURCES:%.asm=%.o)
CSOURCES   += asm/x86/predict-c.c asm/x86/mc-c.c
OBJECTS    := $(CSOURCES:.c=.o)
ifeq ($(findstring nasm,$(ASM_PROG)),nasm)
ASMFLAGS := -I. -DARCH_X86_64=1 -f elf64 -w+orphan-labels -DSTACK_ALIGNMENT=32 -DHIGH_BIT_DEPTH=0 -DBIT_DEPTH=8
else
ASMFLAGS := -I. -DARCH_X86_64=1 -f elf64 -Worphan-labels -DSTACK_ALIGNMENT=32 -DHIGH_BIT_DEPTH=0 -DBIT_DEPTH=8
endif
endif
endif
endif

all: vbench_config.h $(EXECUTABLE)

# Generated platform config header (SYS_*/ARCH_*/HAVE_* macros) — the CMake
# build generates the same file from vbench_config.h.in via configure_file.
vbench_config.h: Makefile
	@set -e; \
	sw=0; sl=0; sm=0; \
	case "$(UNAME_S)" in \
	    Darwin) sm=1 ;; \
	    Linux) sl=1 ;; \
	    MINGW*|MSYS*|CYGWIN*) sw=1 ;; \
	esac; \
	a_x86=0; a_x64=0; a_arm=0; a_a64=0; a_ppc=0; a_mips=0; \
	case "$(UNAME_M)" in \
	    x86_64|amd64) a_x64=1 ;; \
	    i?86|x86) a_x86=1 ;; \
	    aarch64|arm64) a_a64=1 ;; \
	    arm*) a_arm=1 ;; \
	    powerpc*|ppc*) a_ppc=1 ;; \
	    mips*) a_mips=1 ;; \
	esac; \
	neon=0; armv6=0; \
	if [ $$a_arm = 1 ] || [ $$a_a64 = 1 ]; then neon=1; fi; \
	if [ $$a_arm = 1 ]; then armv6=1; fi; \
	asm=0; mmx=0; \
	if [ "$(USE_ASM)" != "0" ] && [ "$(IS_X86_64_LINUX)" = "1" ] && [ -n "$(ASM_PROG)" ]; then asm=1; mmx=1; fi; \
	stack=16; \
	if [ $$asm = 1 ]; then stack=32; fi; \
	printf '#ifndef VBENCH_CONFIG_H\n#define VBENCH_CONFIG_H\n' > $@; \
	for m in SYS_WINDOWS:$$sw SYS_LINUX:$$sl SYS_MACOSX:$$sm SYS_FREEBSD:0 SYS_OPENBSD:0 \
	         SYS_BEOS:0 SYS_CYGWIN:0 SYS_SunOS:0 ARCH_X86:$$a_x86 ARCH_X86_64:$$a_x64 \
	         ARCH_ARM:$$a_arm ARCH_AARCH64:$$a_a64 ARCH_PPC:$$a_ppc ARCH_MIPS:$$a_mips \
	         VBENCH_HAVE_ASM:$$asm HAVE_X86_ASM:$$asm HAVE_MMX:$$mmx HAVE_NEON:$$neon \
	         HAVE_ARMV6:$$armv6 HAVE_ALTIVEC:0 HAVE_MSA:0 HAVE_THREAD:0 HAVE_POSIXTHREAD:0 \
	         HAVE_CPU_COUNT:0; do \
	    n=$${m%%:*}; v=$${m##*:}; printf '#define %s %s\n' "$$n" "$$v" >> $@; \
	done; \
	printf '#define BIT_DEPTH 8\n#define HIGH_BIT_DEPTH 0\n#define STACK_ALIGNMENT %s\n#endif\n' $$stack >> $@; \
	echo "generated $@"

$(EXECUTABLE): $(OBJECTS) $(ASMOBJECTS)
	$(CC) $(OBJECTS) $(ASMOBJECTS) -o $@ $(LDFLAGS)

%.o: %.c vbench_config.h
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.asm
	$(ASM_PROG) $(ASMFLAGS) -o $@ $<

clean:
	rm -f $(OBJECTS) $(ASMOBJECTS) $(EXECUTABLE)

.PHONY: all clean
