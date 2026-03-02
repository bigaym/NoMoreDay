#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import re
from collections import Counter, defaultdict
from dataclasses import dataclass
from datetime import UTC, datetime
from pathlib import Path


MARKERS: tuple[tuple[str, re.Pattern[str]], ...] = (
    ("v2", re.compile(r"(?<![A-Za-z0-9_])V2(?![A-Za-z0-9_])", re.IGNORECASE)),
    ("v3", re.compile(r"(?<![A-Za-z0-9_])V3(?![A-Za-z0-9_])", re.IGNORECASE)),
    ("v4", re.compile(r"(?<![A-Za-z0-9_])V4(?![A-Za-z0-9_])", re.IGNORECASE)),
    ("v5", re.compile(r"(?<![A-Za-z0-9_])V5(?![A-Za-z0-9_])", re.IGNORECASE)),
    ("legacy", re.compile(r"\blegacy\b", re.IGNORECASE)),
    ("deprecated", re.compile(r"\bdeprecated\b", re.IGNORECASE)),
    ("fallback", re.compile(r"\bfallback\b", re.IGNORECASE)),
)

TEXT_EXTENSIONS = {
    ".h",
    ".hpp",
    ".hh",
    ".hxx",
    ".c",
    ".cc",
    ".cpp",
    ".cxx",
    ".ixx",
    ".inl",
    ".ipp",
    ".txt",
}

RUNTIME_BRANCH_HINTS = re.compile(
    r"\b(if|else|switch|case|fallback|compat|legacy)\b", re.IGNORECASE
)
METADATA_HINTS = re.compile(
    r"\b(version|schema|abi|enum|label|name|constant|id)\b", re.IGNORECASE
)


@dataclass(slots=True)
class MatchEntry:
    marker: str
    line: int
    column: int
    classification: str
    excerpt: str


def classify_match(line_text: str) -> str:
    if RUNTIME_BRANCH_HINTS.search(line_text):
        return "removable_runtime_branch"
    if METADATA_HINTS.search(line_text):
        return "metadata_only"
    return "migration_path_dependent"


def scan_file(
    path: Path, root: Path
) -> tuple[list[dict[str, object]], Counter[str], Counter[str]]:
    relative_path = path.relative_to(root).as_posix()
    marker_counts: Counter[str] = Counter()
    class_counts: Counter[str] = Counter()
    matches: list[dict[str, object]] = []

    with path.open("r", encoding="utf-8", errors="ignore") as file_stream:
        for line_number, line_text in enumerate(file_stream, start=1):
            for marker_name, marker_regex in MARKERS:
                for hit in marker_regex.finditer(line_text):
                    classification = classify_match(line_text)
                    marker_counts[marker_name] += 1
                    class_counts[classification] += 1
                    matches.append(
                        {
                            "file": relative_path,
                            "marker": marker_name,
                            "line": line_number,
                            "column": hit.start() + 1,
                            "classification": classification,
                            "excerpt": line_text.strip()[:240],
                        }
                    )
    return matches, marker_counts, class_counts


def collect_scan_paths(src_root: Path) -> list[Path]:
    return [
        file_path
        for file_path in src_root.rglob("*")
        if file_path.is_file() and file_path.suffix.lower() in TEXT_EXTENSIONS
    ]


