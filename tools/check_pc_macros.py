#!/usr/bin/env python3
"""Check platform.h's MELEE_PC_* macros are defined consistently.

Two mistakes are easy to make here and neither shows up at compile time:

  * emptying a macro's body in the PC branch, so it silently does nothing
    while still looking defined at the call sites;
  * defining one only in the PC branch, which breaks the *hardware* build --
    or only in the hardware branch, which silently disables it on PC.

Both have happened. This compares the two branches and fails on either.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HEADER = ROOT / "src/Runtime/platform.h"

DEFINE = re.compile(r"^#define\s+(MELEE_PC_[A-Za-z0-9_]*)\s*(\([^)]*\))?(.*)$")


def branches(text):
    """Return (pc_defines, hw_defines) as {name: body}."""
    pc, hw, cur, depth = {}, {}, None, 0
    for line in text.splitlines():
        s = line.strip()
        if s.startswith("#ifdef MELEE_PC") or s.startswith("#if defined(MELEE_PC"):
            cur, depth = pc, 1
            continue
        if cur is not None:
            if s.startswith("#if"):
                depth += 1
            elif s.startswith("#endif"):
                depth -= 1
                if depth == 0:
                    cur = None
                    continue
            elif s.startswith("#else") and depth == 1:
                cur = hw
                continue
        m = DEFINE.match(s)
        if m and cur is not None:
            body = m.group(3).strip()
            # A trailing backslash means the body is on the following lines,
            # so the macro is definitely not empty.
            cur[m.group(1)] = "<continued>" if body.endswith("\\") else body
    return pc, hw


def main():
    text = HEADER.read_text()
    pc, hw = branches(text)
    errors = []

    # An empty body is expected on hardware -- that is what keeps the matching
    # build's text unchanged -- but never on PC, where it would mean the macro
    # silently does nothing at every call site.
    for name, body in sorted(pc.items()):
        if not body:
            errors.append(f"{name}: empty body in the MELEE_PC branch "
                          f"-- it would silently do nothing")

    for name in sorted(set(pc) - set(hw)):
        errors.append(f"{name}: defined for PC but not for hardware "
                      f"-- the matching build will not compile")
    for name in sorted(set(hw) - set(pc)):
        errors.append(f"{name}: defined for hardware but not for PC "
                      f"-- it would silently do nothing on PC")

    if errors:
        print(f"{HEADER.relative_to(ROOT)}: inconsistent MELEE_PC macros",
              file=sys.stderr)
        for e in errors:
            print(f"  {e}", file=sys.stderr)
        return 1
    print(f"MELEE_PC macros OK ({len(pc)} defined in both branches)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
