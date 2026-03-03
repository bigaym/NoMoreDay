#!/usr/bin/env python3
from __future__ import annotations

import argparse
import copy
import json
from pathlib import Path
from typing import Any

CANONICAL_SCHEMA_VERSION = 3
MIGRATABLE_SCHEMA_VERSIONS = {1}
REJECTED_SCHEMA_VERSIONS = {2}

LEGACY_HEADER_KEYS = (
    "schema_version",
    "format_version",
)

EVENT_TYPE_ALIASES = {
    "shadow_pulse": "ShadowPulse",
    "shadowpulse": "ShadowPulse",
    "light_profile_blend": "LightProfileBlend",
    "lightprofileblend": "LightProfileBlend",
    "material_phase_shift": "MaterialPhaseShift",
    "materialphaseshift": "MaterialPhaseShift",
}


def _canonicalize_event_type(event_type: Any) -> Any:
    if not isinstance(event_type, str):
        return event_type

    normalized = event_type.strip().lower()
    return EVENT_TYPE_ALIASES.get(normalized, event_type)


def _canonicalize_events(document: dict[str, Any]) -> None:
    events = document.get("events")
    if not isinstance(events, list):
        return

    for event in events:
        if not isinstance(event, dict):
            continue

        event_type = event.get("type")
        event["type"] = _canonicalize_event_type(event_type)

        if "tierPolicy" not in event:
            event["tierPolicy"] = "skip"


def migrate_document(source: dict[str, Any]) -> dict[str, Any]:
    for key in LEGACY_HEADER_KEYS:
        if key in source and "vfx_schema_version" not in source:
            raise ValueError(
                f"unsupported legacy format: found '{key}' without 'vfx_schema_version'"
            )

    schema_version = source.get("vfx_schema_version")
    if not isinstance(schema_version, int):
        raise ValueError("missing required integer 'vfx_schema_version'")

    if schema_version in REJECTED_SCHEMA_VERSIONS:
        raise ValueError(
            f"unsupported old vfx schema version {schema_version}; no compatibility migration"
        )

    if (
        schema_version not in MIGRATABLE_SCHEMA_VERSIONS
        and schema_version != CANONICAL_SCHEMA_VERSION
    ):
        raise ValueError(
            f"unsupported vfx schema version {schema_version}; expected one of [1, 3]"
        )

    migrated = copy.deepcopy(source)
    migrated["vfx_schema_version"] = CANONICAL_SCHEMA_VERSION
    _canonicalize_events(migrated)
    return migrated


def migrate_file(input_path: Path, output_path: Path) -> None:
    source = json.loads(input_path.read_text(encoding="utf-8"))
    migrated = migrate_document(source)
    output_path.write_text(json.dumps(migrated, indent=2) + "\n", encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Migrate legacy VFX sequence JSON to canonical schema v3"
    )
    parser.add_argument(
        "--input", type=Path, required=True, help="source VFX JSON path"
    )
    parser.add_argument(
        "--output",
        type=Path,
        help="destination path (defaults to input path for in-place migration)",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    input_path = args.input
    output_path = args.output if args.output is not None else args.input

    try:
        migrate_file(input_path, output_path)
    except (OSError, json.JSONDecodeError, ValueError) as exc:
        print(f"[vfx-migrate] ERROR: {exc}")
        return 1

    print(
        f"[vfx-migrate] migrated {input_path.as_posix()} -> {output_path.as_posix()} "
        f"(schema {CANONICAL_SCHEMA_VERSION})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
