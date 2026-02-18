#!/usr/bin/env python3
"""Run long-duration stability stress checks and track VRAM proxy deltas."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import time
from pathlib import Path


VRAM_DELTA_PATTERN = re.compile(
    r"RELEASE_GATE_METRIC\s+vram_proxy_delta_bytes=([-0-9.]+)"
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="V3 stability stress runner (duration + VRAM proxy sampling)."
    )
    parser.add_argument(
        "--duration-minutes",
        type=float,
        default=30.0,
        help="Stress duration in minutes.",
    )
    parser.add_argument(
        "--threshold-bytes",
        type=float,
        default=0.0,
        help="Allowed max VRAM proxy growth in bytes.",
    )
    parser.add_argument(
        "--test-exe",
        type=Path,
        default=Path("bin/NoMoreDayTests.exe"),
        help="Path to NoMoreDayTests executable.",
    )
    parser.add_argument(
        "--test-case",
        type=str,
        default="[Integration] ReleaseGate - Framebuffer tracked bytes stable under resize stress",
        help="Test case filter for stress iteration.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("bin/release_gate/stability_stress_report.json"),
        help="Output report path.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Only validate configuration without executing tests.",
    )
    return parser.parse_args()


def run_once(test_exe: Path, test_case: str) -> tuple[int, str]:
    command = [str(test_exe), f"--test-case={test_case}"]
    process = subprocess.run(
        command,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    text = process.stdout + "\n" + process.stderr
    return process.returncode, text


def extract_delta_bytes(output: str) -> float | None:
    match = VRAM_DELTA_PATTERN.search(output)
    if match is None:
        return None
    return float(match.group(1))


def main() -> int:
    args = parse_args()
    report = {
        "version": "1.0.0",
        "durationMinutes": args.duration_minutes,
        "thresholdBytes": args.threshold_bytes,
        "status": "pass",
        "iterations": 0,
        "maxDeltaBytes": 0.0,
        "samples": [],
        "notes": [],
    }

    if args.dry_run:
        report["notes"].append("dry_run")
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2), encoding="utf-8")
        print("[stability] dry-run complete")
        return 0

    if not args.test_exe.exists():
        report["status"] = "fail"
        report["notes"].append(f"test_exe_missing:{args.test_exe.as_posix()}")
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2), encoding="utf-8")
        print(f"[stability] missing test exe: {args.test_exe}")
        return 1

    end_time = time.monotonic() + max(args.duration_minutes, 0.0) * 60.0
    max_delta = 0.0
    iteration = 0
    while time.monotonic() < end_time:
        iteration += 1
        code, output = run_once(args.test_exe, args.test_case)
        delta = extract_delta_bytes(output)
        if delta is None:
            report["status"] = "fail"
            report["notes"].append("missing_vram_metric")
            report["iterations"] = iteration
            break
        max_delta = max(max_delta, delta)
        report["samples"].append(delta)
        report["iterations"] = iteration
        if code != 0:
            report["status"] = "fail"
            report["notes"].append(f"test_failed_iter_{iteration}")
            break
        if delta > args.threshold_bytes:
            report["status"] = "fail"
            report["notes"].append(
                f"threshold_exceeded_iter_{iteration}:{delta:.3f}"
            )
            break

    report["maxDeltaBytes"] = max_delta
    if report["status"] == "pass" and max_delta > args.threshold_bytes:
        report["status"] = "fail"
        report["notes"].append("threshold_exceeded")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(
        f"[stability] iterations={report['iterations']} "
        f"maxDeltaBytes={report['maxDeltaBytes']} status={report['status']}"
    )
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
