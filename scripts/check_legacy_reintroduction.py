#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from pathlib import Path

from four_pillars_phase0_inventory import generate_inventory_data


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Check for legacy/version marker reintroduction against baseline inventory"
    )
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="Repository root path",
    )
    parser.add_argument(
        "--scan-root",
        default="src",
        help="Path relative to repo root to scan",
    )
    parser.add_argument(
        "--baseline",
        type=Path,
        default=Path("docs/reports/four-pillars/phase-0/P0-1/inventory.json"),
        help="Baseline inventory JSON path relative to repo root",
    )
    parser.add_argument(
        "--write-current",
        type=Path,
        default=None,
        help="Optional path to write current inventory JSON",
    )
    return parser.parse_args()


def load_baseline(baseline_path: Path) -> dict[str, object]:
    with baseline_path.open("r", encoding="utf-8") as file_stream:
        loaded = json.load(file_stream)
    if not isinstance(loaded, dict):
        raise ValueError(f"Baseline file is not a JSON object: {baseline_path}")
    return loaded


def extract_counts(source: dict[str, object], key: str) -> dict[str, int]:
    raw = source.get(key, {})
    if not isinstance(raw, dict):
        return {}
    counts: dict[str, int] = {}
    for name, value in raw.items():
        if isinstance(name, str):
            counts[name] = int(value)
    return counts


def collect_regressions(
    baseline_inventory: dict[str, object], current_inventory: dict[str, object]
) -> list[str]:
    regressions: list[str] = []

    baseline_total = int(baseline_inventory.get("total_matches", 0))
    current_total = int(current_inventory.get("total_matches", 0))
    if current_total > baseline_total:
        regressions.append(
            f"total marker matches increased: baseline={baseline_total}, current={current_total}"
        )

    baseline_markers = extract_counts(baseline_inventory, "marker_counts")
    current_markers = extract_counts(current_inventory, "marker_counts")
    marker_names = sorted(set(baseline_markers) | set(current_markers))
    for marker_name in marker_names:
        baseline_value = baseline_markers.get(marker_name, 0)
        current_value = current_markers.get(marker_name, 0)
        if current_value > baseline_value:
            regressions.append(
                "marker increased: "
                f"{marker_name} baseline={baseline_value}, current={current_value}"
            )

    baseline_classes = extract_counts(baseline_inventory, "classification_counts")
    current_classes = extract_counts(current_inventory, "classification_counts")
    baseline_runtime_branch = baseline_classes.get("removable_runtime_branch", 0)
    current_runtime_branch = current_classes.get("removable_runtime_branch", 0)
    if current_runtime_branch > baseline_runtime_branch:
        regressions.append(
            "removable_runtime_branch classification increased: "
            f"baseline={baseline_runtime_branch}, current={current_runtime_branch}"
        )

    return regressions


def print_summary(
    baseline_inventory: dict[str, object], current_inventory: dict[str, object]
) -> None:
    baseline_total = int(baseline_inventory.get("total_matches", 0))
    current_total = int(current_inventory.get("total_matches", 0))
    baseline_files = int(baseline_inventory.get("files_with_matches", 0))
    current_files = int(current_inventory.get("files_with_matches", 0))
    print(f"[Legacy Gate] Baseline total/files: {baseline_total}/{baseline_files}")
    print(f"[Legacy Gate] Current total/files:  {current_total}/{current_files}")


def main() -> int:
    args = parse_args()
    repo_root = args.repo_root.resolve()
    baseline_path = (repo_root / args.baseline).resolve()

    if not baseline_path.exists():
        print(f"[Legacy Gate] ERROR: baseline inventory not found: {baseline_path}")
        return 2

    try:
        baseline_inventory = load_baseline(baseline_path)
        current_inventory = generate_inventory_data(repo_root, args.scan_root)
    except Exception as exc:  # pylint: disable=broad-except
        print(f"[Legacy Gate] ERROR: {exc}")
        return 2

    if args.write_current is not None:
        output_path = (repo_root / args.write_current).resolve()
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(
            json.dumps(current_inventory, indent=2),
            encoding="utf-8",
        )
        print(f"[Legacy Gate] Current inventory written: {output_path}")

    print_summary(baseline_inventory, current_inventory)
    regressions = collect_regressions(baseline_inventory, current_inventory)
    if regressions:
        print("[Legacy Gate] FAILED: reintroduction or increase detected.")
        for item in regressions:
            print(f"[Legacy Gate] - {item}")
        return 1

    print("[Legacy Gate] PASS: no marker/classification regression detected.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
