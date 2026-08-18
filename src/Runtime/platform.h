#ifndef RUNTIME_PLATFORM_H
#define RUNTIME_PLATFORM_H

#include <stdbool.h>       // IWYU pragma: export
#include <stddef.h>        // IWYU pragma: export
#include <dolphin/types.h> // IWYU pragma: export

/// @typedef bool
/// @note Dolphin's #BOOL macro is not supported.
/// @typedef BOOL
/// @deprecated Use #bool instead.
#undef BOOL

/// @def false
/// @note Dolphin's #FALSE macro is not supported.
/// @def FALSE
/// @deprecated Use #false instead.
#undef FALSE

/// @def true
/// @note Dolphin's #TRUE macro is not supported.
/// @def TRUE
/// @deprecated Use #true instead.
#undef TRUE

/// The underlying type of an @c enum, used as a placeholder
typedef int enum_t;

/// A @c void callback with no arguments.
typedef void (*Event)(void);

typedef bool (*Predicate)(void);

#if defined(__MWERKS__) && defined(__PPCGEKKO__)
#define MWERKS_GEKKO
#endif

#ifndef ATTRIBUTE_ALIGN
#if defined(__MWERKS__) || defined(__GNUC__)
#define ATTRIBUTE_ALIGN(num) __attribute__((aligned(num)))
#elif defined(_MSC_VER)
#define ATTRIBUTE_ALIGN(num)
#else
#error unknown compiler
#endif
#endif

#ifndef SECTION_INIT
#if defined(__MWERKS__) && !defined(M2CTX)
#define SECTION_INIT __declspec(section ".init")
#else
#define SECTION_INIT
#endif
#endif

#ifndef SECTION_CTORS
#if defined(__MWERKS__) && !defined(M2CTX)
#define SECTION_CTORS __declspec(section ".ctors")
#else
#define SECTION_CTORS
#endif
#endif

#ifndef SECTION_DTORS
#if defined(__MWERKS__) && !defined(M2CTX)
#define SECTION_DTORS __declspec(section ".dtors")
#else
#define SECTION_DTORS
#endif
#endif

#ifndef ATTRIBUTE_NORETURN
#if defined(__clang__) || defined(__GNUC__)
#define ATTRIBUTE_NORETURN __attribute__((noreturn))
#else
#define ATTRIBUTE_NORETURN
#endif
#endif

#ifndef ATTRIBUTE_RESTRICT
#if defined(__MWERKS__) && !defined(M2CTX)
#define ATTRIBUTE_RESTRICT __restrict
#else
#define ATTRIBUTE_RESTRICT
#endif
#endif

#ifndef AT_ADDRESS
#ifdef PERMUTER
#define AT_ADDRESS(x) = FIXEDADDR(x)
#elif defined(__MWERKS__) && !defined(M2CTX)
#define AT_ADDRESS(x) : (x)
#else
#define AT_ADDRESS(x)
#endif
#endif

#ifdef __PPCGEKKO__
#define qr0 0
#define qr1 1
#define qr2 2
#define qr3 3
#define qr4 4
#define qr5 5
#define qr6 6
#define qr7 7
#endif

#define U8_MAX 0xFF
#define U16_MAX 0xFFFF
#define U32_MAX 0xFFFFFFFF
#define S8_MAX 0x7F
#define S16_MAX 0x7FFF
#define S32_MAX 0x7FFFFFFF
#define F32_MAX 3.4028235e38f

#define SQ(x) ((x) * (x))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#ifdef __cplusplus
#ifndef _Static_assert
#define _Static_assert static_assert
#endif
#endif
#ifdef M2CTX
#define STATIC_ASSERT(cond)
#elif defined(MELEE_PC)
// The PC build cannot satisfy these. They pin struct layouts to the GameCube
// ABI, but aurora deliberately redefines the opaque GX types (GXTexObj and
// friends) with different sizes for its PC backend, so any melee struct
// embedding one changes size
#define STATIC_ASSERT(cond)
#elif defined(__MWERKS__)
#define STATIC_ASSERT(cond)                                                   \
    struct {                                                                  \
        int x[1 - 2 * !(cond)];                                               \
    };
#else
#define STATIC_ASSERT(cond) _Static_assert((cond), "(" #cond ") failed")
#endif