def generate_inventory_data(repo_root: Path, src_relative: str) -> dict[str, object]:
    src_root = repo_root / src_relative
    if not src_root.exists() or not src_root.is_dir():
        raise FileNotFoundError(f"source directory not found: {src_root}")

    scan_paths = collect_scan_paths(src_root)
    marker_counts: Counter[str] = Counter()
    class_counts: Counter[str] = Counter()
    file_match_counts: Counter[str] = Counter()
    matches: list[dict[str, object]] = []

    for file_path in scan_paths:
        file_matches, file_marker_counts, file_class_counts = scan_file(
            file_path, repo_root
        )
        if not file_matches:
            continue
        marker_counts.update(file_marker_counts)
        class_counts.update(file_class_counts)
        relative_path = file_path.relative_to(repo_root).as_posix()
        file_match_counts[relative_path] += len(file_matches)
        matches.extend(file_matches)

    hotspots = [
        {"file": file_name, "match_count": count}
        for file_name, count in file_match_counts.most_common(20)
    ]

    return {
        "generated_at_utc": datetime.now(UTC).isoformat(timespec="seconds"),
        "repo_root": str(repo_root),
        "scan_root": src_relative,
        "scanned_files": len(scan_paths),
        "files_with_matches": len(file_match_counts),
        "total_matches": len(matches),
        "markers": [marker_name for marker_name, _ in MARKERS],
        "marker_counts": dict(sorted(marker_counts.items())),
        "classification_counts": dict(sorted(class_counts.items())),
        "hotspots": hotspots,
        "matches": matches,
    }


def write_summary_markdown(summary_path: Path, inventory: dict[str, object]) -> None:
    hotspots: list[dict[str, object]] = inventory["hotspots"]  # type: ignore[assignment]
    markers: dict[str, int] = inventory["marker_counts"]  # type: ignore[assignment]
    classifications: dict[str, int] = inventory["classification_counts"]  # type: ignore[assignment]
    top_hotspots = hotspots[:8]

    lines: list[str] = [
        "# P0-1 Legacy/Version Inventory Summary",
        "",
        f"Generated at (UTC): {inventory['generated_at_utc']}",
        f"Scanned files: {inventory['scanned_files']}",
        f"Files with matches: {inventory['files_with_matches']}",
        f"Total marker matches: {inventory['total_matches']}",
        "",
        "## Marker counts",
        "",
    ]

    for marker_name in sorted(markers.keys()):
        lines.append(f"- {marker_name}: {markers[marker_name]}")

    lines.extend(
        [
            "",
            "## Classification counts (heuristic)",
            "",
        ]
    )
    for classification_name in sorted(classifications.keys()):
        lines.append(f"- {classification_name}: {classifications[classification_name]}")

    lines.extend(
        [
            "",
            "## Top hotspots by file",
            "",
        ]
    )

    if not top_hotspots:
        lines.append("- No marker matches found.")
    else:
        for hotspot in top_hotspots:
            lines.append(f"- {hotspot['file']}: {hotspot['match_count']} matches")

    summary_path.parent.mkdir(parents=True, exist_ok=True)
    summary_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def run_inventory(
    repo_root: Path, output_path: Path, summary_path: Path, src_relative: str
) -> int:
    try:
        inventory = generate_inventory_data(repo_root, src_relative)
    except FileNotFoundError as exc:
        print(f"[P0-1] ERROR: {exc}")
        return 1

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(inventory, indent=2), encoding="utf-8")
    write_summary_markdown(summary_path, inventory)

    print(f"[P0-1] Inventory written: {output_path}")
    print(f"[P0-1] Summary written:   {summary_path}")
    print(f"[P0-1] Scanned files: {inventory['scanned_files']}")
    print(
        f"[P0-1] Matches: {inventory['total_matches']} in {inventory['files_with_matches']} files"
    )
    return 0


def parse_args() -> argparse.Namespace:
    default_inventory = Path("docs/reports/four-pillars/phase-0/P0-1/inventory.json")
    default_summary = Path("docs/reports/four-pillars/phase-0/P0-1/summary.md")
    parser = argparse.ArgumentParser(
        description="Generate four-pillars Phase 0 legacy/version inventory"
    )
    parser.add_argument(
        "--repo-root", type=Path, default=Path(__file__).resolve().parents[1]
    )
    parser.add_argument(
        "--scan-root", default="src", help="Path relative to repo root to scan"
    )
    parser.add_argument("--output", type=Path, default=default_inventory)
    parser.add_argument("--summary", type=Path, default=default_summary)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    repo_root = args.repo_root.resolve()
    output_path = (repo_root / args.output).resolve()
    summary_path = (repo_root / args.summary).resolve()
    return run_inventory(repo_root, output_path, summary_path, args.scan_root)


if __name__ == "__main__":
    raise SystemExit(main())
