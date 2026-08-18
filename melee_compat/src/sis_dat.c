// Host-layout conversion for SIS font blocks.
#include "compat_report.h"

#include <string.h>

#include <baselib/archive.h>
#include <dolphin/os.h>

void* malloc(size_t size);

// Generous upper bound so a block that does not follow the layout assumption
// fails as a diagnosable report rather than a runaway loop.
#define SIS_MAX_SLOTS 8192

// Converted blocks are memoised: HSD_SisLib_803A62A0 can be called more than
// once for the same font, and every caller has to see the same array or the
// pointer comparison in mnnamenew.c:455 stops matching.
#define SIS_CACHE_MAX 16

static struct {
    const void* gc;
    void* host;
} sis_cache[SIS_CACHE_MAX];
static int sis_cache_count;

void* melee_pc_sis_convert(HSD_Archive* archive, void* sis)
{
    const u8* data;
    const u32* slots;
    void** host;
    u32 datasz, off, limit, n, i;

    if (archive == NULL || sis == NULL) {
        return sis;
    }

    for (i = 0; i < (u32) sis_cache_count; i++) {
        if (sis_cache[i].gc == sis) {
            return sis_cache[i].host;
        }
    }

    data = (const u8*) archive->data;
    datasz = archive->header.data_size;
    if (data == NULL || (const u8*) sis < data ||
        (const u8*) sis >= data + datasz)
    {
        // Not archive-resident, so not in GameCube layout either.
        return sis;
    }

    off = (u32) ((const u8*) sis - data);
    slots = (const u32*) sis;

    // Walk until the current slot reaches the lowest target seen so far.
    limit = datasz;
    for (n = 0; n < SIS_MAX_SLOTS; n++) {
        u32 slot_off = off + n * 4;
        s32 delta = (s32) MELEE_PC_BE32(slots[n]);
        s64 target;

        if (slot_off >= limit) {
            break;
        }
        if (delta == 0) {
            continue; // an unrelocated slot: NULL, and it points nowhere
        }
        target = (s64) slot_off + delta;
        // <= datasz on purpose: a one-past-the-end marker is still in range.
        if (target >= 0 && target <= (s64) datasz) {
            if (target > (s64) slot_off && target < (s64) limit) {
                limit = (u32) target;
            }
        }
    }

    if (n == 0 || n >= SIS_MAX_SLOTS) {
        OSReport("melee_pc: SIS block at +0x%x has an implausible slot count "
                 "(%u); leaving it unconverted\n",
                 off, n);
        return sis;
    }

    host = malloc(n * sizeof(void*));
    if (host == NULL) {
        OSReport("melee_pc: out of memory converting a %u-slot SIS block\n", n);
        return sis;
    }

    for (i = 0; i < n; i++) {
        s32 delta = (s32) MELEE_PC_BE32(slots[i]);
        // Resolve against the slot's own address, matching the encoding the
        // loader wrote. Zero stays NULL, as everywhere else.
        host[i] = delta ? (void*) ((const u8*) &slots[i] + delta) : NULL;
    }

    if (sis_cache_count < SIS_CACHE_MAX) {
        sis_cache[sis_cache_count].gc = sis;
        sis_cache[sis_cache_count].host = host;
        sis_cache_count++;
    }

    return host;
}

// Slot index -> byte offset, for the places that index the block with a
// hardcoded GameCube byte offset (tylist.c). On GameCube a slot is 4 bytes;
// here it is sizeof(void*).
u32 melee_pc_sis_slot_offset(u32 gc_byte_offset)
{
    return (gc_byte_offset / 4) * (u32) sizeof(void*);
}