#ifdef MELEE_PC
#include <melee_pc_dat_types.h> // IWYU pragma: export

/// @brief Reads or writes a 32-bit value that is stored big-endian, as
/// everything inside a DAT archive is. Identity on the GameCube, which is
/// big-endian already.
#define MELEE_PC_BE32(x) __builtin_bswap32(x)
#define MELEE_PC_BE16(x) __builtin_bswap16(x)

/// @brief Resolves one archive pointer slot, storing the result as a signed
/// byte offset *from the slot itself* rather than as an address.
///
/// On GameCube a relocated slot simply holds the address, because a pointer is
/// four bytes there. On a 64-bit host it cannot: writing an address into a
/// four-byte slot is what forces the whole archive below 4GB. A self-relative
/// delta has no such ceiling -- it stays valid wherever the buffer is mapped,
/// because it never contains an address at all.
///
/// Zero keeps meaning NULL. A slot absent from the relocation table stays zero
/// and must decode as NULL, so the encoding must never produce a zero delta
/// for a real pointer -- which it cannot, since that would need a slot holding
/// its own address.
///
/// Only the DAT converter reads these slots back (melee_compat/src/dat_reloc.c,
/// which also defines this); code that reads archive memory in place has to be
/// converted first.
void melee_pc_dat_store_ptr(void* slot, const void* target);

/// @brief Byte-swaps a DAT archive header in place. Nothing elsewhere.
struct HSD_ArchiveHeader;
void melee_pc_archive_header_be(struct HSD_ArchiveHeader* h);
#define MELEE_PC_ARCHIVE_HEADER_BE(h) melee_pc_archive_header_be(h)

/// @brief Converts a structure read straight out of a DAT archive into host
/// layout: byte order, field offsets and struct size all differ from the
/// GameCube's. Idempotent, so nested loads that pass an already-converted
/// pointer are left alone. Expands to the pointer unchanged everywhere else.
void* melee_pc_dat_root(const void* p, int type);
#define MELEE_PC_DAT(T, p) ((p) = melee_pc_dat_root((p), DAT_T_##T))

