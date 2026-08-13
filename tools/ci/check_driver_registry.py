#!/usr/bin/env python3
"""Assert every platform that compiles a driver registry also compiles its drivers.

Both driver registries reference their drivers unconditionally:

    src/usb/usbh/hid/hid_registry.c    device_interfaces[CONTROLLER_X] = &foo_interface;
    src/bt/bthid/bthid_registry.c      foo_bt_register();

There is not a single #if in either init function. So any build that compiles a
registry owes the .c file behind every symbol it names, or it fails to link.

The per-platform source lists that have to satisfy that are hand-maintained --
one CMakeLists or Makefile per backend -- and they have drifted four times that
this check covers, each verified by running it on the commit before the fix:

    steam_controller_ble        esp+nrf  fixed by c24395a7
    steam_controller            esp+nrf  fixed by 3cc45843
    steam_controller            wch      still missing on main
    elo_vagabond                esp+wch  still missing on main

Every one was a driver added to a registry and to the RP2040 list and to no
other list. The ones on a CI-built target were caught in hours; the ones that
were not stayed broken until somebody went looking.

NOT covered: the same failure *class* also shows up outside the registries --
shared code calling a platform function that only some backends define, e.g. the
ps4_auth_flash_* calls in cdc_commands.c that left the CH32V307 port unlinkable
for 32 days. Those are unguarded references too, but they do not come from a
registry table, so this script does not see them. It closes one well-defined
hole, not the class.

This check needs no toolchain, so it runs everywhere in about a second and does
not care whether a target is in the release matrix.

SCOPE / KNOWN LIMITATION
    Membership is tested per *file*, not per config branch: if a build file
    mentions the registry anywhere, it must mention every driver somewhere. A
    driver listed under the wrong CONFIG_APP branch would satisfy this check and
    still fail to link. That is deliberate -- evaluating CMake branches properly
    means running CMake, and the coarse version already catches all five
    historical drifts with no false positives. Tighten it if that stops being true.

Exit status: 0 clean, 1 drift found, 2 the check itself could not run.
"""

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

# Registry -> how its drivers are referenced.
REGISTRIES = {
    "src/usb/usbh/hid/hid_registry.c": [
        # device_interfaces[CONTROLLER_DUALSHOCK4] = &sony_ds4_interface;
        re.compile(r"device_interfaces\s*\[[^\]]+\]\s*=\s*&\s*(\w+)\s*;"),
    ],
    "src/bt/bthid/bthid_registry.c": [
        # ds4_bt_register();
        re.compile(r"^\s*(\w+_register)\s*\(\s*\)\s*;", re.M),
    ],
}

# Hand-maintained per-backend source lists.
BUILD_FILES = [
    "src/CMakeLists.txt",
    "esp/main/CMakeLists.txt",
    "nrf/CMakeLists.txt",
    "wch/Makefile",
]

BLOCK_COMMENT = re.compile(r"/\*.*?\*/", re.S)
LINE_COMMENT = re.compile(r"//[^\n]*")


def strip_comments(text):
    """Drop comments so commented-out registrations are not treated as live."""
    return LINE_COMMENT.sub("", BLOCK_COMMENT.sub("", text))


def has_preprocessor_guard(text):
    """True if the file gates anything with #if/#ifdef.

    The whole check rests on the registries being unconditional. If someone adds
    a guard, the symbol set stops being a constant and this script would start
    demanding sources a platform legitimately does not need -- so refuse to run
    rather than emit a confident wrong answer.
    """
    return re.search(r"^\s*#\s*(if|ifdef|ifndef)\b", text, re.M) is not None


def find_definition(symbol):
    """Locate the single .c under src/ that defines `symbol`. None if not unique."""
    patterns = [
        re.compile(r"^\s*(?:const\s+)?\w[\w\s\*]*\b" + re.escape(symbol) + r"\s*=", re.M),
        re.compile(r"^\s*(?:void|int|bool)\s+" + re.escape(symbol) + r"\s*\([^;]*\)\s*\{", re.M),
    ]
    hits = []
    for c in sorted((REPO / "src").rglob("*.c")):
        # Vendored trees are not ours to police and are huge.
        if "lib/" in c.relative_to(REPO).as_posix():
            continue
        try:
            body = strip_comments(c.read_text(errors="ignore"))
        except OSError:
            continue
        if symbol not in body:
            continue
        if any(p.search(body) for p in patterns):
            hits.append(c.relative_to(REPO).as_posix())
    return hits[0] if len(hits) == 1 else None


def main():
    problems = []
    unresolved = []

    for reg_path, patterns in REGISTRIES.items():
        reg_file = REPO / reg_path
        if not reg_file.is_file():
            print(f"error: registry not found: {reg_path}", file=sys.stderr)
            return 2

        raw = reg_file.read_text(errors="ignore")
        body = strip_comments(raw)

        if has_preprocessor_guard(body):
            print(
                f"error: {reg_path} now has a preprocessor guard.\n"
                "       This check assumes the registries are unconditional, so it can no\n"
                "       longer tell a real omission from a deliberate exclusion. Teach it the\n"
                "       guard before re-enabling.",
                file=sys.stderr,
            )
            return 2

        symbols = sorted({m for p in patterns for m in p.findall(body)})
        if not symbols:
            print(f"error: no driver registrations parsed out of {reg_path}", file=sys.stderr)
            return 2

        required = {}
        for sym in symbols:
            src = find_definition(sym)
            if src is None:
                unresolved.append((reg_path, sym))
            else:
                required[sym] = src

        registry_name = Path(reg_path).name
        print(f"{reg_path}: {len(symbols)} drivers registered unconditionally")

        for build_path in BUILD_FILES:
            bf = REPO / build_path
            if not bf.is_file():
                continue
            text = bf.read_text(errors="ignore")
            if registry_name not in text:
                continue  # this backend does not build that registry at all
            missing = [
                (sym, src) for sym, src in sorted(required.items())
                if Path(src).name not in text
            ]
            status = "OK" if not missing else f"{len(missing)} MISSING"
            print(f"    {build_path:<28} compiles {registry_name}  -> {status}")
            for sym, src in missing:
                problems.append((build_path, registry_name, sym, src))

    if unresolved:
        print("\nCould not resolve a unique definition for:", file=sys.stderr)
        for reg, sym in unresolved:
            print(f"  {sym}  (registered by {reg})", file=sys.stderr)
        print(
            "\nEvery registered driver should have exactly one defining .c under src/.\n"
            "Fix the symbol or teach this script about it -- an unresolved symbol is an\n"
            "unchecked one.",
            file=sys.stderr,
        )
        return 2

    if problems:
        print("\n" + "=" * 72, file=sys.stderr)
        print("Driver registry drift: a backend registers a driver it never compiles.", file=sys.stderr)
        print("=" * 72, file=sys.stderr)
        for build_path, registry_name, sym, src in problems:
            print(
                f"\n  {build_path}\n"
                f"      compiles {registry_name}, which references  {sym}\n"
                f"      but never lists                             {src}\n"
                f"      -> undefined reference to `{sym}` at link time",
                file=sys.stderr,
            )
        print(
            f"\nAdd the listed source to that backend's list, next to the other drivers.\n"
            f"{len(problems)} problem(s).",
            file=sys.stderr,
        )
        return 1

    print("\nAll backends compile every driver their registries reference.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
