#if defined(__i386__)

#define f_QNAN 0xffc00000
#define d_QNAN0 0x0
#define d_QNAN1 0xfff80000
#define ld_QNAN0 0x0
#define ld_QNAN1 0xc0000000
#define ld_QNAN2 0xffff
#define ld_QNAN3 0x0
#define ldus_QNAN0 0x0
#define ldus_QNAN1 0x0
#define ldus_QNAN2 0x0
#define ldus_QNAN3 0xc000
#define ldus_QNAN4 0xffff

#else
#error Unsupported gdtoa arch; run qnan.c
#endif