/// @brief As #MELEE_PC_DAT, for a symbol that is a NULL-terminated *table* of
/// pointers rather than a single structure (LightList** and friends).
void* melee_pc_dat_root_ptrnull(const void* p, int type);
#define MELEE_PC_DAT_PTRNULL(T, p)                                            \
    ((p) = melee_pc_dat_root_ptrnull((p), DAT_T_##T))

/// @brief True when an address is NOT main RAM -- i.e. an ARAM or physical
/// address. On GameCube main RAM starts at 0x80000000, so anything below that
/// is not it. On PC, MEM1 is mapped below 2GB (so that pointers round-trip
/// through the game's signed 32-bit slots), which makes the literal comparison
/// classify every main-RAM address as ARAM. Ask the compat layer where MEM1
/// actually is instead.
bool melee_pc_in_mem1(const void* p);
#define MELEE_PC_IS_NOT_MAINRAM(a)                                            \
    (!melee_pc_in_mem1((const void*) (uintptr_t) (a)))
#define MELEE_PC_IS_MAINRAM(a)                                                \
    (melee_pc_in_mem1((const void*) (uintptr_t) (a)))

/// @brief 32-byte alignment for objects used as DMA destinations.
/// HSD_DevComRequest asserts `dest % 32 == 0` (devcom.c:411). On GameCube
/// these objects are 32-byte aligned by their fixed addresses; on PC the
/// compiler only aligns them to what the type needs. Expands to nothing
/// elsewhere, so the matching build's text is unchanged.
#define MELEE_PC_ALIGN32 ATTRIBUTE_ALIGN(32)

/// @brief Tells the PC build's relocating DAT loader that an archive's memory
/// is going away, so it can drop the structures it converted out of it.
/// Expands to nothing everywhere else.
void melee_pc_dat_forget(const void* base, u32 size);
#define MELEE_PC_ON_ARCHIVE_FREE(base, size)                                  \
    melee_pc_dat_forget((base), (size))

/// @brief Runs one round of simulated interrupt delivery: ARAM DMA, memory
/// card, alarms and vertical retrace. Expands to nothing everywhere else,
/// because on the GameCube the hardware does this on its own.
///
/// Only needed inside the game's busy-wait loops. Elsewhere the compat layer
/// delivers from OSRestoreInterrupts(), which is the closest thing a PC has to
/// the moment the CPU becomes interruptible again. A loop like
/// lbfile.c's waitForDisc() never touches the interrupt state, though, so
/// nothing there is ever delivered and the completion it is waiting for cannot
/// arrive.
void melee_pc_pump(void);
#define MELEE_PC_PUMP() melee_pc_pump()

/// @brief Qualifies a pointer *field* so it occupies the 32-bit slot the
/// GameCube gave it. Expands to nothing everywhere else, where a pointer is
/// already 32 bits wide.
///
/// Written after the `*`, in the position a qualifier goes:
///
///     CardState* MELEE_PC_PTR32 x4;   // 4 bytes on every target
///
/// The point is that nothing else has to change. The compiler narrows on
/// store and widens on load, so pointer arithmetic, indexing, assignment from
/// an ordinary pointer, null tests and `&field` all keep working as written --
/// which matters because these fields are read in hundreds of places that the
/// matching build must keep verbatim.
///
/// Use it on any struct whose layout the game depends on: ones it reinterprets
/// through a differently-typed pointer, indexes with a stride the hardware
/// build pinned, or overlays on a fixed run of memory. A 64-bit pointer field
/// silently moves every field after it and changes the struct's stride, which
/// is invisible right up until an aliasing cast lands in the wrong place.
///
/// Safe because the PC build keeps everything the game round-trips through a
/// 32-bit slot below 4GB: the image via -no-pie (cmake/melee_link.cmake), MEM1
/// via AllocMEM1 in aurora's OSMemory.cpp, and the audio heap via
/// melee_compat/src/audio_heap.c. An address above that would truncate
/// silently, but so would the `(s32) ptr` casts the game itself is full of.
///
/// One restriction: LLVM cannot emit a call *through* a 32-bit function
/// pointer. Widen it first, which costs a cast that is a no-op on the
/// GameCube and so needs no macro of its own:
///
///     ((CardCallback) ctx->x8)(ctx->xC, result);
///
/// Storing, comparing and passing one are all fine as they are.
#if !defined(__clang__)
// GCC has no equivalent: its named address spaces are per-target and x86 only
// exposes the segment ones. Without a 32-bit pointer qualifier these fields
// silently double in width and every overlaid struct in the tree is wrong, so
// fail here rather than at some unrelated address at runtime. The PC build
// already requires clang regardless -- see -finline-hint-functions in
// cmake/melee_pc.cmake, which GCC rejects outright.
#error "the melee PC build requires clang: no 32-bit pointer qualifier"
#endif
#define MELEE_PC_PTR32 __ptr32

/// @brief Pins a GameCube struct layout at compile time. Unlike #STATIC_ASSERT
/// this one is live on PC and inert on the GameCube, which is the direction
/// that catches anything: the hardware build gets these offsets right by
/// construction, the PC build is where a field silently grows.
#define MELEE_PC_LAYOUT_ASSERT(cond) _Static_assert((cond), #cond)
#else
/// Identity on the GameCube, which is big-endian already.
#define MELEE_PC_BE32(x) (x)
#define MELEE_PC_BE16(x) (x)
#define MELEE_PC_DAT(T, p)
#define MELEE_PC_DAT_PTRNULL(T, p)
#define MELEE_PC_ON_ARCHIVE_FREE(base, size) ((void) 0)
#define MELEE_PC_IS_NOT_MAINRAM(a) ((u32) (a) < 0x80000000U)
#define MELEE_PC_IS_MAINRAM(a) ((u32) (a) >= 0x80000000U)
#define MELEE_PC_ALIGN32
#define MELEE_PC_PUMP() ((void) 0)
/// A GameCube pointer is already the 32-bit slot it is stored in.
#define MELEE_PC_PTR32
#define MELEE_PC_LAYOUT_ASSERT(cond)
#endif

#define RETURN_IF(cond)                                                       \
    do {                                                                      \
        if ((cond)) {                                                         \
            return;                                                           \
        }                                                                     \
    } while (0)

#if defined(__MWERKS__) && !defined(M2CTX)
#define SDATA __declspec(section ".sdata")
#define DATA __declspec(section ".data")
#define WEAK __declspec(weak)
#else
#define SDATA
#define DATA
#define WEAK
#endif

#endif
