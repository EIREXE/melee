#ifndef GALE01_3A949C
#define GALE01_3A949C

#include <placeholder.h>

#include <Gecko_setjmp.h>
#include <stddef.h>
#include <dolphin/card.h>

typedef struct CardFileData {
    u8* MELEE_PC_PTR32 ptr;
} CardFileData;

typedef struct CardState {
    /* 0x00 */ u8* MELEE_PC_PTR32 x0;
    /* 0x04 */ s32 x4;
    /* 0x08 */ u32 x8;
    /* 0x0C */ CARDFileInfo file_info;
    /* 0x20 */ s32 x20;
    /* 0x24 */ u32 x24;
    /* 0x28 */ int x28[9];
    /* 0x4C */ int x4C[9];
    /* 0x70 */ CardFileData x70[9];
    /* 0x94 */ u8 pad_94[0xDC];
    /* 0x170 */ s32 x170[64];
    /* 0x270 */ s32 x270[64];
    /* 0x370 */ u8 x370[0x40];
    /* 0x3B0 */ u8 x3B0;
    /* 0x3B1 */ u8 pad_3B1[1];
    /* 0x3B2 */ u8 icon_format[8];
    /* 0x3BA */ u8 icon_speed[8];
    /* 0x3C2 */ u8 pad_3C2[2];
    /* 0x3C4 */ CARDStat stat;
    /* 0x430 */ u8 digest[0x30];
    /* 0x460 */ s32 x460;
} CardState;

typedef struct {
    u8 x0[0x300];
} __baselib_UnkType003;

/// A callback the card queue holds in a 32-bit slot.
typedef void (*CardCallback)(s32, s32);

/// The ring of pending card commands, viewed as raw slots. hsd_3A94.c reaches
/// the same memory through this, through #CardCmd and through #CardContext, at
/// three different base offsets; all three only line up because every field is
/// one 32-bit slot and the stride is the same.
typedef struct CardBufEntry {
    s32 x0, x4, x8, xC;
    s32 x10;
    s32 x14, x18, x1C, x20;
} CardBufEntry;

typedef struct CardCmd {
    /* 0x00 */ s32 type;
    /* 0x04 */ CardState* MELEE_PC_PTR32 state;
    /* 0x08 */ s32 x8;
    /* 0x0C */ s32 xC;
    /* 0x10 */ s32 x10;
    /* 0x14 */ s32 x14;
    /* 0x18 */ void* MELEE_PC_PTR32 x18;
    /* 0x1C */ s32 x1C;
    /* 0x20 */ s32 x20;
} CardCmd;

typedef struct HsdCmdEntry {
    s32 type;
    s32 f1;
    s32 f2;
    s32 f3;
    s32 f4;
    s32 f5;
} HsdCmdEntry;

/// The whole card work area. On GameCube this is three adjacent .bss objects
/// -- hsd_804D1138 (0x10), hsd_804D1148 (0x1200) and hsd_804D2348 (0x300) --
/// and the card code walks straight across the boundaries: CMD_QUEUE() indexes
/// hsd_804D1138 + offsetof(CardContext, x1210), which lands in hsd_804D2348.
/// See hsd_4D11.c, which reserves the storage.
typedef struct CardContext {
    /* 0x0000 */ s32 x0;
    /* 0x0004 */ CardState* MELEE_PC_PTR32 x4;
    /* 0x0008 */ CardCallback MELEE_PC_PTR32 x8;
    /* 0x000C */ s32 xC;
    /* 0x0010 */ CardCmd x10[128];
    /* 0x1210 */ HsdCmdEntry x1210[32];
} CardContext;

/// Overlays CardContext::x1210, so it has to match HsdCmdEntry's stride.
typedef struct CardQueueEntry {
    /* 0x00 */ s32 x0;
    /* 0x04 */ s32 x4;
    /* 0x08 */ s32 x8;
    /* 0x0C */ s32 xC;
    /* 0x10 */ s32 x10;
    /* 0x14 */ CardCallback MELEE_PC_PTR32 x14;
} CardQueueEntry;

/// Everything above is aliased against everything else, so pin the shape the
/// GameCube build gets for free.
MELEE_PC_LAYOUT_ASSERT(sizeof(CardBufEntry) == 0x24);
MELEE_PC_LAYOUT_ASSERT(sizeof(CardCmd) == 0x24);
MELEE_PC_LAYOUT_ASSERT(sizeof(HsdCmdEntry) == 0x18);
MELEE_PC_LAYOUT_ASSERT(sizeof(CardQueueEntry) == sizeof(HsdCmdEntry));
MELEE_PC_LAYOUT_ASSERT(offsetof(CardContext, x4) == 0x04);
MELEE_PC_LAYOUT_ASSERT(offsetof(CardContext, x8) == 0x08);
MELEE_PC_LAYOUT_ASSERT(offsetof(CardContext, xC) == 0x0C);
MELEE_PC_LAYOUT_ASSERT(offsetof(CardContext, x10) == 0x10);
MELEE_PC_LAYOUT_ASSERT(offsetof(CardContext, x1210) == 0x1210);
MELEE_PC_LAYOUT_ASSERT(sizeof(CardContext) == 0x1510);
MELEE_PC_LAYOUT_ASSERT(sizeof(CardState) == 0x464);
MELEE_PC_LAYOUT_ASSERT(offsetof(CardState, x70) == 0x70);
MELEE_PC_LAYOUT_ASSERT(offsetof(CardState, x170) == 0x170);

