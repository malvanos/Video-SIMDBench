
yasm -I.  -DARCH_X86_64=1 -f elf64 -Worphan-labels -DSTACK_ALIGNMENT=32 ./asm/x86/cpu-a.asm -o cpu-a.o

gcc -Wall --std=gnu99 -DARCH_X86_64=1 -DHAVE_MMX cpu-a.o ./c_kernels/pixel.c main.c bench_pixel.c bench.c cpu.c   -I./
