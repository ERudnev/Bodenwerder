"""Fail when any project object file has more COFF sections than a threshold.

Usage: python tools/buildstats/check_sections.py <build-dir> [--limit N] [--top K]

Rationale: MSVC objects are limited to 65,536 sections unless compiled with /bigobj.
The project compiles with /bigobj, so the hard limit never fires. This check keeps
the per-object instantiation volume visible instead. Default limit 60,000 leaves a
margin below the hard limit. See AUDIT_2026-09-23_templates.md.
"""
import argparse
import glob
import os
import struct
import sys


def sections_of(path):
    with open(path, "rb") as f:
        header = f.read(64)
    sig1, sig2 = struct.unpack_from("<HH", header, 0)
    if sig1 == 0 and sig2 == 0xFFFF:  # ANON_OBJECT_HEADER_BIGOBJ
        return struct.unpack_from("<I", header, 44)[0]
    return struct.unpack_from("<H", header, 2)[0]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("build_dir")
    parser.add_argument("--limit", type=int, default=60000)
    parser.add_argument("--top", type=int, default=10)
    parser.add_argument("--include", default="modules,stories", help="comma separated subdirectories of build_dir")
    parser.add_argument("--allow", action="append", default=[],
                        help="path substring of a known outlier to report but not fail on (repeatable)")
    args = parser.parse_args()
    # Known outlier until R3/R4 shrink per-aspect instantiation: runtimes.q1 registers 12 aspects.
    if not args.allow:
        args.allow = ["runtimes.q1.cpp.obj"]

    rows = []
    for sub in args.include.split(","):
        for path in glob.glob(os.path.join(args.build_dir, sub, "**", "*.obj"), recursive=True):
            rows.append((sections_of(path), os.path.relpath(path, args.build_dir)))
    rows.sort(reverse=True)

    print(f"objects={len(rows)} limit={args.limit}")
    for count, path in rows[: args.top]:
        print(f"{count:9d}  {path}")

    over = [(c, p) for c, p in rows if c > args.limit]
    allowed = [(c, p) for c, p in over if any(a in p for a in args.allow)]
    failing = [(c, p) for c, p in over if (c, p) not in allowed]
    for count, path in allowed:
        print(f"ALLOWED: {count} sections in {path}")
    if failing:
        print(f"FAIL: {len(failing)} object(s) over {args.limit} sections")
        return 1
    print("OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
