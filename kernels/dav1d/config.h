/* Minimal hand-written replacement for the config.h that dav1d's meson build
 * normally generates. Only what the ported C kernels need: C-only build
 * (no asm, so the tmpl files skip their per-arch asm includes), 8bpc only,
 * no logging. ARCH_* default to 0 but defer to the harness build flags
 * (e.g. -DARCH_AARCH64=1) when those are already defined. */
#ifndef VBENCH_KERNELS_DAV1D_CONFIG_H
#define VBENCH_KERNELS_DAV1D_CONFIG_H

#define CONFIG_8BPC 1
#define CONFIG_16BPC 0
#define CONFIG_LOG 0
#define CONFIG_ASM_DEBUG 0
#define TRIM_DSP_FUNCTIONS 0

#define HAVE_ASM 0

#ifndef ARCH_AARCH64
#define ARCH_AARCH64 0
#endif
#ifndef ARCH_ARM
#define ARCH_ARM 0
#endif
#ifndef ARCH_X86
#define ARCH_X86 0
#endif
#ifndef ARCH_X86_64
#define ARCH_X86_64 0
#endif
#ifndef ARCH_X86_32
#define ARCH_X86_32 0
#endif
#ifndef ARCH_PPC64LE
#define ARCH_PPC64LE 0
#endif
#ifndef ARCH_RISCV
#define ARCH_RISCV 0
#endif
#ifndef ARCH_RV32
#define ARCH_RV32 0
#endif
#ifndef ARCH_RV64
#define ARCH_RV64 0
#endif
#ifndef ARCH_LOONGARCH
#define ARCH_LOONGARCH 0
#endif
#ifndef ARCH_LOONGARCH32
#define ARCH_LOONGARCH32 0
#endif
#ifndef ARCH_LOONGARCH64
#define ARCH_LOONGARCH64 0
#endif

#ifndef HAVE_AS_ARCH_DIRECTIVE
#define HAVE_AS_ARCH_DIRECTIVE 0
#endif
#ifndef HAVE_AS_FPU_DIRECTIVE
#define HAVE_AS_FPU_DIRECTIVE 0
#endif
#ifndef HAVE_AS_ARCH_LEVEL_DIRECTIVE
#define HAVE_AS_ARCH_LEVEL_DIRECTIVE 0
#endif

#endif /* VBENCH_KERNELS_DAV1D_CONFIG_H */
