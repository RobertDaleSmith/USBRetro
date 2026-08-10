#!/usr/bin/env python3
"""Assert every app build target is either in a CI matrix or explicitly excluded.

Why this exists
---------------
The release is `files: releases/*` over whatever the build matrices produced, so
**matrix membership is release membership** — an app target that is in no matrix
does not exist for users, no matter what the Makefile or the CHANGELOG says.
Nothing enumerated the targets and checked that, so the default outcome for a new
app was silence: as of 2026-08-09, 41 targets had never shipped a UF2, several of
them announced in CHANGELOG.md as shipped features.

`APPS` in the Makefile is not a usable denominator — it is a second
hand-maintained list with its own drift (it omits 21 targets the matrix builds).
The authoritative set is the build targets themselves:

  * pico family  — every `$(call build_app,<target>)` in the Makefile. The matrix
    runs `make <app>` literally, so the target name *is* the build identity and an
    exact name comparison is sound.
  * esp / nrf    — the hand-written recipes in the Makefile. These jobs bypass
    `make` and drive `idf.py` / `west` themselves, so a matrix `app:` value is an
    **artifact name chosen independently of the make target**. Comparing names
    there gives false positives: `bt2usb_xiao_esp32s3` ships as `bt2usb_esp32s3`,
    and `usb2usb_feather_nrf52840` ships as `usb2usb_feather_nrf52840_max3421`.
    Both are covered. So esp/nrf are compared on build *identity* — the
    (board, app) pair — which the Makefile recipe and the matrix entry both state.

Excluding a target is fine; excluding it *silently* is the bug. Anything not in a
matrix must be listed in .github/ci-excluded-apps.txt with a reason.

Exit status: 0 = every target accounted for, 1 = something is unaccounted for.
Run locally with `python3 tools/check_build_coverage.py` from the repo root.
"""

from __future__ import annotations

import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAKEFILE = os.path.join(REPO, "Makefile")
WORKFLOW = os.path.join(REPO, ".github", "workflows", "build.yml")
EXCLUDES = os.path.join(REPO, ".github", "ci-excluded-apps.txt")

ESP_SUFFIX = "_esp32s3"
NRF_SUFFIX = "_nrf52840"


def read(path: str) -> str:
    with open(path, encoding="utf-8") as fh:
        return fh.read()


# Defaults the sub-Makefiles apply when a recipe passes no override. Kept here
# rather than parsed so a change over there surfaces as a coverage diff to review.
SUB_DEFAULTS = {
    "esp": {"board": "xiao_esp32s3", "app": "bt2usb"},   # esp/Makefile BOARD?=/CONFIG_APP?=
    "nrf": {"board": "xiao_ble", "app": "bt2usb"},       # nrf/Makefile BOARD?=/APP_TYPE?=
}


def pico_targets(makefile: str) -> set[str]:
    """Pico-family targets. The matrix runs `make <name>`, so the name is enough."""
    return set(re.findall(r"\$\(call build_app,\s*([A-Za-z0-9_]+)", makefile))


def sub_targets(makefile: str, family: str) -> dict[str, tuple[str, str]]:
    """esp/nrf Makefile targets mapped to the (board, app) pair they actually build.

    Rule heads are matched at column 0 rather than via `.PHONY`, so a missing
    `.PHONY` line cannot hide a target from this check.
    """
    suffix = ESP_SUFFIX if family == "esp" else NRF_SUFFIX
    app_var = "CONFIG_APP" if family == "esp" else "APP_TYPE"
    defaults = SUB_DEFAULTS[family]

    out: dict[str, tuple[str, str]] = {}
    for match in re.finditer(r"^([A-Za-z0-9_]+):(?!=)[^\n]*\n((?:\t[^\n]*\n)+)", makefile, re.M):
        name, body = match.group(1), match.group(2)
        if not name.endswith(suffix):
            continue
        board = re.search(r"\bBOARD=(\S+)", body)
        app = re.search(rf"\b{app_var}=(\S+)", body)
        out[name] = (
            board.group(1) if board else defaults["board"],
            app.group(1) if app else defaults["app"],
        )
    return out


def rpi_matrix(workflow: str) -> set[str]:
    block = re.search(
        r"^  build-rpi:.*?\n        app:\n((?:\s+- \S+\n)+)", workflow, re.S | re.M
    )
    if not block:
        sys.exit("check-build-coverage: could not parse the build-rpi matrix")
    return {line.strip().lstrip("- ").strip() for line in block.group(1).splitlines()}


def sub_matrix(workflow: str, family: str) -> dict[str, tuple[str, str]]:
    """esp/nrf matrix entries mapped to the (board, app) pair they build."""
    app_key = "config_app" if family == "esp" else "app_type"
    block = re.search(
        rf"^  build-{family}:.*?\n        include:\n(.*?)(?=\n  [a-z][\w-]*:\n)",
        workflow,
        re.S | re.M,
    )
    if not block:
        sys.exit(f"check-build-coverage: could not parse the build-{family} matrix")

    out: dict[str, tuple[str, str]] = {}
    for entry in re.split(r"\n(?=\s+- app:)", block.group(1)):
        name = re.search(r"- app:\s*(\S+)", entry)
        board = re.search(r"\bboard:\s*(\S+)", entry)
        app = re.search(rf"\b{app_key}:\s*(\S+)", entry)
        if not (name and board and app):
            continue
        out[name.group(1)] = (board.group(1), app.group(1))
    return out


