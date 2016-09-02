
# Preprocessor Flags:
#      - SYS_WINDOWS
#      - ARCH_ARM
#      - ARCH_X86
#      - ARCH_X86_64
#      - ARCH_PPC
#      - ARCH_AARCH64 
#      - ARCH_MIPS
#      - HAVE_X86_INLINE_ASM
#      - HAVE_MMX


### clang
#CC=clang-3.8 
#CFLAGS=-c -Wall -O3   -std=c99  -DARCH_X86_64=1 -DHAVE_MMX  -I./
#CFLAGS=-c -Wall -O3  -fno-vectorize    -std=c99  -DARCH_X86_64=1 -DHAVE_MMX  -I./


### icc
#CC=icc
#CFLAGS=-c -Wall -O3  -vec-report=5 -std=c99  -DARCH_X86_64=1 -DHAVE_MMX  -I./
#CFLAGS=-c -Wall -O3  -no-vec  -std=c99  -DARCH_X86_64=1 -DHAVE_MMX  -I./

### GCC
CC=gcc
CFLAGS=-c -Wall -O3 -mtune=core-avx2 -march=core-avx2 -ftree-vectorize  --std=gnu99 -DARCH_X86_64=1 -DHAVE_MMX  -I./
#CFLAGS=-c -Wall -O3 -fno-tree-vectorize  --std=gnu99 -DARCH_X86_64=1 -DHAVE_MMX  -I./


# -O5 is generic
LDFLAGS= -lm -O5
SOURCES=  asm/x86/predict-c.c  	\
	  asm/x86/mc-c.c 	\
          c_kernels/pixel.c	\
          c_kernels/predict.c	\
          c_kernels/quant.c	\
          c_kernels/dct.c	\
          c_kernels/mc.c	\
          c_kernels/deblock.c	\
          c_kernels/bitstream.c	\
          c_kernels/cabac.c	\
          main.c		\
          bench_pixel.c		\
          bench_dct.c		\
          bench_cabac.c		\
          bench_mc.c		\
          bench_intra.c		\
          bench_deblock.c	\
          bench_quant.c		\
          bench_bitstream.c	\
          bench.c		\
          cpu.c
          
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=bench

YASM=yasm
ASMSOURCES= asm/x86/cpu-a.asm 	\
	asm/x86/checkasm-a.asm	\
	asm/x86/const-a.asm	\
	asm/x86/predict-a.asm	\
 	asm/x86/pixel-a.asm	\
 	asm/x86/sad-a.asm	\
 	asm/x86/dct-a.asm	\
 	asm/x86/dct-64.asm	\
 	asm/x86/quant-a.asm	\
 	asm/x86/trellis-64.asm	\
 	asm/x86/cabac-a.asm	\
 	asm/x86/mc-a.asm	\
 	asm/x86/mc-a2.asm	\
 	asm/x86/deblock-a.asm	\
	asm/x86/bitstream-a.asm

ASMOBJECTS=$(ASMSOURCES:%.asm=%.o)

ASM=yasm
ASMFLAGS=-I.  -DARCH_X86_64=1 -f elf64 -Worphan-labels -DSTACK_ALIGNMENT=32 -DHIGH_BIT_DEPTH=0 -DBIT_DEPTH=8 



all: $(SOURCES) $(EXECUTABLE)
	    
$(EXECUTABLE): $(OBJECTS)  $(ASMOBJECTS)
	$(CC) $(OBJECTS) $(ASMOBJECTS)  -o $@  $(LDFLAGS)

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

%.o: %.asm 
	$(ASM) $(ASMFLAGS) -o $@ $<


assm: $(ASMSOURCES)
	echo "test"
	$(ASM) $(ASMFLAGS)   $^   -o $@

clean:
	rm -rf $(ASMOBJECTS) $(OBJECTS)


