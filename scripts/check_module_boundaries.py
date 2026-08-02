#!/usr/bin/env python3
"""Check the four-layer module-boundary ownership ledger.

Each candidate root and the PCH carries its own declarative policy of
forbidden direct project include prefixes.  The checker scans only direct
project includes (both quote and angle forms) and never follows transitive
includes, so no CMake target topology is consulted.
"""

from __future__ import annotations

import argparse
import json
import re
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
from typing import Any


SCHEMA_VERSION = "2.0"


@dataclass(frozen=True)
class CandidatePolicy:
    """One declarative scanning policy for a candidate root or the PCH."""

    path: str
    candidate_target: str
    candidate_layer: str
    current_owner: str
    forbidden_include_prefixes: tuple[str, ...]

    def scope_dict(self) -> dict[str, object]:
        """Return the ledger scope representation of this policy."""
        return {
            "path": self.path,
            "candidate_target": self.candidate_target,
            "candidate_layer": self.candidate_layer,
            "forbidden_include_prefixes": list(self.forbidden_include_prefixes),
        }


EXPECTED_CANDIDATE_ROOTS = (
    CandidatePolicy(
        "src/core", "NoMoreDayCore", "Core", "core_layer",
        ("engine/", "game/", "app/"),
    ),
    CandidatePolicy(
        "src/engine", "NoMoreDayEngine", "Engine", "engine_layer",
        ("game/", "app/"),
    ),
    CandidatePolicy(
        "src/game", "NoMoreDayGame", "Game", "game_layer",
        ("app/",),
    ),
)
EXPECTED_PCH_FILES = (
    CandidatePolicy(
        "src/pch.hpp", "EngineOwnedPch", "Engine-owned PCH",
        "engine_owned_pch", ("game/", "app/"),
    ),
)
# First-party header prefixes; anything else is treated as an external header.
PROJECT_INCLUDE_PREFIXES = ("app/", "core/", "engine/", "game/")
FUTURE_OWNER_LAYERS = {"Game", "App"}
ENTRY_FIELDS = {
    "id",
    "source",
    "line",
    "include_path",
    "candidate_target",
    "candidate_layer",
    "current_owner",
    "forbidden_include_prefixes",
    "future_owner_layer",
    "disposition",
    "milestone",
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
# The C preprocessor permits whitespace between '#' and the directive name and
# between the directive name and the header token (e.g. `# include <app/x.hpp>`
# or `#\tinclude\t"app/x.hpp"`), so both are matched.
INCLUDE_PATTERN = re.compile(r'^\s*#\s*include\s+(?:"([^"]+)"|<([^>]+)>)')


def _casefolded_startswith(value: str, prefixes: tuple[str, ...]) -> bool:
    """Return whether value starts with any prefix, compared case-insensitively.

    Windows include paths are case-insensitive, so the project-prefix policy
    judgment is made on the casefolded spelling.  Callers preserve the
    original include_path spelling for ledger evidence.
    """
    folded = value.casefold()
    return any(folded.startswith(prefix.casefold()) for prefix in prefixes)


class BoundaryInputError(ValueError):
    """Raised when the ledger or its scan configuration is malformed."""


@dataclass(frozen=True)
class ObservedInclude:
    """One directly observed project include that violates a candidate policy."""

    source: str
    line: int
    include_path: str
    candidate_target: str
    candidate_layer: str
    current_owner: str
    forbidden_include_prefixes: tuple[str, ...]

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
        description=(
            "Check direct project includes against the per-root "
            "module-boundary ledger"
        )
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
    if set(scope) != {"candidate_roots", "pch_files"}:
        raise BoundaryInputError(
            "ledger scope must contain exactly candidate_roots and pch_files"
        )
    roots = scope.get("candidate_roots")
    pch_files = scope.get("pch_files")
    expected_roots = [policy.scope_dict() for policy in EXPECTED_CANDIDATE_ROOTS]
    expected_pch = [policy.scope_dict() for policy in EXPECTED_PCH_FILES]
    if roots != expected_roots:
        raise BoundaryInputError("scope.candidate_roots do not match checker policy")
    if pch_files != expected_pch:
        raise BoundaryInputError("scope.pch_files do not match checker policy")

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
        # bool is a subclass of int in Python, so reject it explicitly to
        # keep JSON `line` strictly an integer (never true/false).
        if (
            isinstance(entry["line"], bool)
            or not isinstance(entry["line"], int)
            or entry["line"] < 1
        ):
            raise BoundaryInputError(f"entries[{index}].line must be a positive integer")
        key = (source, entry["line"], include_path)
        if key in seen_keys:
            raise BoundaryInputError(f"duplicate evidence key: {key}")
        seen_keys.add(key)
        if entry_id != f"{source}:{entry['line']}:{include_path}":
            raise BoundaryInputError(f"entries[{index}].id does not match evidence key")
        for field in (
            "candidate_target", "candidate_layer", "current_owner",
            "future_owner_layer", "disposition", "milestone",
        ):
            _as_non_empty_string(entry[field], f"entries[{index}].{field}")
        policy = _candidate_for_source(source)
        if policy is None:
            raise BoundaryInputError(
                f"entries[{index}].source is outside checker policy: {source}"
            )
        for field, expected_value in zip(
            ("candidate_target", "candidate_layer", "current_owner"),
            (
                policy.candidate_target,
                policy.candidate_layer,
                policy.current_owner,
            ),
        ):
            if entry[field] != expected_value:
                raise BoundaryInputError(
                    f"entries[{index}].{field} must be {expected_value!r}"
                )
        prefixes = entry["forbidden_include_prefixes"]
        if prefixes != list(policy.forbidden_include_prefixes):
            raise BoundaryInputError(
                f"entries[{index}].forbidden_include_prefixes must match "
                f"candidate policy: {list(policy.forbidden_include_prefixes)!r}"
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
    return loaded


def _candidate_for_source(source: str) -> CandidatePolicy | None:
    for policy in EXPECTED_PCH_FILES:
        if source == policy.path:
            return policy
    for policy in EXPECTED_CANDIDATE_ROOTS:
        if source == policy.path or source.startswith(f"{policy.path}/"):
            return policy
    return None


def scan_sources(repo_root: Path) -> list[ObservedInclude]:
    """Scan configured roots and PCH files for direct project includes.

    Args:
        repo_root: Repository root to scan.

    Returns:
        Sorted observed reverse includes.

    Raises:
        BoundaryInputError: If a configured source root or PCH is missing.
    """
    source_paths: set[Path] = set()
    for policy in EXPECTED_CANDIDATE_ROOTS:
        root_path = _resolve_relative(
            repo_root, policy.path, "checker candidate root"
        )
        if not root_path.is_dir():
            raise BoundaryInputError(f"candidate source root not found: {root_path}")
        source_paths.update(
            path for path in root_path.rglob("*")
            if path.is_file() and path.suffix.lower() in SOURCE_EXTENSIONS
        )
    for policy in EXPECTED_PCH_FILES:
        pch_path = _resolve_relative(repo_root, policy.path, "checker PCH")
        if not pch_path.is_file():
            raise BoundaryInputError(f"candidate PCH not found: {pch_path}")
        source_paths.add(pch_path)

    observed: list[ObservedInclude] = []
    for source_path in sorted(source_paths):
        source = source_path.relative_to(repo_root).as_posix()
        policy = _candidate_for_source(source)
        if policy is None:
            raise BoundaryInputError(
                f"scanned source is outside configured scope: {source}"
            )
        try:
            lines = source_path.read_text(encoding="utf-8-sig").splitlines()
        except (OSError, UnicodeError) as exc:
            raise BoundaryInputError(
                f"cannot read candidate source {source}: {exc}"
            ) from exc
        for line_number, line in enumerate(lines, start=1):
            match = INCLUDE_PATTERN.match(line)
            if not match:
                continue
            include_path = match.group(1) or match.group(2)
            # Policy judgments are case-insensitive (Windows paths); the raw
            # include_path spelling is retained as ledger evidence below.
            if not _casefolded_startswith(include_path, PROJECT_INCLUDE_PREFIXES):
                continue
            if _casefolded_startswith(include_path, policy.forbidden_include_prefixes):
                observed.append(
                    ObservedInclude(
                        source, line_number, include_path,
                        policy.candidate_target, policy.candidate_layer,
                        policy.current_owner, policy.forbidden_include_prefixes,
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
            "forbidden_include_prefixes": list(item.forbidden_include_prefixes),
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
