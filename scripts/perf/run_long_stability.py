#!/usr/bin/env python3
"""Run long-duration deterministic stability sampling and emit JSON."""

from __future__ import annotations

import argparse
import datetime as dt
import json
import re
import statistics
import subprocess
import sys
import time
from pathlib import Path

P95_PATTERN = re.compile(r"P95\s*[=:]\s*([0-9]+(?:\.[0-9]+)?)\s*ms", re.IGNORECASE)
P99_PATTERN = re.compile(r"P99=([0-9]+(?:\.[0-9]+)?)ms", re.IGNORECASE)

PROFILE_TEST_CASE = {
    "phase5": "[Performance] MaterialVFX - MaterialSwap+Distortion Stress P95",
}


def parse_metric_ms(output: str) -> tuple[float, str]:
    match = P95_PATTERN.search(output)
    if match:
        return float(match.group(1)), "p95_ms"

    proxy = P99_PATTERN.search(output)
    if proxy:
        return float(proxy.group(1)), "p95_proxy_p99_ms"

    raise ValueError("No P95/P99 benchmark metric found in test output")


def percentile(values: list[float], pct: float) -> float:
    if not values:
        raise ValueError("Cannot compute percentile for empty values")
    if len(values) == 1:
        return values[0]
    ordered = sorted(values)
    rank = (len(ordered) - 1) * pct
    low = int(rank)
    high = min(low + 1, len(ordered) - 1)
    weight = rank - low
    return ordered[low] * (1.0 - weight) + ordered[high] * weight


def write_text(path: Path, value: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(value, encoding="utf-8")


def run_once(command: list[str]) -> tuple[float, str, str, int, float]:
    started = time.monotonic()
    completed = subprocess.run(command, check=False, capture_output=True, text=True)
    elapsed_s = time.monotonic() - started
    output = f"{completed.stdout}\n{completed.stderr}".strip()
    if completed.returncode != 0:
        raise RuntimeError(
            "Benchmark command failed with non-zero exit code "
            f"{completed.returncode}.\n{output}"
        )
    metric_ms, metric_kind = parse_metric_ms(output)
    return metric_ms, metric_kind, output, completed.returncode, elapsed_s


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run long stability benchmark sampling for a fixed duration"
    )
    parser.add_argument("--profile", default="phase5", help="Stability profile name")
    parser.add_argument(
        "--minutes", type=float, default=60.0, help="Duration in minutes"
    )
    parser.add_argument(
        "--test-binary",
        default="./bin/NoMoreDayTests.exe",
        help="Path to benchmark test binary",
    )
    parser.add_argument(
        "--test-case",
        default="",
        help="Explicit benchmark test-case override",
    )
    parser.add_argument(
        "--output-json",
        default="docs/reports/four-pillars/phase-5/C5-2/stability-phase5.json",
        help="JSON output path",
    )
    parser.add_argument(
        "--raw-output-dir",
        default="docs/reports/four-pillars/phase-5/C5-2/stability-run-logs",
        help="Directory for per-sample raw outputs",
    )
    parsed = parser.parse_args()

    if parsed.minutes <= 0:
        raise ValueError("--minutes must be > 0")

    test_case = parsed.test_case or PROFILE_TEST_CASE.get(parsed.profile)
    if not test_case:
        raise ValueError(
            f"No test case configured for profile '{parsed.profile}'. "
            "Provide --test-case explicitly."
        )

    command = [parsed.test_binary, f"--test-case={test_case}"]
    started_utc = dt.datetime.now(dt.timezone.utc)
    deadline = time.monotonic() + parsed.minutes * 60.0

    samples: list[dict[str, object]] = []
    values: list[float] = []
    metric_kind = ""
    raw_dir = Path(parsed.raw_output_dir)
    iteration = 0

    while True:
        now = time.monotonic()
        if iteration > 0 and now >= deadline:
            break

        iteration += 1
        metric_ms, kind, output, return_code, elapsed_s = run_once(command)
        metric_kind = kind
        values.append(metric_ms)

        sample = {
            "iteration": iteration,
            "captured_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
            "metric_ms": metric_ms,
            "metric_kind": kind,
            "elapsed_seconds": elapsed_s,
            "return_code": return_code,
        }
        samples.append(sample)
        write_text(raw_dir / f"sample-{iteration:04d}.log", output + "\n")
        print(f"sample {iteration}: {kind}={metric_ms:.6f}ms elapsed={elapsed_s:.3f}s")

    finished_utc = dt.datetime.now(dt.timezone.utc)
    actual_duration_s = (finished_utc - started_utc).total_seconds()

    summary = {
        "sample_count": len(values),
        "min_metric_ms": min(values),
        "max_metric_ms": max(values),
        "median_metric_ms": statistics.median(values),
        "p95_metric_ms": percentile(values, 0.95),
    }

    payload = {
        "captured_utc": finished_utc.isoformat(),
        "protocol": {
            "profile": parsed.profile,
            "target_minutes": parsed.minutes,
            "actual_duration_seconds": actual_duration_s,
            "fixed_command": command,
            "metric_kind": metric_kind,
        },
        "summary": summary,
        "samples": samples,
    }

    output_path = Path(parsed.output_json)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    print(
        f"wrote {output_path} "
        f"(samples={summary['sample_count']}, median={summary['median_metric_ms']:.6f}ms)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
