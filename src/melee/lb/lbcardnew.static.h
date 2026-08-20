
#include <placeholder.h>
#include <platform.h>

#include <dolphin/card.h>
#include <dolphin/os/OSAlarm.h>
#include <sysdolphin/baselib/cobj.h>
#include <melee/lb/types.h>
#include <melee/sc/types.h>

struct lb_80432A68_t {
    /* 0x000 */ UNK_T MELEE_PC_PTR32 work_area;
    /* 0x004 */ UNK_T MELEE_PC_PTR32 lib_area;
    /* 0x008 */ int chan;
    /* 0x00C */ UNK_T MELEE_PC_PTR32 unk_C;
    /* 0x010 */ const char* MELEE_PC_PTR32 unk_10;
    /* 0x014 */ const char* MELEE_PC_PTR32 unk_14;
    /* 0x018 */ s32 unk_18;
    /* 0x01C */ s32 unk_1C;
    /* 0x020 */ lbCardNew_SnapshotEntry* MELEE_PC_PTR32 snapshot_entries;
    /* 0x024 */ int* MELEE_PC_PTR32 free_blocks;
    /* 0x028 */ int* MELEE_PC_PTR32 free_files;
    /* 0x02C */ char x2C[2];
    /* 0x02C */ char x2E;
    /* 0x02C */ char x2F[4];
    /* 0x034 */ s32 unk_34;
    /* 0x038 */ struct lb_80432A68_38_t unk_38[9];
    /* 0x080 */ s32 unk_80;
    /* 0x084 */ s32 memsize;
    /* 0x088 */ s32 sectorsize;
    /* 0x08C */ s32 unused_bytes;
    /* 0x090 */ s32 unused_files;
    /* 0x094 */ CARDFileInfo file_info;
    // 0x0A8 through 0x50C is a CardState: lbcardnew.c hands &unk_A8 to
    // hsd_803B29D8() as one, and xD0/xF4 are that state's x28[9] and x4C[9]
    // arrays reached from here. The padding is derived from CardState rather
    // than written out as 0xD0 - 0xAC and 0x50C - 0xF4 - 9 * 4, which are the
    // same numbers on GameCube but reserve a hole 40 bytes too small on a
    // 64-bit host, where CardState's two pointer members are twice as wide.
    // The card code then wrote its tail straight over x50C and the game jumped
    // through the result.
    /* 0x0A8 */ s32 unk_A8; // CardState::x0
    /* 0x0AC */ u8 pad_AC[offsetof(CardState, x28) - sizeof(s32)];
    /* 0x0D0 */ int xD0[9];  // CardState::x28
    /* 0x0F4 */ volatile int xF4[9]; // CardState::x4C
    /* 0x118 */ u8 pad_500[sizeof(CardState) - offsetof(CardState, x4C) -
                           9 * sizeof(int)];
    /* 0x50C */ LbCardNewDone MELEE_PC_PTR32 x50C;
    /* 0x510 */ struct CardTask {
        int x0;
        int x4;
        UNK_T MELEE_PC_PTR32 x8;
        char* MELEE_PC_PTR32 xC;
        char x10[0x20];
        u8 x18;
        char x19[7];
        u8 unk20[0x1C];
    } task_array[LbCardNewTaskArray_Max];
    /* 0x8AC */ int x8AC;
}; /* size = 0x8B0 */
STATIC_ASSERT(sizeof(struct lb_80432A68_t) == 0x8B0);
// STATIC_ASSERT above is inert on PC (platform.h disables it there for the GX
// types); these are live, and now pass, because every pointer field in this
// struct carries MELEE_PC_PTR32.
MELEE_PC_LAYOUT_ASSERT(sizeof(struct lb_80432A68_t) == 0x8B0);
MELEE_PC_LAYOUT_ASSERT(offsetof(struct lb_80432A68_t, unk_A8) == 0xA8);
MELEE_PC_LAYOUT_ASSERT(offsetof(struct lb_80432A68_t, x50C) == 0x50C);
// The embedded CardState has to fit its hole on every target. Exact on
// GameCube; on a 64-bit host x50C's own alignment can leave a few bytes spare,
// which is harmless -- what matters is that the state cannot reach x50C.
MELEE_PC_LAYOUT_ASSERT(offsetof(struct lb_80432A68_t, x50C) -
                           offsetof(struct lb_80432A68_t, unk_A8) >=
                       sizeof(CardState));

/* 432A68 */ static struct lb_80432A68_t lb_80432A68;
