#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Validate required worktree mapping prerequisites for build context"
    )
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="Repository root path",
    )
    parser.add_argument(
        "--required-dir",
        action="append",
        default=["third_party"],
        help="Required directory relative to repo root (repeatable)",
    )
    return parser.parse_args()


def check_required_dir(repo_root: Path, relative_dir: str) -> tuple[bool, str]:
    candidate = repo_root / relative_dir
    if not candidate.exists():
        return False, f"MISSING: {relative_dir} (not found at {candidate})"
    if not candidate.is_dir():
        return (
            False,
            f"INVALID: {relative_dir} exists but is not a directory ({candidate})",
        )
    if not any(candidate.iterdir()):
        return False, f"EMPTY: {relative_dir} exists but has no contents ({candidate})"
    return True, f"OK: {relative_dir} -> {candidate}"


def main() -> int:
    args = parse_args()
    repo_root = args.repo_root.resolve()

    if not repo_root.exists() or not repo_root.is_dir():
        print(f"[Mapping Check] ERROR: invalid repo root: {repo_root}")
        return 2

    print(f"[Mapping Check] Repo root: {repo_root}")
    has_failure = False
    required_dirs = list(dict.fromkeys(args.required_dir))
    for required_dir in required_dirs:
        ok, message = check_required_dir(repo_root, required_dir)
        print(f"[Mapping Check] {message}")
        if not ok:
            has_failure = True

    if has_failure:
        print("[Mapping Check] FAILED: one or more required mappings are unavailable.")
        return 1

    print("[Mapping Check] PASS: all required mappings are available.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
