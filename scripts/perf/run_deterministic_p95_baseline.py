#!/usr/bin/env python3
"""Run deterministic baseline protocol and compute median P95."""

from __future__ import annotations

import argparse
import datetime as dt
import json
import re
import statistics
import subprocess
import sys
from pathlib import Path

P95_PATTERN = re.compile(r"P95\s*[=:]\s*([0-9]+(?:\.[0-9]+)?)\s*ms", re.IGNORECASE)
P99_PATTERN = re.compile(r"P99=([0-9]+(?:\.[0-9]+)?)ms", re.IGNORECASE)


def parse_metric_ms(output: str) -> tuple[float, str]:
    match = P95_PATTERN.search(output)
    if match:
        return float(match.group(1)), "p95_ms"

    proxy = P99_PATTERN.search(output)
    if proxy:
        return float(proxy.group(1)), "p95_proxy_p99_ms"

    raise ValueError("No P95/P99 benchmark metric found in test output")


def run_once(args: list[str]) -> tuple[float, str, str, int]:
    completed = subprocess.run(args, check=False, capture_output=True, text=True)
    combined = f"{completed.stdout}\n{completed.stderr}".strip()
    if completed.returncode != 0:
        raise RuntimeError(
            "Benchmark command failed with non-zero exit code "
            f"{completed.returncode}.\n{combined}"
        )
    metric_ms, metric_kind = parse_metric_ms(combined)
    return metric_ms, metric_kind, combined, completed.returncode


def write_text(path: Path, value: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(value, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Run deterministic perf baseline protocol: 1 warmup + 5 measured runs"
        )
    )
    parser.add_argument(
        "--test-binary",
        default="./bin/NoMoreDayTests.exe",
        help="Path to benchmark test binary",
    )
    parser.add_argument(
        "--test-case",
        default="[Performance] MaterialVFX - MaterialSwap+Distortion Stress P95",
        help="Fixed benchmark scenario",
    )
    parser.add_argument(
        "--warmup-runs",
        type=int,
        default=1,
        help="Number of warmup runs",
    )
    parser.add_argument(
        "--measured-runs",
        type=int,
        default=5,
        help="Number of measured runs",
    )
    parser.add_argument(
        "--output-json",
        default="docs/reports/four-pillars/phase-0/P0-2/perf-baseline-run.json",
        help="JSON output path",
    )
    parser.add_argument(
        "--raw-output-dir",
        default="docs/reports/four-pillars/phase-0/P0-2/perf-run-logs",
        help="Directory for raw command outputs",
    )
    parsed = parser.parse_args()

    if parsed.warmup_runs != 1 or parsed.measured_runs != 5:
        raise ValueError("Protocol must use exactly 1 warmup run and 5 measured runs")

    command = [parsed.test_binary, f"--test-case={parsed.test_case}"]
    measured: list[float] = []
    metric_kind = ""
    raw_dir = Path(parsed.raw_output_dir)

    for run_idx in range(1, parsed.warmup_runs + parsed.measured_runs + 1):
        metric_ms, kind, output, _ = run_once(command)
        phase = "warmup" if run_idx <= parsed.warmup_runs else "measured"
        write_text(raw_dir / f"run-{run_idx:02d}-{phase}.log", output + "\n")
        if phase == "measured":
            measured.append(metric_ms)
            metric_kind = kind
        print(f"run {run_idx}: {phase} {kind}={metric_ms:.6f}ms")

    median_value = statistics.median(measured)
    payload = {
        "captured_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "protocol": {
            "warmup_runs": parsed.warmup_runs,
            "measured_runs": parsed.measured_runs,
            "fixed_command": command,
            "metric_kind": metric_kind,
        },
        "measured_values_ms": measured,
        "median_metric_ms": median_value,
    }

    output_path = Path(parsed.output_json)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    print(f"wrote {output_path} (median={median_value:.6f}ms)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
