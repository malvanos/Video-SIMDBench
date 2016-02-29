
# 
# Flags:
#      - SYS_WINDOWS
#      - ARCH_ARM
#      - ARCH_X86
#      - ARCH_X86_64
#      - ARCH_PPC
#      - ARCH_AARCH64 
#      - ARCH_MIPS
#      - HAVE_X86_INLINE_ASM
#      - HAVE_MMX
#

# set the echo on
set -x 

#yasm -I.  -DARCH_X86_64=1 -f elf64 -Worphan-labels -DSTACK_ALIGNMENT=32 -DHIGH_BIT_DEPTH=0 -DBIT_DEPTH=8 ./asm/x86/sad16-a.asm -o sad16-a.o



yasm -I.  -DARCH_X86_64=1 -f elf64 -Worphan-labels -DSTACK_ALIGNMENT=32 ./asm/x86/cpu-a.asm -o cpu-detect.o
yasm -I.  -DARCH_X86_64=1 -f elf64 -Worphan-labels -DSTACK_ALIGNMENT=32 ./asm/x86/checkasm-a.asm -o checkasm-a.o
yasm -I.  -DARCH_X86_64=1 -f elf64 -Worphan-labels -DSTACK_ALIGNMENT=32 -DBIT_DEPTH=8  ./asm/x86/const-a.asm -o const-a.o
yasm -I.  -DARCH_X86_64=1 -f elf64 -Worphan-labels -DSTACK_ALIGNMENT=32 -DHIGH_BIT_DEPTH=0 -DBIT_DEPTH=8 ./asm/x86/predict-a.asm -o predict-a.o
yasm -I.  -DARCH_X86_64=1 -f elf64 -Worphan-labels -DSTACK_ALIGNMENT=32 -DHIGH_BIT_DEPTH=0 -DBIT_DEPTH=8 ./asm/x86/pixel-a.asm -o pixel-a.o
yasm -I.  -DARCH_X86_64=1 -f elf64 -Worphan-labels -DSTACK_ALIGNMENT=32 -DHIGH_BIT_DEPTH=0 -DBIT_DEPTH=8 ./asm/x86/sad-a.asm -o sad-a.o
yasm -I.  -DARCH_X86_64=1 -f elf64 -Worphan-labels -DSTACK_ALIGNMENT=32 -DHIGH_BIT_DEPTH=0 -DBIT_DEPTH=8 ./asm/x86/dct-a.asm -o dct-a.o
yasm -I.  -DARCH_X86_64=1 -f elf64 -Worphan-labels -DSTACK_ALIGNMENT=32 -DHIGH_BIT_DEPTH=0 -DBIT_DEPTH=8 ./asm/x86/dct-64.asm -o dct-64.o
yasm -I.  -DARCH_X86_64=1 -f elf64 -Worphan-labels -DSTACK_ALIGNMENT=32 -DHIGH_BIT_DEPTH=0 -DBIT_DEPTH=8 ./asm/x86/quant-a.asm -o quant-a.o
yasm -I.  -DARCH_X86_64=1 -f elf64 -Worphan-labels -DSTACK_ALIGNMENT=32 -DHIGH_BIT_DEPTH=0 -DBIT_DEPTH=8 ./asm/x86/trellis-64.asm -o trellis-64.o
yasm -I.  -DARCH_X86_64=1 -f elf64 -Worphan-labels -DSTACK_ALIGNMENT=32 -DHIGH_BIT_DEPTH=0 -DBIT_DEPTH=8 ./asm/x86/cabac-a.asm -o cabac-a.o
yasm -I.  -DARCH_X86_64=1 -f elf64 -Worphan-labels -DSTACK_ALIGNMENT=32 -DHIGH_BIT_DEPTH=0 -DBIT_DEPTH=8 ./asm/x86/mc-a.asm -o mc-a.o


gcc -g -Wall -O0  -march=corei7 --std=gnu99 -DARCH_X86_64=1 -DHAVE_MMX \
    cpu-detect.o checkasm-a.o predict-a.o const-a.o pixel-a.o  sad-a.o dct-a.o dct-64.o quant-a.o trellis-64.o  cabac-a.o mc-a.o \
    ./asm/x86/predict-c.c ./asm/x86/mc-c.c \
    ./c_kernels/pixel.c ./c_kernels/predict.c ./c_kernels/quant.c  ./c_kernels/dct.c  \
    main.c bench_pixel.c  bench_dct.c bench_cabac.c bench.c cpu.c   -I./ -lm

#gcc -g -Wall -O3  -std=c99 -DARCH_X86_64=1 -DHAVE_MMX cpu-detect.o checkasm-a.o predict-a.o const-a.o pixel-a.o  sad-a.o  \
#    ./asm/x86/predict-c.c ./c_kernels/pixel.c ./c_kernels/predict.c main.c bench_pixel.c bench.c cpu.c   -I./


