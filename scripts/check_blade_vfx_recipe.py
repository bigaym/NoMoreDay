#!/usr/bin/env python3
"""
Minimal validator for Blade Ascendant VFX recipe config.

Usage:
  python scripts/check_blade_vfx_recipe.py --check
  python scripts/check_blade_vfx_recipe.py --check --file assets/data/vfx/blade_ascendant_v3.json
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


VALID_EVENT_NAMES = {
    "CastStart",
    "CastImpact",
    "TriggerProc",
    "EmpoweredConsume",
    "BuffEnter",
    "BuffExit",
    "TransmuterSwitch",
    "KeystoneActivate",
    "*",
}

VALID_ACTION_KINDS = {
    "Overlay",
    "ParticleBurst",
    "TrailStroke",
    "DistortionPulse",
    "ResistOverlay",
}

VALID_ROLE_NAMES = {
    "Keystone",
    "Trigger",
    "Synergy",
    "Transmuter",
}

EVENT_NAME_TO_ID = {
    "CastStart": 0,
    "CastImpact": 1,
    "TriggerProc": 2,
    "EmpoweredConsume": 3,
    "BuffEnter": 4,
    "BuffExit": 5,
    "TransmuterSwitch": 6,
    "KeystoneActivate": 7,
}

REQUIRED_BASE_FORM_EVENTS = {
    1: {"CastStart", "CastImpact", "TriggerProc", "EmpoweredConsume"},
    2: {"CastImpact", "TriggerProc"},
    3: {"CastStart", "CastImpact", "TriggerProc", "BuffEnter"},
    4: {"CastStart", "CastImpact", "TriggerProc", "BuffEnter"},
    5: {"CastStart", "CastImpact", "EmpoweredConsume"},
    6: {"CastStart", "TriggerProc", "BuffEnter"},
    7: {"CastImpact", "TriggerProc"},
    8: {"CastStart", "CastImpact", "TriggerProc", "BuffExit"},
    9: {"CastStart", "CastImpact", "TriggerProc", "BuffEnter", "BuffExit"},
}


def _fail(message: str) -> None:
    raise ValueError(message)


def _is_int_or_wildcard(value: Any, *, min_value: int, max_value: int | None = None) -> bool:
    if isinstance(value, int):
        if value < min_value:
            return False
        if max_value is not None and value > max_value:
            return False
        return True
    return value == "*"


def validate_recipe_config(data: dict[str, Any]) -> None:
    if not isinstance(data.get("version"), int) or data["version"] < 1:
        _fail("version must be integer >= 1")

    recipes = data.get("recipes")
    if not isinstance(recipes, list) or not recipes:
        _fail("recipes must be a non-empty array")

    for idx, recipe in enumerate(recipes):
        prefix = f"recipes[{idx}]"
        if not isinstance(recipe, dict):
            _fail(f"{prefix} must be an object")
        if not recipe.get("name"):
            _fail(f"{prefix}.name is required")
        selector = recipe.get("selector")
        if not isinstance(selector, dict):
            _fail(f"{prefix}.selector must be an object")

        skill_id = selector.get("skillId", "*")
        if not _is_int_or_wildcard(skill_id, min_value=0):
            _fail(f"{prefix}.selector.skillId must be integer >= 0 or '*'")

        event_type = selector.get("eventType", "*")
        if isinstance(event_type, int):
            if event_type < 0:
                _fail(f"{prefix}.selector.eventType integer must be >= 0")
        elif event_type not in VALID_EVENT_NAMES:
            _fail(f"{prefix}.selector.eventType invalid: {event_type}")

        element_type = selector.get("elementType", "*")
        if not _is_int_or_wildcard(element_type, min_value=0, max_value=4):
            _fail(f"{prefix}.selector.elementType must be 0..4 or '*'")

        resist_type = selector.get("resistDebuffType", "*")
        if not _is_int_or_wildcard(resist_type, min_value=0, max_value=5):
            _fail(f"{prefix}.selector.resistDebuffType must be 0..5 or '*'")

        role_mask = selector.get("requiredNodeRoleMask", 0)
        if isinstance(role_mask, int):
            if role_mask < 0:
                _fail(f"{prefix}.selector.requiredNodeRoleMask must be >= 0")
        elif isinstance(role_mask, list):
            if not all(isinstance(role, str) and role in VALID_ROLE_NAMES for role in role_mask):
                _fail(
                    f"{prefix}.selector.requiredNodeRoleMask array must contain "
                    "Keystone/Trigger/Synergy/Transmuter"
                )
        else:
            _fail(f"{prefix}.selector.requiredNodeRoleMask must be int or string array")

        actions = recipe.get("actions")
        if not isinstance(actions, list) or not actions:
            _fail(f"{prefix}.actions must be a non-empty array")

        for action_idx, action in enumerate(actions):
            action_prefix = f"{prefix}.actions[{action_idx}]"
            if not isinstance(action, dict):
                _fail(f"{action_prefix} must be an object")
            kind = action.get("kind")
            if kind not in VALID_ACTION_KINDS:
                _fail(f"{action_prefix}.kind invalid: {kind}")

            count = action.get("count", 1)
            if not isinstance(count, int) or count < 1:
                _fail(f"{action_prefix}.count must be integer >= 1")

            alpha = action.get("alpha", 1.0)
            if not isinstance(alpha, (int, float)) or alpha < 0.0 or alpha > 1.0:
                _fail(f"{action_prefix}.alpha must be in [0,1]")

    _validate_base_form_coverage(recipes)


def _normalize_event_name(value: Any) -> str | None:
    if isinstance(value, str):
        return value
    if isinstance(value, int):
        for event_name, event_id in EVENT_NAME_TO_ID.items():
            if event_id == value:
                return event_name
    return None


def _validate_base_form_coverage(recipes: list[dict[str, Any]]) -> None:
    discovered: dict[int, set[str]] = {skill_id: set() for skill_id in REQUIRED_BASE_FORM_EVENTS}

    for recipe in recipes:
        selector = recipe.get("selector")
        if not isinstance(selector, dict):
            continue

        skill_id = selector.get("skillId")
        if not isinstance(skill_id, int) or skill_id not in discovered:
            continue

        event_name = _normalize_event_name(selector.get("eventType", "*"))
        if event_name is None or event_name == "*":
            continue
        discovered[skill_id].add(event_name)

    missing: list[str] = []
    for skill_id, required_events in REQUIRED_BASE_FORM_EVENTS.items():
        missing_events = sorted(required_events - discovered[skill_id])
        if missing_events:
            missing.append(f"skill {skill_id}: {', '.join(missing_events)}")

    if missing:
        _fail("base form coverage missing required event recipes: " + " | ".join(missing))


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate blade vfx recipe JSON.")
    parser.add_argument("--check", action="store_true", help="run validation")
    parser.add_argument(
        "--file",
        default="assets/data/vfx/blade_ascendant_v3.json",
        help="recipe json file path",
    )
    args = parser.parse_args()

    if not args.check:
        parser.error("only --check mode is supported")

    file_path = Path(args.file)
    data = json.loads(file_path.read_text(encoding="utf-8"))
    validate_recipe_config(data)
    print(f"OK: {file_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
