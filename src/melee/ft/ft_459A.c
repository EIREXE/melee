#include "ft/types.h"

UnkCostumeStruct ft_8045A6A8;
UnkCostumeStruct ft_8045A690;
UnkCostumeStruct ft_8045A678;
UnkCostumeStruct ft_8045A660;
UnkCostumeStruct ft_8045A648;
UnkCostumeStruct ft_8045A630;
UnkCostumeStruct ft_8045A5B8[5];
UnkCostumeStruct ft_8045A540[5];
UnkCostumeStruct ft_8045A4E0[4];
UnkCostumeStruct ft_8045A480[4];
UnkCostumeStruct ft_8045A420[4];
UnkCostumeStruct ft_8045A3A8[5];
UnkCostumeStruct ft_8045A330[5];
UnkCostumeStruct ft_8045A2D0[4];
UnkCostumeStruct ft_8045A270[4];
UnkCostumeStruct ft_8045A1F8[5];
// ft_0877.h declares this one as `HSD_Joint* ft_8045A1E0[6]`, the same 24
// bytes on GameCube, but on the host the two views disagree on both size (40
// vs 48) and alignment. ftPr_Init_8013C2F8 clears it through the array view
// and clang vectorises that into 16-byte `movaps` stores, which fault on an
// object clang only had to align to 8. Over-align the definition so the
// array view's stores are valid.
MELEE_PC_ALIGN16 UnkCostumeStruct ft_8045A1E0;
UnkCostumeStruct ft_8045A168[5];
UnkCostumeStruct ft_8045A0F0[5];
UnkCostumeStruct ft_8045A090[4];
UnkCostumeStruct ft_8045A000[6];
UnkCostumeStruct ft_80459F88[5];
UnkCostumeStruct ft_80459F28[4];
UnkCostumeStruct ft_80459EC8[4];
UnkCostumeStruct ft_80459E68[4];
UnkCostumeStruct ft_80459DF0[5];
UnkCostumeStruct ft_80459D90[4];
UnkCostumeStruct ft_80459D18[5];
UnkCostumeStruct ft_80459CA0[5];
UnkCostumeStruct ft_80459C10[6];
u8 ft_80459B88[0x88];
UnkCostumeStruct ft_80459B28[4];
UnkCostumeStruct ft_80459A98[6];
