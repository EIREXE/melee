// Deferred completion callbacks for the disc.

#include "compat_report.h"

// The prelude points this at us; here we want aurora's.
#undef DVDReadAsyncPrio

#include <dolphin/dvd.h>
#include <dolphin/os.h>

void melee_pc_dvd_drain(void);

// One in flight per DVDFileInfo. devcom.c serialises its own reads behind
// HSD_DevCom_804D77F5 and reuses a single static fileinfo; axdriver.c has its
// own. Four covers both with room to spare.
#define MAX_INFLIGHT 4
#define MAX_PENDING 8

typedef struct {
    DVDFileInfo* info;
    DVDCallback cb;
} DvdInflight;

typedef struct {
    DVDCallback cb;
    DVDFileInfo* info;
    s32 result;
} DvdPending;

static DvdInflight inflight[MAX_INFLIGHT];
static DvdPending pending[MAX_PENDING];
static u32 pending_count;

// Held across every touch of the two tables above. The worker thread only ever
// takes it inside the trampoline, so the game thread never waits long, and
// nothing here can block -- which matters because the game thread reaches the
// drain from inside a busy-wait.
static volatile int lock_flag;

static void dvd_lock(void)
{
    while (__atomic_test_and_set(&lock_flag, __ATOMIC_ACQUIRE)) {
        __builtin_ia32_pause();
    }
}

static void dvd_unlock(void)
{
    __atomic_clear(&lock_flag, __ATOMIC_RELEASE);
}

// Runs on aurora's DVD worker thread. Does nothing but hand the result over.
static void dvd_tramp(s32 result, DVDFileInfo* info)
{
    DVDCallback cb = NULL;
    int i;

    dvd_lock();
    for (i = 0; i < MAX_INFLIGHT; i++) {
        if (inflight[i].info == info) {
            cb = inflight[i].cb;
            inflight[i].info = NULL;
            inflight[i].cb = NULL;
            break;
        }
    }
    if (cb != NULL) {
        if (pending_count < MAX_PENDING) {
            pending[pending_count].cb = cb;
            pending[pending_count].info = info;
            pending[pending_count].result = result;
            pending_count++;
        } else {
            // Dropping one would hang the game on a flag that never gets set,
            // so say so rather than let it look like the original deadlock.
            OSReport("melee_pc: DVD completion queue overflow\n");
        }
    }
    dvd_unlock();
}

// Callbacks queue further reads, so each entry is copied out and the slot
// released before it runs.
void melee_pc_dvd_drain(void)
{
    for (;;) {
        DvdPending p;
        u32 i;

        dvd_lock();
        if (pending_count == 0) {
            dvd_unlock();
            return;
        }
        p = pending[0];
        pending_count--;
        for (i = 0; i < pending_count; i++) {
            pending[i] = pending[i + 1];
        }
        dvd_unlock();

        if (p.cb != NULL) {
            p.cb(p.result, p.info);
        }
    }
}

BOOL melee_pc_dvd_read_async_prio(DVDFileInfo* info, void* addr, s32 length,
                                  s32 offset, DVDCallback callback, s32 prio)
{
    int i;
    int slot = -1;

    if (callback == NULL) {
        // Nothing to defer; aurora's own path is fine.
        return DVDReadAsyncPrio(info, addr, length, offset, NULL, prio);
    }

    // Claimed before the read is posted: the worker can complete and call the
    // trampoline before DVDReadAsyncPrio() has even returned.
    dvd_lock();
    for (i = 0; i < MAX_INFLIGHT; i++) {
        if (inflight[i].info == NULL) {
            slot = i;
            break;
        }
    }
    if (slot >= 0) {
        inflight[slot].info = info;
        inflight[slot].cb = callback;
    }
    dvd_unlock();

    if (slot < 0) {
        OSPanic(__FILE__, __LINE__,
                "melee_pc: more than %d DVD reads in flight", MAX_INFLIGHT);
    }

    return DVDReadAsyncPrio(info, addr, length, offset, dvd_tramp, prio);
}
