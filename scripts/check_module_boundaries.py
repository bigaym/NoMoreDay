#!/usr/bin/env python3
"""Check the MS-0 lower-layer reverse-dependency ownership ledger."""

from __future__ import annotations

import argparse
import json
import re
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
from typing import Any


SCHEMA_VERSION = "1.0"
EXPECTED_CANDIDATE_ROOTS = (
    ("src/engine", "NoMoreDayEngine", "Engine"),
    ("src/core", "NoMoreDayCore", "Core"),
)
EXPECTED_PCH_FILES = ("src/pch.hpp",)
FORBIDDEN_INCLUDE_PREFIXES = ("game/", "app/")
FUTURE_OWNER_LAYERS = {"Game", "App"}
P0_BLOCKER = "gpu_rendergraph_resource_foundation_20260726"
P0_DISPOSITION = "split_engine_primitive_and_game_adapter"
P0_MILESTONE = "MS-6"
REQUIRED_P0_SOURCES = frozenset({
    "src/engine/render/GPUEntitySystem.cpp",
    "src/engine/render/GPUEntitySystem.hpp",
    "src/engine/render/GPULootSystem.cpp",
    "src/engine/render/GPUParticleSystem.cpp",
    "src/engine/render/GPUSkillEffectSystem.hpp",
    "src/engine/render/RenderSystem.hpp",
    "src/engine/render/lighting/GlobalHeightField.cpp",
    "src/engine/render/lighting/LightManager.cpp",
    "src/engine/render/passes/FluidSimulationPass.cpp",
    "src/engine/render/passes/GICompositePass.cpp",
    "src/engine/render/passes/HeightShadowPass.cpp",
    "src/engine/render/passes/JFAPass.cpp",
    "src/engine/render/passes/LightCullingPass.cpp",
    "src/engine/render/passes/OccluderExtractPass.cpp",
    "src/engine/render/passes/RadianceCascadesPass.cpp",
    "src/engine/render/passes/ShadowBuildPass.cpp",
    "src/engine/vfx/VFXSequencerSystem.cpp",
})
ENTRY_FIELDS = {
    "id",
    "source",
    "line",
    "include_path",
    "candidate_target",
    "candidate_layer",
    "current_owner",
    "future_owner_layer",
    "disposition",
    "milestone",
    "p0_blocking",
}
DISPOSITIONS = {
    "move_to_game",
    "move_to_app",
    "split_engine_primitive_and_game_adapter",
    "complete_dto_contract",
    "remove_from_lower_pch",
    "remove_dead_code",
}
MILESTONES = {f"MS-{index}" for index in range(9)}
SOURCE_EXTENSIONS = {
    ".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx",
    ".inl", ".ipp",
}
INCLUDE_PATTERN = re.compile(r'^\s*#include\s+"([^"]+)"')


class BoundaryInputError(ValueError):
    """Raised when the ledger or its scan configuration is malformed."""


@dataclass(frozen=True)
class ObservedInclude:
    """One directly observed quoted reverse include."""

    source: str
    line: int
    include_path: str
    candidate_target: str
    candidate_layer: str
    current_owner: str

    @property
    def evidence_key(self) -> tuple[str, int, str]:
        """Return the mechanically stable source/line/include key."""
        return self.source, self.line, self.include_path


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    """Parse command-line options.

    Returns:
        Parsed command-line arguments.
    """
    parser = argparse.ArgumentParser(
        description="Check lower-layer game/app includes against the MS-0 ledger"
    )
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="Repository root path",
    )
    parser.add_argument(
        "--ledger",
        type=Path,
        default=Path(
            "docs/reports/modular-split-exe-lib-dll/ms-0/"
            "reverse-dependency-ledger.json"
        ),
        help="Ledger path relative to --repo-root",
    )
    return parser.parse_args(argv)


def _as_non_empty_string(value: Any, field: str) -> str:
    if not isinstance(value, str) or not value:
        raise BoundaryInputError(f"{field} must be a non-empty string")
    return value


def _resolve_relative(repo_root: Path, value: str, field: str) -> Path:
    relative = Path(value)
    if relative.is_absolute() or ".." in relative.parts:
        raise BoundaryInputError(f"{field} must be repository-relative: {value}")
    resolved = (repo_root / relative).resolve()
    try:
        resolved.relative_to(repo_root.resolve())
    except ValueError as exc:
        raise BoundaryInputError(f"{field} escapes repository root: {value}") from exc
    return resolved


