#!/usr/bin/env python3
from __future__ import annotations

import re
from pathlib import Path

import gen_map_monster_modifier_v2 as gen_map


REPO_ROOT = Path(__file__).resolve().parent.parent
MODIFIER_CONTEXT = REPO_ROOT / "src/game/systems/modifier/ModifierContext.hpp"
MONSTER_AFFIX_SYSTEM = REPO_ROOT / "src/game/systems/combat/MonsterAffixSystem.hpp"
RUNTIME_GENERATOR = REPO_ROOT / "scripts/gen_modifier_runtime_v2.py"

OPCODE_RE = re.compile(r"\bMONSTER_BEHAVIOR_[A-Z0-9_]+\b")
DISPATCH_OPCODE_RE = re.compile(r"ModifierOpCode::(MONSTER_BEHAVIOR_[A-Z0-9_]+)")
RUNTIME_OPCODE_RE = re.compile(r'"(MONSTER_BEHAVIOR_[A-Z0-9_]+)"\s*:\s*\d+')


def _read_text(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except OSError as err:
        raise RuntimeError(f"failed to read '{path}': {err}") from err


def _collect_expected_behavior_opcodes() -> set[str]:
    expected: set[str] = set()
    for opcode_set in gen_map.MONSTER_BEHAVIOR_OPCODES_BY_AFFIX.values():
        expected.update(opcode_set)
    return expected


def main() -> int:
    try:
        expected = _collect_expected_behavior_opcodes()
        context_text = _read_text(MODIFIER_CONTEXT)
        dispatch_text = _read_text(MONSTER_AFFIX_SYSTEM)
        runtime_text = _read_text(RUNTIME_GENERATOR)
    except RuntimeError as err:
        print(f"[behavior-dispatch] check failed: {err}")
        return 1

    declared = set(OPCODE_RE.findall(context_text))
    dispatched = set(DISPATCH_OPCODE_RE.findall(dispatch_text))
    runtime_mapped = set(RUNTIME_OPCODE_RE.findall(runtime_text))

    failures: list[str] = []

    missing_declared = sorted(expected - declared)
    if missing_declared:
        failures.append(
            "missing opcodes in ModifierContext.hpp: " + ", ".join(missing_declared)
        )

    missing_runtime = sorted(expected - runtime_mapped)
    if missing_runtime:
        failures.append(
            "missing opcode mappings in gen_modifier_runtime_v2.py: "
            + ", ".join(missing_runtime)
        )

    missing_dispatch = sorted(expected - dispatched)
    if missing_dispatch:
        failures.append(
            "missing dispatch checks in MonsterAffixSystem.hpp: "
            + ", ".join(missing_dispatch)
        )

    unknown_dispatch = sorted(dispatched - declared)
    if unknown_dispatch:
        failures.append(
            "dispatch references unknown opcodes (not in ModifierContext.hpp): "
            + ", ".join(unknown_dispatch)
        )

    if failures:
        print("[behavior-dispatch] check failed:")
        for failure in failures:
            print(f"  - {failure}")
        return 1

    print(
        "[behavior-dispatch] check passed: "
        f"expected={len(expected)}, dispatched={len(dispatched)}, runtime_mapped={len(runtime_mapped)}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