def excluded(path: str) -> dict[str, str]:
    """Parse `target  # reason` lines. A reason is mandatory."""
    if not os.path.exists(path):
        return {}
    out: dict[str, str] = {}
    for lineno, raw in enumerate(read(path).splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        target, _, reason = line.partition("#")
        target, reason = target.strip(), reason.strip()
        if not reason:
            sys.exit(
                f"{os.path.relpath(path, REPO)}:{lineno}: '{target}' has no reason. "
                "Every exclusion must say why, so it can be re-decided later."
            )
        out[target] = reason
    return out


def main() -> int:
    makefile, workflow = read(MAKEFILE), read(WORKFLOW)
    skips = excluded(EXCLUDES)
    rel = os.path.relpath(EXCLUDES, REPO)

    problems: list[str] = []
    summary: list[str] = []
    covered: dict[str, set[str]] = {}

    # --- pico family: exact name comparison, because the matrix runs `make <app>`.
    pico, matrix = pico_targets(makefile), rpi_matrix(workflow)
    covered["rpi"] = pico & matrix
    for target in sorted(pico - matrix - set(skips)):
        problems.append(
            f"rpi: '{target}' is a build target in no CI matrix and not in {rel}. "
            "It ships no UF2, so users cannot flash it. Add it to the matrix, or "
            "list it with a reason."
        )
    # An entry with no Makefile target is a build failure waiting to happen.
    for target in sorted(matrix - pico):
        problems.append(
            f"rpi: matrix builds '{target}' but the Makefile has no such target — "
            f"`make {target}` would fail."
        )
    summary.append(f"rpi: {len(pico)} targets, {len(covered['rpi'])} in matrix, "
                   f"{len(pico & set(skips))} excluded")

    # --- esp/nrf: compare build identity, not name. See the module docstring.
    for family in ("esp", "nrf"):
        built = sub_targets(makefile, family)
        entries = sub_matrix(workflow, family)
        shipped = set(entries.values())

        covered[family] = {t for t, ident in built.items() if ident in shipped}
        for target in sorted(set(built) - covered[family] - set(skips)):
            board, app = built[target]
            problems.append(
                f"{family}: '{target}' builds ({board}, {app}), which no "
                f"build-{family} matrix entry produces, and it is not in {rel}. "
                "It ships no UF2. Add it to the matrix, or list it with a reason."
            )
        # A matrix entry whose (board, app) pair no Makefile target builds means the
        # workflow and the Makefile have drifted apart on how to build it.
        for name, ident in sorted(entries.items()):
            if ident not in set(built.values()):
                problems.append(
                    f"{family}: matrix entry '{name}' builds {ident}, which no "
                    "Makefile target produces — the two have drifted."
                )
        summary.append(f"{family}: {len(built)} targets, {len(covered[family])} in matrix, "
                       f"{len(set(built) & set(skips))} excluded")

    # A target both shipped and excluded means the exclusion is stale; left alone it
    # rots into a false record of what ships.
    all_covered = set().union(*covered.values())
    for target in sorted(all_covered & set(skips)):
        problems.append(
            f"'{target}' is built by a CI matrix AND excluded in {rel}. "
            "Drop the stale exclusion."
        )

    # `APPS` drives `make all`, which is how a change gets validated locally. It
    # does not decide what ships, so this is advisory and never fails the build.
    notes: list[str] = []
    apps = re.search(r"^APPS := (.*)$", makefile, re.M)
    if apps:
        gap = sorted(covered["rpi"] - set(apps.group(1).split()))
        if gap:
            notes.append(
                f"note: {len(gap)} targets are in the CI matrix but not in the "
                "Makefile's APPS list, so `make all` does not build them: "
                + ", ".join(gap)
            )

    print("build coverage")
    for line in summary:
        print(f"  {line}")
    for line in notes:
        print(f"  {line}")

    if problems:
        print("\nunaccounted-for build targets:", file=sys.stderr)
        for line in problems:
            print(f"  - {line}", file=sys.stderr)

    step_summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if step_summary:
        with open(step_summary, "a", encoding="utf-8") as fh:
            fh.write("## Build coverage\n\n")
            for line in summary:
                fh.write(f"- {line}\n")
            for line in notes:
                fh.write(f"- {line}\n")
            if problems:
                fh.write("\n### Unaccounted-for build targets\n\n")
                for line in problems:
                    fh.write(f"- {line}\n")
            else:
                fh.write("\nEvery build target is in a matrix or explicitly excluded.\n")

    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
