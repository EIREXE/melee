// Workaround for the HSD_Synth_804D6018
//
// synth.static.h defines the audio heap handle as private to synth.c, while initialize.c
// declares the same name `extern` and stores the heap it creates into it.
// They result in two different objects, so HSD_OSInit creates the audio heap and
// synth.c never sees it, the first HSD_AudioMalloc calls
// and gets nullptr.
//
// The real fix is to make that one shared global, which changes what the
// decomp asserts about the original symbol.

#include "compat_report.h"

// The prelude renames these; this file needs the real ones.
#undef OSAllocFromHeap
#undef OSFreeToHeap

#include <dolphin/os.h>

// Two properties are needed of these blocks, and only one of them is free.
//
// Alignment: OSAllocFromHeap hands back 32-byte-aligned blocks and the DMA
// layer relies on it -- HSD_DevComRequest asserts `dest % 32 == 0`. Host malloc
// only promises 16, so ask for the alignment explicitly.
//
// Address range: axdriver.c:873 narrows an audio-heap pointer to 32 bits when
// it relocates its own tables (`(u32) AXDriver_804D7798 & ~3u`), so a block
// above 4GB comes back truncated and the DVD layer writes to a garbage
// address. The cast is unsigned, so 4GB is the ceiling.
//
// Nothing here places the block. It comes out low because the executable is
// linked -no-pie and pc_main.c raises glibc's mmap threshold, which keeps
// allocations on the brk heap just above the image -- see the reasoning in
// cmake/melee_link.cmake. That is a property of the link and of the allocator
// tuning rather than of this call, so it is checked here instead of assumed.
//
// This replaced a hand-rolled mmap that walked candidate addresses looking for
// a free slot below 4GB and carried a shadow header so free() could unmap the
// right length. The search was only ever reproducing where brk already puts
// things, and it made this the one file in the compat layer that needed
// <sys/mman.h>.

// Declared here rather than by including <stdlib.h>: melee_compat/include
// carries MSL-shadowing headers for the game sources, and this file is built
// with them on the include path.
void* aligned_alloc(size_t alignment, size_t size);
void free(void* ptr);

#define AUDIO_ALIGN 32
#define AUDIO_CEILING 0x100000000ULL

static void* audio_alloc(size_t size)
{
    // C11 wants a size that is an integral multiple of the alignment.
    size_t rounded = (size + (AUDIO_ALIGN - 1)) & ~(size_t) (AUDIO_ALIGN - 1);
    void* p = aligned_alloc(AUDIO_ALIGN, rounded);

    if (p == NULL) {
        return NULL;
    }
    if ((uintptr_t) p + rounded > AUDIO_CEILING) {
        // Refusing is better than handing back a block whose address the
        // driver will truncate: the failure would otherwise surface as the DVD
        // layer writing somewhere unrelated, a long way from the cause.
        OSReport("melee_pc: audio heap block at %p is above the 4GB ceiling "
                 "axdriver relocation needs -- is the build still -no-pie?\n",
                 p);
        free(p);
        return NULL;
    }
    return p;
}

static void audio_free(void* p) { free(p); }

// Heap handles are indices, so a negative one was never initialised.
static bool bad_heap(OSHeapHandle heap) { return heap < 0; }

void* melee_pc_alloc_from_heap(OSHeapHandle heap, u32 size)
{
    if (bad_heap(heap)) {
        void* p;
        COMPAT_STUB();
        p = audio_alloc(size);
        if (p == NULL) {
            OSReport("melee_pc: audio heap fallback failed for %u bytes\n",
                     (unsigned) size);
        }
        return p;
    }
    return OSAllocFromHeap(heap, size);
}

void melee_pc_free_to_heap(OSHeapHandle heap, void* ptr)
{
    if (bad_heap(heap)) {
        audio_free(ptr);
        return;
    }
    OSFreeToHeap(heap, ptr);
}