def load_ledger(ledger_path: Path) -> dict[str, Any]:
    """Load and structurally validate the JSON ledger.

    Args:
        ledger_path: Path to the ledger JSON file.

    Returns:
        The validated ledger object.

    Raises:
        BoundaryInputError: If the file is missing, invalid JSON, or malformed.
    """
    if not ledger_path.is_file():
        raise BoundaryInputError(f"ledger not found: {ledger_path}")
    try:
        loaded = json.loads(ledger_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise BoundaryInputError(f"cannot read ledger {ledger_path}: {exc}") from exc
    if not isinstance(loaded, dict):
        raise BoundaryInputError("ledger root must be a JSON object")
    if loaded.get("schema_version") != SCHEMA_VERSION:
        raise BoundaryInputError(
            f"unsupported schema_version: {loaded.get('schema_version')!r}"
        )

    scope = loaded.get("scope")
    if not isinstance(scope, dict):
        raise BoundaryInputError("ledger scope must be a JSON object")
    roots = scope.get("candidate_roots")
    pch_files = scope.get("pch_files")
    prefixes = scope.get("forbidden_include_prefixes")
    expected_roots = [
        {
            "path": path,
            "candidate_target": target,
            "candidate_layer": layer,
        }
        for path, target, layer in EXPECTED_CANDIDATE_ROOTS
    ]
    if roots != expected_roots:
        raise BoundaryInputError("scope.candidate_roots do not match checker policy")
    if pch_files != list(EXPECTED_PCH_FILES):
        raise BoundaryInputError("scope.pch_files do not match checker policy")
    if prefixes != list(FORBIDDEN_INCLUDE_PREFIXES):
        raise BoundaryInputError(
            "scope.forbidden_include_prefixes must be exactly [\"game/\", \"app/\"]"
        )

    entries = loaded.get("entries")
    if not isinstance(entries, list):
        raise BoundaryInputError("ledger entries must be a list")
    seen_ids: set[str] = set()
    seen_keys: set[tuple[str, int, str]] = set()
    for index, entry in enumerate(entries):
        if not isinstance(entry, dict):
            raise BoundaryInputError(f"entries[{index}] must be an object")
        missing = ENTRY_FIELDS - set(entry)
        extra = set(entry) - ENTRY_FIELDS
        if missing or extra:
            raise BoundaryInputError(
                f"entries[{index}] fields invalid; missing={sorted(missing)}, "
                f"extra={sorted(extra)}"
            )
        entry_id = _as_non_empty_string(entry["id"], f"entries[{index}].id")
        source = _as_non_empty_string(entry["source"], f"entries[{index}].source")
        include_path = _as_non_empty_string(
            entry["include_path"], f"entries[{index}].include_path"
        )
        if entry_id in seen_ids:
            raise BoundaryInputError(f"duplicate entry id: {entry_id}")
        seen_ids.add(entry_id)
        if not isinstance(entry["line"], int) or entry["line"] < 1:
            raise BoundaryInputError(f"entries[{index}].line must be a positive integer")
        key = (source, entry["line"], include_path)
        if key in seen_keys:
            raise BoundaryInputError(f"duplicate evidence key: {key}")
        seen_keys.add(key)
        if entry_id != f"{source}:{entry['line']}:{include_path}":
            raise BoundaryInputError(f"entries[{index}].id does not match evidence key")
        for field in (
            "candidate_target", "candidate_layer", "current_owner",
            "future_owner_layer", "milestone",
        ):
            _as_non_empty_string(entry[field], f"entries[{index}].{field}")
        expected_candidate = _candidate_for_source(source)
        if expected_candidate is None:
            raise BoundaryInputError(
                f"entries[{index}].source is outside checker policy: {source}"
            )
        for field, expected_value in zip(
            ("candidate_target", "candidate_layer", "current_owner"),
            expected_candidate,
        ):
            if entry[field] != expected_value:
                raise BoundaryInputError(
                    f"entries[{index}].{field} must be {expected_value!r}"
                )
        if entry["future_owner_layer"] not in FUTURE_OWNER_LAYERS:
            raise BoundaryInputError(
                f"entries[{index}].future_owner_layer must be Game or App"
            )
        if entry["disposition"] not in DISPOSITIONS:
            raise BoundaryInputError(
                f"entries[{index}].disposition is not permitted: "
                f"{entry['disposition']!r}"
            )
        if entry["milestone"] not in MILESTONES:
            raise BoundaryInputError(
                f"entries[{index}].milestone is not one of MS-0..MS-8"
            )
        p0_blocking = entry["p0_blocking"]
        if p0_blocking not in (None, P0_BLOCKER):
            raise BoundaryInputError(
                f"entries[{index}].p0_blocking must be null or {P0_BLOCKER!r}"
            )
        if source in REQUIRED_P0_SOURCES and p0_blocking != P0_BLOCKER:
            raise BoundaryInputError(
                f"entries[{index}] source requires {P0_BLOCKER!r}"
            )
        if source not in REQUIRED_P0_SOURCES and p0_blocking == P0_BLOCKER:
            raise BoundaryInputError(
                f"entries[{index}] P0 blocker source is outside checker policy"
            )
        if p0_blocking == P0_BLOCKER and (
                entry["disposition"] != P0_DISPOSITION
                or entry["milestone"] != P0_MILESTONE):
            raise BoundaryInputError(
                f"entries[{index}] P0-blocked entries must use "
                f"{P0_DISPOSITION!r} and {P0_MILESTONE!r}"
            )
    return loaded


def _candidate_for_source(source: str) -> tuple[str, str, str] | None:
    if source in EXPECTED_PCH_FILES:
        return "LegacyLowerPch", "lower-layer PCH", "legacy_global_pch"
    for root_path, target, layer in EXPECTED_CANDIDATE_ROOTS:
        if source == root_path or source.startswith(f"{root_path}/"):
            return (
                target, layer, f"legacy_monolithic_{target}",
            )
    return None


def scan_sources(repo_root: Path) -> list[ObservedInclude]:
    """Scan configured lower roots and PCH files for direct quoted edges.

    Args:
        repo_root: Repository root to scan.

    Returns:
        Sorted observed reverse includes.

    Raises:
        BoundaryInputError: If a configured source root or PCH is missing.
    """
    source_paths: set[Path] = set()
    for root_path, _, _ in EXPECTED_CANDIDATE_ROOTS:
        root_path = _resolve_relative(
            repo_root, root_path, "checker candidate root"
        )
        if not root_path.is_dir():
            raise BoundaryInputError(f"candidate source root not found: {root_path}")
        source_paths.update(
            path for path in root_path.rglob("*")
            if path.is_file() and path.suffix.lower() in SOURCE_EXTENSIONS
        )
    for pch_file in EXPECTED_PCH_FILES:
        pch_path = _resolve_relative(repo_root, pch_file, "checker PCH")
        if not pch_path.is_file():
            raise BoundaryInputError(f"candidate PCH not found: {pch_path}")
        source_paths.add(pch_path)

    observed: list[ObservedInclude] = []
    for source_path in sorted(source_paths):
        source = source_path.relative_to(repo_root).as_posix()
        candidate = _candidate_for_source(source)
        if candidate is None:
            raise BoundaryInputError(
                f"scanned source is outside configured scope: {source}"
            )
        candidate_target, candidate_layer, current_owner = candidate
        try:
            lines = source_path.read_text(encoding="utf-8-sig").splitlines()
        except (OSError, UnicodeError) as exc:
            raise BoundaryInputError(
                f"cannot read candidate source {source}: {exc}"
            ) from exc
        for line_number, line in enumerate(lines, start=1):
            match = INCLUDE_PATTERN.match(line)
            if match and match.group(1).startswith(FORBIDDEN_INCLUDE_PREFIXES):
                observed.append(
                    ObservedInclude(
                        source, line_number, match.group(1), candidate_target,
                        candidate_layer, current_owner,
                    )
                )
    return sorted(observed, key=lambda item: item.evidence_key)


def compare_ledger(
    observed: list[ObservedInclude], ledger: dict[str, Any]
) -> list[str]:
    """Return unregistered, stale, or target-mismatched ledger evidence."""
    errors: list[str] = []
    observed_by_key = {item.evidence_key: item for item in observed}
    entries = ledger["entries"]
    entries_by_key = {
        (entry["source"], entry["line"], entry["include_path"]): entry
        for entry in entries
    }
    for key, item in observed_by_key.items():
        entry = entries_by_key.get(key)
        if entry is None:
            errors.append(
                "untracked reverse dependency: "
                f"{item.source}:{item.line} includes \"{item.include_path}\""
            )
            continue
        expected = {
            "candidate_target": item.candidate_target,
            "candidate_layer": item.candidate_layer,
            "current_owner": item.current_owner,
        }
        for field, expected_value in expected.items():
            if entry[field] != expected_value:
                errors.append(
                    f"target metadata mismatch at {item.source}:{item.line}: "
                    f"{field}={entry[field]!r}, expected={expected_value!r}"
                )
    for key, entry in entries_by_key.items():
        if key not in observed_by_key:
            errors.append(
                "stale ledger entry: "
                f"{entry['source']}:{entry['line']} includes \"{entry['include_path']}\""
            )
    return errors


def print_summary(observed: list[ObservedInclude], ledger: dict[str, Any]) -> None:
    """Print concise totals and ownership groups."""
    entries = ledger["entries"]
    print(
        f"[Module Boundary] Observed/ledger edges: {len(observed)}/{len(entries)}; "
        f"files: {len({item.source for item in observed})}"
    )
    grouped = Counter(
        (
            entry["candidate_target"], entry["future_owner_layer"],
            entry["disposition"], entry["milestone"],
        )
        for entry in entries
    )
    for (candidate, owner, disposition, milestone), count in sorted(grouped.items()):
        print(
            f"[Module Boundary] {candidate} -> {owner}; {disposition}; "
            f"{milestone}: {count}"
        )


def main(argv: list[str] | None = None) -> int:
    """Run the boundary check and return its process status."""
    args = parse_args(argv)
    repo_root = args.repo_root.resolve()
    ledger_path = args.ledger if args.ledger.is_absolute() else repo_root / args.ledger
    try:
        ledger = load_ledger(ledger_path.resolve())
        observed = scan_sources(repo_root)
    except (BoundaryInputError, OSError) as exc:
        print(f"[Module Boundary] ERROR: {exc}")
        return 2

    print_summary(observed, ledger)
    errors = compare_ledger(observed, ledger)
    if errors:
        print(f"[Module Boundary] FAILED: {len(errors)} violation(s).")
        for error in errors:
            print(f"[Module Boundary] - {error}")
        return 1
    print("[Module Boundary] PASS: ledger and observed reverse edges match.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
