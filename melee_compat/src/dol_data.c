// Host-native definitions for initialised data the decomp has not reached yet.
//
// The PC build links only decompiled translation units, so every symbol that
// still lives as raw bytes in the original DOL comes from
// tools/gen_undefined_stubs.py -- as a *zero-filled* array.  For a `.bss`
// symbol that is correct.  For `.data`, `.sdata`, `.sdata2` and `.rodata` it
// is not: the game reads a constant and gets zero.  Roughly 400 stubs are in
// that state; see PC_PORT_PITFALLS.md.
//
// Copying the DOL bytes verbatim would not fix it either -- they are
// big-endian and their pointers are absolute GameCube addresses, so each one
// has to be decoded against its type.  That is what this file does, one
// symbol at a time, as the port reaches them.  Values were read out of
// orig/GALE01/sys/main.dol at the address in the symbol name.

#include <platform.h>

#include <baselib/cobj.h>
#include <baselib/wobj.h>

// The background-flash camera, used by lbBgFlash_800208EC().  Perspective,
// 640x480, looking straight down -Z at the 2D UI plane.
static HSD_WObjDesc lbl_803BB000 = {
    NULL,
    { 320.0f, -240.0f, 415.69220f },
    NULL,
};

static HSD_WObjDesc lbl_803BB014 = {
    NULL,
    { 320.0f, -240.0f, 0.0f },
    NULL,
};

HSD_CObjDesc lbl_803BB028 = {
    .perspective = {
        NULL,             // class_name
        0,                // flags
        PROJ_PERSPECTIVE, // projection_type
        { 0, 640, 0, 480 },  // viewport
        { 0, 640, 0, 480 },  // scissor
        &lbl_803BB000,    // eyepos
        &lbl_803BB014,    // interest
        0.0f,             // roll
        NULL,             // up_vector
        0.1f,             // nnear
        30000.0f,         // ffar
        60.0f,            // fov
        1.3333333f,       // aspect
    },
};
