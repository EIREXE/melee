#!/usr/bin/env python3
"""Emit GameCube-vs-host layout tables for the DAT-resident HSD types.

Rather than hand-maintaining offset tables, they are extracted from the real headers with clang:

    clang --target=powerpc-none-eabi -Xclang -fdump-record-layouts-complete
    clang                            -Xclang -fdump-record-layouts-complete

The two dumps are matched up field by field and emitted as C descriptor tables
for melee_compat/src/dat_reloc.c to use

Unions are emitted as markers requiring a
hand-written hook, because choosing the wrong arm converts a pointer as a float
(or follows a non-pointer as an address) and corrupts silently. Array lengths
are declared explicitly below, because array-ness is not visible in a type

Usage:
    python tools/gen_dat_layouts.py
    python tools/gen_dat_layouts.py --check
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Types taken by the HSD_*LoadDesc entry points -- where DAT data enters the
# game. Everything else is discovered from these by following pointer fields,
# so "did we miss a type?" is answered by the tool rather than by a crash.
ROOTS = [
    "HSD_Joint",
    "HSD_DObjDesc",
    "HSD_PObjDesc",
    "HSD_MObjDesc",
    "HSD_TObjDesc",
    "HSD_TlutDesc",
    "HSD_TObjTevDesc",
    "HSD_AObjDesc",
    "HSD_FObjDesc",
    "HSD_RObjDesc",
    "HSD_WObjDesc",
    "HSD_CObjDesc",
    "HSD_LightDesc",
    "HSD_FogDesc",
    "HSD_FogAdjDesc",
    # Melee's own DAT-resident types. These are reached through
    # lbArchive_LoadSymbols()-style public-symbol lookups rather than the HSD
    # load entry points, so each root is converted at its call site.
    "SceneDesc",
    # lbRefract_800222A4() looks this up as "lbRefData" in LbRf.dat.
    "lbRefract_Data",
    "EF_DAT_Entry",
    "GroundParam",
    "it_804D6D20_t",
    "GroundItemData",
    "MapCollData",
    "UnkStageDat",
    "ftLoadCommonData",
    "ftData",
    "Fighter_WaitAnimData",
]

# Reachable DAT types whose names do not end in "Desc", so the closure's name
# filter would not pick them up.
EXTRA = [
    "HSD_VtxDescList",
    "HSD_TexAnim",
    # The HSD_Joint ptcl arm is a DAT-resident singly-linked list: 8 bytes on
    # GameCube, 16 on host, so walking it unconverted reads nonsense.
    "HSD_SList",
    # The HSD_LightDesc attenuation arm.
    "HSD_LightAttn",
    # Reached from SceneDesc.
    "DynamicModelDesc",
    "SceneCameraDesc",
    "SceneFogDesc",
    "LightList",
    "HSD_AnimJoint",
    "HSD_MatAnimJoint",
    "HSD_ShapeAnimJoint",
    "HSD_CameraAnim",
    "HSD_LightAnim",
    # Reached from HSD_LightAnim/HSD_CameraAnim. Named here rather than picked
    # up by the closure because that only follows names ending in "Desc", and
    # these are descriptor chains that happen not to be spelled that way.
    "HSD_WObjAnim",
    "HSD_RObjAnimJoint",
    # The material/shape animation chains hanging off HSD_MatAnimJoint and
    # HSD_ShapeAnimJoint. Same story: descriptor chains not spelled "*Desc".
    "HSD_MatAnim",
    "HSD_RenderAnim",
    "HSD_ChanAnim",
    "HSD_TevRegAnim",
    "HSD_ShapeAnim",
    "HSD_ShapeAnimDObj",
    # Plain structs referenced by pointer from descriptors. Not chains, but
    # they hold floats, so leaving them raw would feed big-endian bit patterns
    # to the renderer as colours and coordinates.
    "HSD_Material",
    "HSD_RvalueList",
    "Vec",
    # The lbRefract_Data curve array. Two floats, so leaving it raw would feed
    # big-endian bit patterns to the refraction maths as parameters.
    "lbRefract_Param",
    # One row per StKind, hanging off GroundParam::stage_params.
    "StageParam",
    # The three arrays hanging off MapCollData.
    "MapJoint",
    "MapLine",
    "Vec2",
    # Reached from UnkStageDat.
    "UnkStageDat_x8_t",
    "UnkStageDatInternal",
    "GrJoint",
    "GroundShadowEntry",
    # Reached through HSD_Joint's spline arm. It looks like a runtime type --
    # splInit() fills segLength/segPoly later -- but the header fields and the
    # control points come straight out of the DAT.
    "HSD_Spline",
]

# Runtime objects: HSD allocates these itself, already in host layout.
# Converting one would read live engine state as though it were file data.
NEVER = {
    "HSD_JObj", "HSD_DObj", "HSD_PObj", "HSD_MObj", "HSD_TObj", "HSD_AObj",
    "HSD_RObj", "HSD_WObj", "HSD_CObj", "HSD_LObj", "HSD_Fog", "HSD_FObj",
    "HSD_Obj", "HSD_GObj", "HSD_Tlut", "HSD_TObjTev", "HSD_TExp",
    "HSD_Exp", "HSD_FogAdj", "HSD_CObj", "HSD_Fog",
    # Runtime objects that loadShapeSetDesc()/friends allocate from a Desc.
    # HSD_LightAnim and HSD_WObjAnim look like they belong here but do not:
    # unlike HSD_ShapeSet they have no separate *Desc type, because they *are*
    # the descriptors. Every field is a `*Desc` pointer or a `next` chain, and
    # HSD_LObjAddAnim() reads lanim->aobjdesc straight into HSD_AObjLoadDesc().
    # They live in the DAT file and must be converted like HSD_AnimJoint.
    "HSD_ShapeSet", "HSD_TexAnimSRT",
}

# Array lengths. A pointer field's type says nothing about how many elements
# sit behind it, so each array is declared here:
#
#   ("sentinel", tag_field, value) -- elements until tag_field == value
#   ("count",    count_field)      -- array of structs, length from a sibling
#   ("ptrcount", count_field)      -- array of *pointers*, length from a sibling
ARRAYS = {
    ("HSD_PObjDesc", "verts"): ("sentinel", "attr", 0xFF),  # GX_VA_NULL
    # `for (i = 0; scene->models[i] != NULL; i++)` -- vi0102.c:157
    ("SceneDesc", "models"): ("ptrnull",),
    ("SceneDesc", "lights"): ("ptrnull",),
    # cameras/fogs are left as single-element pointers: the format gives no
    # terminator and every caller uses [0].
    ("SceneCameraDesc", "anims"): ("ptrnull",),
    ("SceneFogDesc", "anims"): ("ptrnull",),
    ("DynamicModelDesc", "anims"): ("ptrnull",),
    ("DynamicModelDesc", "matanims"): ("ptrnull",),
    ("DynamicModelDesc", "shapeanims"): ("ptrnull",),
    ("LightList", "anims"): ("ptrnull",),
    ("HSD_TexAnim", "imagetbl"): ("ptrcount", "n_imagetbl"),
    ("HSD_TexAnim", "tluttbl"): ("ptrcount", "n_tluttbl"),
    ("lbRefract_Data", "params"): ("count", "count"),
    # Ground_801C28CC walks stage_params[0 .. stage_param_count).
    ("GroundParam", "stage_params"): ("count", "stage_param_count"),
    ("MapCollData", "verts"): ("count", "vert_count"),
    ("MapCollData", "lines"): ("count", "line_count"),
    ("MapCollData", "joints"): ("count", "joint_count"),
    ("UnkStageDat", "unk8"): ("count", "unkC"),
    # unk10 is HSD_Spline**, and HSD_Spline is a runtime object (NEVER), so
    # the pointer is decoded but the splines are left raw.
    ("UnkStageDat", "unk28"): ("ptrcount", "unk2C"),
    ("UnkStageDat", "unk20"): ("count", "unk24"),
    ("UnkStageDat_x8_t", "unk20"): ("count", "unk24"),
    ("UnkStageDat_x8_t", "x18"): ("ptrnull",),
    # Enough for type 0 (lerp), which indexes cv[0 .. numcv). Type 1 walks
    # cv[idx * 3] and would need 3 * numcv - 2; if a bezier spline shows up,
    # this needs a hook rather than a plain count.
    ("HSD_Spline", "cv"): ("count", "numcv"),
    # grAnime_801C7C1C indexes each entry as an array (`aj = &aj[arg2]`) and
    # the file records no length, so entries stay raw and the call site
    # converts the one element it wants.
    ("UnkStageDat_x8_t", "unk4"): ("ptrnullraw",),
    ("UnkStageDat_x8_t", "unk8"): ("ptrnullraw",),
    ("UnkStageDat_x8_t", "unkC"): ("ptrnullraw",),
}

PROBE = """
#include <platform.h>
#include <baselib/jobj.h>
#include <baselib/dobj.h>
#include <baselib/pobj.h>
#include <baselib/mobj.h>
#include <baselib/tobj.h>
#include <baselib/aobj.h>
#include <baselib/fobj.h>
#include <baselib/robj.h>
#include <baselib/wobj.h>
#include <baselib/lobj.h>
#include <baselib/cobj.h>
#include <baselib/fog.h>
#include <baselib/sobjlib.h>
#include <baselib/spline.h>
#include <melee/sc/types.h>
#include <melee/lb/types.h>
#include <melee/ef/types.h>
#include <melee/gr/types.h>
#include <melee/it/it_3F14.h>
#include <melee/mp/types.h>
#include <melee/ft/fighter.h>
"""

INCLUDES = [
    "melee_compat/include",
    "extern/aurora/include",
    "src",
    "src/MSL",
    "src/Runtime",
    "src/sysdolphin",
    "src/melee",
]

RECORD_RE = re.compile(r"^\s*(\d+) \| (struct|union) (\w+)$")
FIELD_RE = re.compile(r"^(\s*)(\d+) \|(\s+)(.+?)\s+(\w+)$")
SIZE_RE = re.compile(r"\[sizeof=(\d+), align=(\d+)\]")


def dump_layouts(target: str | None) -> dict:
    """Parse clang's record layout dump into {name: {rows, size}}.

    Rows keep their indentation depth so nested aggregates can be dropped
    later: clang prints a nested struct *and* its members, and the aggregate
    row would otherwise be converted as if it were a scalar.
    """
    cmd = ["clang", "-fsyntax-only", "-std=gnu99", "-w",
           "-DMELEE_PC", "-DAURORA", "-DTARGET_PC", "-DBUGFIX",
           "-Xclang", "-fdump-record-layouts-complete", "-x", "c", "-"]
    # Order matters and must match the real build: melee_compat/include
    # shadows src/MSL, so getting these backwards makes the probe see
    # `uintptr_t` as MSL's 4-byte typedef while the PC build compiles the
    # 8-byte one -- and every host offset after such a field comes out wrong.
    incs = []
    for inc in INCLUDES:
        incs += ["-isystem", str(ROOT / inc)]
    cmd[1:1] = incs
    if target:
        cmd[1:1] = [f"--target={target}"]

    proc = subprocess.run(cmd, input=PROBE, capture_output=True, text=True,
                          cwd=ROOT)

    records: dict[str, dict] = {}
    cur = None
    for line in proc.stdout.splitlines():
        m = RECORD_RE.match(line)
        if m and m.group(1) == "0":
            cur = {"name": m.group(3), "rows": [], "size": None}
            records[m.group(3)] = cur
            continue
        if cur is None:
            continue
        m = SIZE_RE.search(line)
        if m:
            cur["size"] = int(m.group(1))
            cur = None
            continue
        m = FIELD_RE.match(line)
        if m:
            cur["rows"].append({
                "off": int(m.group(2)),
                "depth": len(m.group(3)),
                "ctype": m.group(4).strip(),
                "name": m.group(5),
            })
    return records


def leaves(rec: dict) -> list:
    """Drop aggregate rows, keeping only scalar/pointer leaves.

    clang prints `struct Vec pos` followed by its `x`, `y`, `z`. The aggregate
    row spans 12 bytes but would be converted as a 4-byte scalar; only the
    leaves carry real field types.
    """
    rows = rec["rows"]
    out = []
    for i, row in enumerate(rows):
        nxt = rows[i + 1] if i + 1 < len(rows) else None
        if nxt is not None and nxt["depth"] > row["depth"]:
            continue  # aggregate: its members follow
        out.append(row)
    return out


def lookup(records: dict, name: str) -> dict | None:
    return records.get(name) or records.get("_" + name)


# Typedef names that clang prints in field types but that are not the name of
# the underlying record, so lookup() would miss them.
ALIASES = {"Vec3": "Vec"}

# Types clang never names in a record dump because they are typedefs of an
# anonymous struct (`typedef struct { f32 x, y; } Vec2;` -- no tag, unlike
# `struct Vec`). Declared by hand as {name: (size, [(field, ctype, offset)])};
# identical on both targets, which is checked below like any other type.
SYNTHETIC = {
    "Vec2": (8, [("x", "f32", 0), ("y", "f32", 4)]),
}


def base_type(ctype: str) -> str:
    """Strip a pointer type to its record name, or "" if it is not one."""
    t = ctype.replace("const ", "").strip()
    if t.count("*") == 0:
        return ""
    base = t.replace("*", "").strip()
    if base.startswith(("struct ", "union ")):
        base = base.split(None, 1)[1]
    if base.startswith("_"):
        base = base[1:]
    return ALIASES.get(base, base)


def closure(gc: dict) -> list[str]:
    """Every DAT type reachable from ROOTS by following pointer fields."""
    seen: list[str] = []
    queue = list(ROOTS) + list(EXTRA)
    while queue:
        name = queue.pop(0)
        if name in seen or name in NEVER or lookup(gc, name) is None:
            continue
        seen.append(name)
        for row in leaves(lookup(gc, name)):
            b = base_type(row["ctype"])
            if not b or b in NEVER or b in seen:
                continue
            # Only follow into descriptors or explicitly named types. Anything
            # else is a runtime object or a raw blob, and stays a plain address.
            if b.endswith("Desc") or b in EXTRA:
                queue.append(b)
    return seen


# Typedefs that are pointers without a "*" in the spelling. A pointer misread
# as a scalar used to be survivable -- the archive loader wrote absolute
# addresses, so copying the 32 bits through happened to produce a valid pointer
# on a low-mapped host. It is not survivable now: slots hold self-relative
# offsets, so an undecoded one is a wild pointer. HSD_Joint::mtx (MtxPtr) is
# how this was found.
# uintptr_t appears where a field holds an *address* but is spelled as an
# integer. Treating it as a pointer widens the slot and maps 0 to NULL, which
# is what these fields hold in the file -- they are filled in at runtime
# (Fighter_WaitAnimData::x14 gets `a_head + x4`, and a_head is a figatree
# buffer from lbFile, not DAT-resident).
POINTER_TYPEDEFS = {"MtxPtr", "VecMtxPtr", "uintptr_t"}

# Typedefs that are a fixed-size array of scalars rather than one scalar.
# Classifying one as DAT_U32 swaps its first four bytes and leaves the rest
# zeroed, so each is emitted as N separate elements instead.
INLINE_SCALAR_ARRAYS = {"Mtx44": ("DAT_U32", 16, 4)}

# Element kinds for inline arrays of scalars wider than a byte.
WIDE_SCALAR_KINDS = {
    "s16": ("DAT_U16", 2), "u16": ("DAT_U16", 2),
    "short": ("DAT_U16", 2), "unsigned short": ("DAT_U16", 2),
    "s32": ("DAT_U32", 4), "u32": ("DAT_U32", 4),
    "int": ("DAT_U32", 4), "unsigned int": ("DAT_U32", 4),
    "f32": ("DAT_U32", 4), "float": ("DAT_U32", 4),
}

# Everything else that may legitimately appear as a scalar field. The point of
# listing them is that kind_for() must never silently fall back: an unknown
# type is far more likely to be a new typedef than a new integer width, and
# guessing DAT_U32 is exactly the bug described above.
KNOWN_SCALARS = {
    "int", "unsigned int", "long", "unsigned long", "s32", "u32", "f32",
    "float", "enum_t",
    # Enum typedefs. -fno-short-enums on both targets, so these are four
    # bytes either side and swap like any other u32. Listed one by one rather
    # than pattern-matched, so a new *pointer* typedef cannot slip in as one.
    "StKind", "GrKind",
}


def kind_for(ctype: str) -> str:
    t = ctype.replace("const ", "").strip()
    if "*" in t:
        return "DAT_PTR"
    if t in POINTER_TYPEDEFS:
        return "DAT_PTR"
    if t in ("u8", "s8", "char", "unsigned char", "signed char", "bool",
             "_Bool"):
        return "DAT_U8"
    if t in ("u16", "s16", "short", "unsigned short"):
        return "DAT_U16"
    if t in ("f64", "double", "u64", "s64", "long long",
             "unsigned long long"):
        return "DAT_U64"
    if t in KNOWN_SCALARS or t.startswith("GX"):
        return "DAT_U32"  # int, u32, f32, enums: swapping 32 bits is right
    return ""  # unknown -- main() turns this into an error


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("-o", "--output", type=Path,
                    default=ROOT / "melee_compat/src/dat_layouts.inc")
    ap.add_argument("--enum-output", type=Path,
                    default=ROOT / "melee_compat/include/melee_pc_dat_types.h")
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()

    gc = dump_layouts("powerpc-none-eabi")
    host = dump_layouts(None)
    if not gc or not host:
        print("error: clang produced no layouts", file=sys.stderr)
        return 1

    for nm, (size, fields) in SYNTHETIC.items():
        rows = [{"name": f, "ctype": t, "off": o, "depth": 0}
                for f, t, o in fields]
        for tbl in (gc, host):
            if nm in tbl:
                print(f"error: {nm} is in SYNTHETIC but clang named it too",
                      file=sys.stderr)
                return 1
            tbl[nm] = {"rows": rows, "size": size}

    types = closure(gc)
    ids = {name: i for i, name in enumerate(types)}

    enum = [
        "// Generated by tools/gen_dat_layouts.py -- do not edit.",
        "//",
        "// Type ids for MELEE_PC_DAT(). Public so the HSD_*LoadDesc entry",
        "// points can name the type they are converting; naming a type that",
        "// is not described is then a compile error rather than a surprise.",
        "",
        "#ifndef MELEE_PC_DAT_TYPES_H",
        "#define MELEE_PC_DAT_TYPES_H",
        "",
        "typedef enum {",
        "    DAT_T_NONE = -1,",
    ]
    for name in types:
        enum.append(f"    DAT_T_{name} = {ids[name]},")
    enum += [f"    DAT_T_COUNT = {len(types)}", "} DatTypeId;", "",
             "#endif", ""]

    body: list[str] = []
    unions: dict[str, list] = {}

    for name in types:
        g, h = lookup(gc, name), lookup(host, name)
        gl = leaves(g)
        hlist = leaves(h)

        # Paired by position, NOT by name. Both dumps enumerate the same
        # fields in the same order, but names are not unique: every Vec3
        # contributes an `x`, `y` and `z`, so a name-keyed lookup collapsed
        # HSD_Joint's rotation, scale and position onto one set of host
        # offsets. rotation and scale were then written into position's slot
        # and the real scale field was never written at all -- it stayed zero,
        # which made every joint matrix collapse the model to a point. The
        # whole scene drew and nothing was visible.
        if len(hlist) != len(gl):
            print(f"error: {name}: {len(gl)} GameCube leaves vs "
                  f"{len(hlist)} host leaves -- the two layouts disagree, "
                  f"so they cannot be paired", file=sys.stderr)
            return 1
        for gr, hr in zip(gl, hlist):
            if gr["name"] != hr["name"]:
                print(f"error: {name}: field order differs between ABIs "
                      f"({gr['name']!r} vs {hr['name']!r})", file=sys.stderr)
                return 1
        hl = {id(gr): hr for gr, hr in zip(gl, hlist)}

        # Several leaves at one offset means a union: only the discriminant
        # says which arm is live, and that is type-specific game logic.
        by_off: dict[int, list] = {}
        for r in gl:
            by_off.setdefault(r["off"], []).append(r)

        # Overlapping fields only need a discriminant if the arms actually
        # differ. A union of *parallel* structs (HSD_CObjDesc's camera
        # variants) has the same type at every offset in every arm --
        # `eyepos` is a HSD_WObjDesc* in all four, `top` and `fov` are both
        # f32 -- so the conversion is the same whichever arm is live and no
        # hook is needed.
        def same_treatment(rows):
            kinds = {kind_for(r["ctype"]) for r in rows}
            pointees = {base_type(r["ctype"]) for r in rows}
            return len(kinds) == 1 and len(pointees) == 1

        union_offs = {o for o, rs in by_off.items()
                      if len(rs) > 1 and not same_treatment(rs)}
        if union_offs:
            unions[name] = sorted(
                (o, [r["name"] for r in by_off[o]]) for o in union_offs)

        body.append(f"static const DatField {name}_fields[] = {{")
        emitted: set[int] = set()
        for r in gl:
            goff, hoff = r["off"], hl[id(r)]["off"]

            if goff in union_offs:
                if goff in emitted:
                    continue
                emitted.add(goff)
                arms = " | ".join(x["name"] for x in by_off[goff])
                body.append(f"    {{ {goff:4}, {hoff:4}, DAT_UNION, "
                            f"DAT_T_NONE, 0, 0 }}, // {arms}")
                continue

            arr = ARRAYS.get((name, r["name"]))
            pointee = base_type(r["ctype"])
            pid = f"DAT_T_{pointee}" if pointee in ids else "DAT_T_NONE"

            # A plain DAT_PTR with no element type is fine -- the converter
            # relocates it and stops (char* class_name, for one). Every array
            # kind, though, recurses into f->type for each element, so an
            # untyped one panics at runtime the first time the game touches
            # that field. Catch it here, where the fix is obvious, instead of
            # several frames deep in a converter backtrace.
            if arr and pid == "DAT_T_NONE":
                print(f"error: {name}.{r['name']}: annotated as an array of "
                      f"'{pointee}', but that type is not in the table -- it "
                      f"is missing from EXTRA/ROOTS or wrongly listed in "
                      f"NEVER. Array elements are always converted, so this "
                      f"would panic at runtime.", file=sys.stderr)
                return 1

            if arr and arr[0] == "sentinel":
                _, tag, val = arr
                elem = lookup(gc, pointee)
                trow = next((x for x in leaves(elem) if x["name"] == tag),
                            None) if elem else None
                if trow is None:
                    print(f"error: {name}.{r['name']}: sentinel field "
                          f"'{tag}' not found in {pointee}", file=sys.stderr)
                    return 1
                body.append(f"    {{ {goff:4}, {hoff:4}, DAT_ARR_SENTINEL, "
                            f"{pid}, {trow['off']}, {val} }}, "
                            f"// {r['name']}[] until {tag}=={val:#x}")
            elif arr and arr[0] == "ptrnullraw":
                body.append(f"    {{ {goff:4}, {hoff:4}, "
                            f"DAT_ARR_PTRNULL_RAW, {pid}, 0, 0 }}, "
                            f"// {r['name']}[] until NULL, elements raw")
            elif arr and arr[0] == "ptrnull":
                body.append(f"    {{ {goff:4}, {hoff:4}, DAT_ARR_PTRNULL, "
                            f"{pid}, 0, 0 }}, "
                            f"// {r['name']}[] until NULL")
            elif arr and arr[0] in ("count", "ptrcount"):
                cname = arr[1]
                crow = next((x for x in gl if x["name"] == cname), None)
                if crow is None:
                    print(f"error: {name}.{r['name']}: count field "
                          f"'{cname}' not found", file=sys.stderr)
                    return 1
                ck = kind_for(crow["ctype"])
                cw = 1 if ck == "DAT_U8" else 2 if ck == "DAT_U16" else 4
                k = ("DAT_ARR_COUNT" if arr[0] == "count"
                     else "DAT_ARR_PTRCOUNT")
                body.append(f"    {{ {goff:4}, {hoff:4}, {k}, "
                            f"{pid}, {crow['off']}, {cw} }}, "
                            f"// {r['name']}[{cname}]")
            else:
                # base_type() is for pointers and returns "" here, so match
                # the spelled type the same way kind_for() does.
                base = r["ctype"].replace("const ", "").strip()
                # Inline byte arrays -- padding, and char buffers. Single
                # bytes have no byte order, so copy the run straight through
                # rather than emitting one row per byte.
                mb = re.fullmatch(r"(?:u8|s8|char|unsigned char|signed char)"
                                  r"\s*\[(\d+)\]", base)
                if mb:
                    body.append(f"    {{ {goff:4}, {hoff:4}, "
                                f"{'DAT_BYTES':15}, {'DAT_T_NONE':28}, "
                                f"{mb.group(1)}, 0 }}, "
                                f"// {r['ctype']} {r['name']}")
                    continue
                # Inline arrays of wider scalars. Each element still needs
                # swapping, so emit one row per element.
                ms = re.fullmatch(r"([A-Za-z_][\w ]*?)\s*\[(\d+)\]", base)
                if ms and ms.group(1).strip() in WIDE_SCALAR_KINDS:
                    ek, w = WIDE_SCALAR_KINDS[ms.group(1).strip()]
                    for j in range(int(ms.group(2))):
                        body.append(f"    {{ {goff + j * w:4}, "
                                    f"{hoff + j * w:4}, {ek:15}, "
                                    f"{'DAT_T_NONE':28}, 0, 0 }}, "
                                    f"// {r['ctype']} {r['name']}[{j}]")
                    continue
                if base in INLINE_SCALAR_ARRAYS:
                    ek, n, w = INLINE_SCALAR_ARRAYS[base]
                    for j in range(n):
                        body.append(f"    {{ {goff + j * w:4}, "
                                    f"{hoff + j * w:4}, {ek:15}, "
                                    f"{'DAT_T_NONE':28}, 0, 0 }}, "
                                    f"// {r['ctype']} {r['name']}[{j}]")
                    continue
                k = kind_for(r["ctype"])
                if not k:
                    print(f"error: {name}.{r['name']}: unknown field type "
                          f"'{r['ctype']}'. If it is a pointer add it to "
                          f"POINTER_TYPEDEFS, if it is an array of scalars add "
                          f"it to INLINE_SCALAR_ARRAYS, otherwise add it to "
                          f"KNOWN_SCALARS. Guessing here is how a pointer ends "
                          f"up copied through undecoded.", file=sys.stderr)
                    return 1
                body.append(f"    {{ {goff:4}, {hoff:4}, "
                            f"{k:15}, {pid:28}, 0, 0 }},"
                            f" // {r['ctype']} {r['name']}")

        body.append("    { 0, 0, DAT_END, DAT_T_NONE, 0, 0 },")
        body.append("};")
        body.append("")

    body.append("static const DatType dat_types[] = {")
    for name in types:
        g, h = lookup(gc, name), lookup(host, name)
        body.append(f'    {{ "{name}", {g["size"]}, {h["size"]}, '
                    f"{name}_fields, {1 if name in unions else 0} }},")
    body.append("};")
    body.append("")

    header = [
        "// Generated by tools/gen_dat_layouts.py -- do not edit.",
        "//",
        "// GameCube (ppc32) vs host field layouts for the DAT-resident HSD",
        "// types, extracted from the real headers by clang.",
        "//",
        f"// {len(types)} types, reached from the HSD_*LoadDesc entry points.",
        "// Types flagged with has_union need a hook in dat_reloc.c.",
        "",
    ]
    text = "\n".join(header + body)
    enum_text = "\n".join(enum)

    if unions:
        print(f"note: {len(unions)} type(s) contain unions and need a hook:",
              file=sys.stderr)
        for name, offs in unions.items():
            for off, arms in offs:
                print(f"    {name} @{off}: {' | '.join(arms)}",
                      file=sys.stderr)

    if args.check:
        for path, want in ((args.output, text), (args.enum_output, enum_text)):
            if not path.exists() or path.read_text() != want:
                print(f"{path} is out of date; run "
                      f"python tools/gen_dat_layouts.py", file=sys.stderr)
                return 1
        print(f"{args.output} is up to date")
        return 0

    args.output.write_text(text, encoding="utf-8")
    args.enum_output.write_text(enum_text, encoding="utf-8")
    print(f"{len(types)} types -> {args.output}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
