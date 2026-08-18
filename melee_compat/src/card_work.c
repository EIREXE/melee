// The memory-card work area, laid out as one contiguous block.
//
// hsd_4D11.c reserves these as five separate .bss objects, and the card code
// walks straight across the boundaries: it takes hsd_804D1138 as the base of a
// CardContext and indexes past the end of it, so hsd_804D1148 is that context's
// command ring and hsd_804D2348 is the queue at offsetof(CardContext, x1210).
// On GameCube that works because the linker placed them back to back in that
// order. Nothing makes separate objects adjacent on a PC, so hsd_4D11.c is
// excluded from this build (cmake/pc_excluded.cmake) and the run is reproduced
// here as one object with the original names aliased into it.
//
// The offsets come from the structures rather than from a table of constants.
// Only the .set directives still spell them out, because file-scope asm cannot
// call offsetof -- so every literal below is checked against the struct it is
// supposed to describe, and a field that changes width breaks the build here
// instead of corrupting the queue at runtime.

#include <baselib/hsd_3A94.h>

#include <stddef.h>

typedef struct MeleePcCardWork {
    /* 0x0000 */ CardContext ctx;  // hsd_804D1138 / _1148 / _2348
    /* 0x1510 */ u8 jpeg[0x828];   // hsd_804D2648, a JpegWork (hsd_3B34.c)
    /* 0x1D38 */ u8 x2E70[2084];   // hsd_804D2E70
} MeleePcCardWork;

// Aligned well past anything in here.
MeleePcCardWork melee_pc_card_work __attribute__((aligned(32)));

#define CARD_WORK_OFF(path) offsetof(MeleePcCardWork, path)

_Static_assert(CARD_WORK_OFF(ctx) == 0x0000, "hsd_804D1138 offset");
_Static_assert(CARD_WORK_OFF(ctx.x10) == 0x0010, "hsd_804D1148 offset");
_Static_assert(CARD_WORK_OFF(ctx.x1210) == 0x1210, "hsd_804D2348 offset");
_Static_assert(CARD_WORK_OFF(jpeg) == 0x1510, "hsd_804D2648 offset");
_Static_assert(CARD_WORK_OFF(x2E70) == 0x1D38, "hsd_804D2E70 offset");
_Static_assert(sizeof(MeleePcCardWork) == 0x255C, "card work area size");

// clang-format off
__asm__(".globl hsd_804D1138\n\t.set hsd_804D1138, melee_pc_card_work+0x0000");
__asm__(".globl hsd_804D1148\n\t.set hsd_804D1148, melee_pc_card_work+0x0010");
__asm__(".globl hsd_804D2348\n\t.set hsd_804D2348, melee_pc_card_work+0x1210");
__asm__(".globl hsd_804D2648\n\t.set hsd_804D2648, melee_pc_card_work+0x1510");
__asm__(".globl hsd_804D2E70\n\t.set hsd_804D2E70, melee_pc_card_work+0x1D38");
// clang-format on

// The eight scalars hsd_4D11.c defines alongside the work area, at 0x4D7980
// through 0x4D799C. Nothing indexes across these; they are here only because
// their TU is excluded.
#define CARD_STATE_SCALARS 8

u8 melee_pc_card_state[CARD_STATE_SCALARS * sizeof(s32)]
    __attribute__((aligned(32)));

// clang-format off
__asm__(".globl hsd_804D7980\n\t.set hsd_804D7980, melee_pc_card_state+0x00");
__asm__(".globl hsd_804D7984\n\t.set hsd_804D7984, melee_pc_card_state+0x04");
__asm__(".globl hsd_804D7988\n\t.set hsd_804D7988, melee_pc_card_state+0x08");
__asm__(".globl hsd_804D798C\n\t.set hsd_804D798C, melee_pc_card_state+0x0C");
__asm__(".globl hsd_804D7990\n\t.set hsd_804D7990, melee_pc_card_state+0x10");
__asm__(".globl hsd_804D7994\n\t.set hsd_804D7994, melee_pc_card_state+0x14");
__asm__(".globl hsd_804D7998\n\t.set hsd_804D7998, melee_pc_card_state+0x18");
__asm__(".globl hsd_804D799C\n\t.set hsd_804D799C, melee_pc_card_state+0x1C");
// clang-format on