/// @brief Start of the command queue, which is CardContext::x1210 reached from
/// a `u8*` to the base of the work area.
#define CMD_QUEUE(base)                                                       \
    ((HsdCmdEntry*) ((base) + offsetof(CardContext, x1210)))

/// Ring length of the command buffer and of the queue behind it.
#define CARD_CMD_COUNT (sizeof(((CardContext*) NULL)->x10) / sizeof(CardCmd))
#define CARD_QUEUE_COUNT                                                      \
    (sizeof(((CardContext*) NULL)->x1210) / sizeof(HsdCmdEntry))

/* 3AA790 */ s32 fn_803AA790(void);
/* 3AAA48 */ void hsd_803AAA48(void);
/* 3AC168 */ s32 fn_803AC168(s32* cmd_buf);
/* 3AC258 */ s32 fn_803AC258(CardState* card_state, s32 block_idx);
/* 3AC2A4 */ s32 fn_803AC2A4(CardState* card_state);
/* 3AC2D4 */ UNK_RET fn_803AC2D4(UNK_PARAMS);
/* 3AC2E0 */ void fn_803AC2E0(void);
/* 3AC334 */ UNK_RET fn_803AC334(UNK_PARAMS);
/* 3AC340 */ int hsd_803AC340(void* header);
/* 3AC3E0 */ void hsd_803AC3E0(struct CardState* file_desc, int file_idx,
                               int file_size, int file_flags, u8* data);
/* 3AC3F8 */ void fn_803AC3F8(void*, u8*, s32);
/* 3AC558 */ void hsd_803AC558(struct CardState*, u8*);
/* 3AC634 */ u32 fn_803AC634(struct CardState* file_desc, s32 file_idx);
/* 3AC6B8 */ s32 fn_803AC6B8(struct CardState* file_desc, s32 file_count);
/* 3AC7DC */ s32 fn_803AC7DC(CardState*);
/* 3ACBE8 */ s32 fn_803ACBE8(CardState* state, s32 block_idx);
/* 3ACC0C */ s32 fn_803ACC0C(CardState* state, s32 block_idx, s32 file_id,
                             s32 seq_num, void* expected_data, s32 data_size);
/* 3ACD58 */ s32 fn_803ACD58(CardState* state, void* icon_data,
                             void* file_data);
/* 3ACF30 */ s32 fn_803ACF30(CardState* state, s32 file_id, s32 seq_num,
                             s32 version);
/* 3ACFC0 */ s32 fn_803ACFC0(CardState* state, s32 block_idx, s32 file_id,
                             s32 seq_num, void* payload, s32 payload_size,
                             s32 version);
/* 3AD16C */ s32 fn_803AD16C(CardState* state);
/* 3ADE4C */ s32 fn_803ADE4C(s32 card_state, s32 channel, s32 callback);
/* 3ADF90 */ s32 fn_803ADF90(struct CardState*, s32, s32, s32, s32);
/* 3AE7F8 */ s32 fn_803AE7F8(struct CardState*, s32, s32, s32, s32);
/* 3AF3F0 */ s32 fn_803AF3F0(CardState* state, s32, s32, s32, s32);
/* 3B0120 */ s32 fn_803B0120(CardState* state, s32, s32, s32, s32);
/* 3B0E9C */ s32 fn_803B0E9C(struct CardState*, s32, s32, s32, s32);
/* 3B1338 */ s32 fn_803B1338(CardState* state, s32);
/* 3B1F78 */ s32 fn_803B1F78(CardState* state, s32 channel, s32 file_id,
                             s32 seq_num, s32 callback);
/* 3B21E8 */ s32 fn_803B21E8(s32 card_state, s32 file_id, s32 seq_num,
                             s32 callback);
/* 3B2374 */ void hsd_803B2374(void);
/* 3B24E4 */ void hsd_803B24E4(s32* ctx, int channel, int file_no,
                               void* work_buf);
/* 3B2550 */ int hsd_803B2550(s32*, const char*, void (*)(int, int));
/* 3B2674 */ s32 hsd_803B2674(CardState* state);
/* 3B26CC */ s32 fn_803B26CC(CardState* state, s32 file_id, s32 seq_num,
                             s32 version, void (*callback)(s32, s32));
// The three objects the card code walks as one CardContext. Adjacent in that
// order on GameCube; melee_compat/src/card_work.c reproduces the adjacency on
// PC, where hsd_4D11.c is excluded from the build.
/* 4D1138 */ extern u8 hsd_804D1138[0x10];
/* 4D1148 */ extern u32 hsd_804D1148[0x80][0x9];
/* 4D2348 */ extern __baselib_UnkType003 hsd_804D2348;
/* 4D2648 */ extern __jmp_buf hsd_804D2648;
/* 4D2E70 */ extern u8 hsd_804D2E70[2084];
/* 4D7990 */ extern s32 hsd_804D7990;
/* 4D7994 */ extern s32 hsd_804D7994;
/* 4D79A0 */ extern u8* hsd_804D79A0;
/* 4D79A4 */ extern u8* hsd_804D79A4;
/* 4D79A8 */ extern s32 hsd_804D79A8;
/* 4D79AC */ extern s32 hsd_804D79AC;
/* 4D79B0 */ extern u8 hsd_804D79B0;
/* 4D79B8 */ extern u8* hsd_804D79B8;
/* 4D79BC */ extern u8* hsd_804D79BC;
/* 4D79C0 */ extern s32 hsd_804D79C0;
/* 4D79C4 */ extern s32 hsd_804D79C4;
/* 4D79C8 */ extern u8 hsd_804D79C8;

#endif
