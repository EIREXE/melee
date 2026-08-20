#ifndef MELEE_COMPAT_THP_H
#define MELEE_COMPAT_THP_H
#include <dolphin/thp.h>
typedef struct {
  s32 val0;
  u16 val1;
  u16 _pad;
  u8 val2;
} THPDec_8032FD40_Data;

s32 THPVideoDecode(void* file, void* tileY, void* tileU, void* tileV, void* work);
s32 THPDec_803302EC(u8** data);
s32 THPDec_8032FD40(THPDec_8032FD40_Data* arg0, u16 arg1);
s32 THPDec_8032F8D4(u32 file, void* out);
u8 THPDec_80330158(THPFileInfo* info);
void THPDec_80331340(s32, void*, void*, void*);
void THPDec_803313D0(s32, void*, void*, void*, u32);
void THPDec_803300E0(u32* data);
BOOL THPInit(void);
#endif
