#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import re
from collections import Counter
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path


INCLUDE_PATTERN = re.compile(
    r'^\s*#\s*include\s+"game/systems/([^"\\/]+)(?:[\\/][^"]*)?"'
)


@dataclass(frozen=True)
class CppFileStat:
    relative_path: str
    subsystem: str
    loc: int


def parse_args() -> argparse.Namespace:
    repo_root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(
        description="Report baseline architecture risk hotspots for src/game/systems."
    )
    parser.add_argument(
        "--systems-root",
        type=Path,
        default=repo_root / "src/game/systems",
        help="Path to src/game/systems directory.",
    )
    parser.add_argument(
        "--top",
        type=int,
        default=10,
        help="Number of largest .cpp files to include.",
    )
    parser.add_argument(
        "--json-out",
        type=Path,
        default=None,
        help="Optional path to write JSON output.",
    )
    return parser.parse_args()


def detect_subsystem(relative_cpp_path: Path) -> str:
    if not relative_cpp_path.parts:
        return "(root)"
    return relative_cpp_path.parts[0]


def count_loc(text: str) -> int:
    return sum(1 for line in text.splitlines() if line.strip())


def collect_stats(
    systems_root: Path,
) -> tuple[list[CppFileStat], dict[tuple[str, str], int]]:
    cpp_stats: list[CppFileStat] = []
    include_edges: Counter[tuple[str, str]] = Counter()

    cpp_files = sorted(systems_root.rglob("*.cpp"))
    for cpp_path in cpp_files:
        relative_cpp_path = cpp_path.relative_to(systems_root)
        subsystem = detect_subsystem(relative_cpp_path)
        text = cpp_path.read_text(encoding="utf-8")
        cpp_stats.append(
            CppFileStat(
                relative_path=relative_cpp_path.as_posix(),
                subsystem=subsystem,
                loc=count_loc(text),
            )
        )

        for line in text.splitlines():
            include_match = INCLUDE_PATTERN.match(line)
            if include_match is None:
                continue
            target_subsystem = include_match.group(1)
            if target_subsystem == subsystem:
                continue
            include_edges[(subsystem, target_subsystem)] += 1

    return cpp_stats, dict(include_edges)


def build_report(
    systems_root: Path,
    top_n: int,
    cpp_stats: list[CppFileStat],
    include_edges: dict[tuple[str, str], int],
) -> dict:
    subsystem_counts = Counter(stat.subsystem for stat in cpp_stats)
    top_largest = sorted(cpp_stats, key=lambda stat: (-stat.loc, stat.relative_path))[
        :top_n
    ]
    edge_rows = sorted(
        (
            {"source": source, "target": target, "count": count}
            for (source, target), count in include_edges.items()
        ),
        key=lambda row: (-row["count"], row["source"], row["target"]),
    )

    return {
        "generated_at_utc": datetime.now(timezone.utc)
        .replace(microsecond=0)
        .isoformat(),
        "systems_root": str(systems_root),
        "total_cpp_files": len(cpp_stats),
        "subsystem_cpp_counts": dict(sorted(subsystem_counts.items())),
        "top_largest_cpp": [
            {
                "path": stat.relative_path,
                "subsystem": stat.subsystem,
                "loc": stat.loc,
            }
            for stat in top_largest
        ],
        "cross_subsystem_include_edges": edge_rows,
    }


def print_report(report: dict) -> None:
    print("[quality] Systems baseline report")
    print(f"- systems_root: {report['systems_root']}")
    print(f"- total_cpp_files: {report['total_cpp_files']}")

    print("\nSubsystem .cpp counts:")
    for subsystem, count in report["subsystem_cpp_counts"].items():
        print(f"- {subsystem}: {count}")

    print("\nTop largest .cpp files by LOC:")
    for index, item in enumerate(report["top_largest_cpp"], start=1):
        print(f"{index:2d}. {item['path']} ({item['subsystem']}) - {item['loc']} LOC")

    print("\nCross-subsystem include edges:")
    edges = report["cross_subsystem_include_edges"]
    if not edges:
        print("- (none)")
        return
    for edge in edges:
        print(f"- {edge['source']} -> {edge['target']}: {edge['count']}")


def main() -> int:
    args = parse_args()
    systems_root = args.systems_root.resolve()

    if args.top <= 0:
        raise ValueError("--top must be greater than zero")
    if not systems_root.is_dir():
        raise FileNotFoundError(f"systems root not found: {systems_root}")

    cpp_stats, include_edges = collect_stats(systems_root)
    report = build_report(systems_root, args.top, cpp_stats, include_edges)
    print_report(report)

    if args.json_out is not None:
        json_path = args.json_out.resolve()
        json_path.parent.mkdir(parents=True, exist_ok=True)
        json_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
        print(f"\n[quality] wrote JSON report: {json_path}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
