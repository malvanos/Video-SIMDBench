
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


yasm -I.  -DARCH_X86_64=1 -f elf64 -Worphan-labels -DSTACK_ALIGNMENT=32 ./asm/x86/cpu-a.asm -o cpu-detect.o
yasm -I.  -DARCH_X86_64=1 -f elf64 -Worphan-labels -DSTACK_ALIGNMENT=32 ./asm/x86/checkasm-a.asm -o checkasm-a.o
yasm -I.  -DARCH_X86_64=1 -f elf64 -Worphan-labels -DSTACK_ALIGNMENT=32 -DBIT_DEPTH=8  ./asm/x86/const-a.asm -o const-a.o
yasm -I.  -DARCH_X86_64=1 -f elf64 -Worphan-labels -DSTACK_ALIGNMENT=32 -DHIGH_BIT_DEPTH=0 -DBIT_DEPTH=8 ./asm/x86/predict-a.asm -o predict-a.o

gcc -Wall --std=gnu99 -DARCH_X86_64=1 -DHAVE_MMX cpu-detect.o checkasm-a.o predict-a.o const-a.o ./asm/x86/predict-c.c ./c_kernels/pixel.c ./c_kernels/predict.c main.c bench_pixel.c bench.c cpu.c   -I./


