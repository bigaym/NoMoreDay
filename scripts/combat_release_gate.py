#!/usr/bin/env python3
"""Combat release gate runner for CI/Nightly/Release modes."""

from __future__ import annotations

import argparse
import csv
import json
import re
import subprocess
import time
import uuid
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


def utc_now() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


@dataclass
class CheckResult:
    check_id: str
    title: str
    status: str
    critical: bool
    duration_seconds: float
    message: str
    command: str = ""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run combat release gate checks and export artifacts."
    )
    parser.add_argument(
        "--mode",
        type=str,
        choices=["ci", "nightly", "release"],
        default="release",
        help="Gate mode to execute.",
    )
    parser.add_argument(
        "--config-file",
        type=Path,
        default=Path("conductor/validation/combat_gate_config.json"),
        help="Combat gate configuration file.",
    )
    parser.add_argument(
        "--baseline-file",
        type=Path,
        default=Path("conductor/validation/combat_gate_baseline_m1.json"),
        help="Combat gate baseline file.",
    )
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=Path("build"),
        help="Build directory path.",
    )
    parser.add_argument(
        "--ctest-config",
        type=str,
        default="RelWithDebInfo",
        help="CTest config for ci/integration.",
    )
    parser.add_argument(
        "--performance-config",
        type=str,
        default="Release",
        help="CTest config for performance checks.",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("bin/combat_gate"),
        help="Output directory for gate reports.",
    )
    parser.add_argument(
        "--update-baseline",
        action="store_true",
        help="Update baseline file with current gate metrics.",
    )
    return parser.parse_args()


def read_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def load_baseline(path: Path) -> dict[str, Any]:
    if not path.exists():
        return {"version": "1.0.0", "metrics": {}}
    return read_json(path)


def substitute_tokens(tokens: list[str], replacements: dict[str, str]) -> list[str]:
    resolved: list[str] = []
    for token in tokens:
        value = token
        for key, replacement in replacements.items():
            value = value.replace(key, replacement)
        resolved.append(value)
    return resolved


def run_command(
    command: list[str], repo_root: Path, timeout_seconds: int
) -> tuple[int, str, float]:
    start = time.monotonic()
    try:
        completed = subprocess.run(
            command,
            cwd=repo_root,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=timeout_seconds,
            check=False,
        )
        output = (completed.stdout or "") + (
            "\n" + completed.stderr if completed.stderr else ""
        )
        return completed.returncode, output, time.monotonic() - start
    except subprocess.TimeoutExpired as exc:
        output = (exc.stdout or "") + ("\n" + exc.stderr if exc.stderr else "")
        return 124, output + "\ncommand_timeout", time.monotonic() - start


def clip_output(output: str, max_lines: int = 14) -> str:
    lines = [line.strip() for line in output.splitlines() if line.strip()]
    if len(lines) <= max_lines:
        return " | ".join(lines)
    return " | ".join(lines[-max_lines:])


def parse_metric(pattern: str, output: str) -> float | None:
    matched = re.search(pattern, output)
    if matched is None:
        return None
    return float(matched.group(1))


def evaluate_metric_threshold(
    metric_name: str,
    value: float,
    thresholds: dict[str, dict[str, float]],
) -> tuple[bool, str]:
    threshold = thresholds.get(metric_name, {})
    min_value = threshold.get("min")
    max_value = threshold.get("max")
    if min_value is not None and value < float(min_value):
        return False, f"{metric_name} below min: {value:.4f} < {float(min_value):.4f}"
    if max_value is not None and value > float(max_value):
        return False, f"{metric_name} above max: {value:.4f} > {float(max_value):.4f}"
    return True, "ok"


def evaluate_manual_checklist(path: Path) -> tuple[str, str]:
    if not path.exists():
        return "fail", f"manual_checklist_missing:{path.as_posix()}"

    text = path.read_text(encoding="utf-8", errors="replace")
    checked = len(re.findall(r"- \[x\]", text, flags=re.IGNORECASE))
    unchecked = len(re.findall(r"- \[ \]", text))
    if checked == 0:
        return "fail", "manual_checklist_empty"
    if unchecked > 0:
        return "fail", f"manual_checklist_unchecked:{unchecked}"
    return "pass", f"manual_checklist_complete:{checked}"


def evaluate_build_integration(build_bat: Path) -> tuple[str, str]:
    if not build_bat.exists():
        return "fail", "build_bat_missing"
    text = build_bat.read_text(encoding="utf-8", errors="replace")
    has_arg = 'if /i "%~1"=="combat-gate"' in text
    has_runner = "scripts\\combat_release_gate.py" in text
    if has_arg and has_runner:
        return "pass", "build_combat_gate_integrated"
    return "fail", "build_combat_gate_missing"


def parse_major_regression_counts(
    bug_registry_path: Path, scope_keywords: list[str]
) -> tuple[int, int]:
    if not bug_registry_path.exists():
        return 0, 0

    text = bug_registry_path.read_text(encoding="utf-8", errors="replace")
    scope_terms = [item.lower() for item in scope_keywords]
    pattern = re.compile(
        r"^\|\s*(BUG-\d{8}-\d+)\s*\|\s*([^|]*)\|\s*([^|]*)\|\s*(P[0-3])\s*\|\s*([^|]*)\|",
        flags=re.IGNORECASE,
    )

    historical = 0
    active = 0
    for line in text.splitlines():
        match = pattern.match(line)
        if match is None:
            continue

        stage = match.group(3).strip().lower()
        severity = match.group(4).strip().upper()
        status = match.group(5).strip().lower()
        normalized_line = line.lower()
        if severity not in {"P0", "P1"}:
            continue
        if "回归" not in stage and "regression" not in stage:
            continue
        if scope_terms and not any(term in normalized_line for term in scope_terms):
            continue

        historical += 1
        if status not in {"verified", "closed"}:
            active += 1

    return historical, active


def write_csv(path: Path, checks: list[CheckResult]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(
            ["id", "status", "critical", "durationSeconds", "message", "command"]
        )
        for check in checks:
            writer.writerow(
                [
                    check.check_id,
                    check.status,
                    "1" if check.critical else "0",
                    f"{check.duration_seconds:.3f}",
                    check.message,
                    check.command,
                ]
            )


def summarize(checks: list[CheckResult]) -> dict[str, int]:
    total = len(checks)
    passed = sum(1 for item in checks if item.status == "pass")
    failed = sum(1 for item in checks if item.status == "fail")
    warning = sum(1 for item in checks if item.status == "warning")
    return {"total": total, "passed": passed, "failed": failed, "warning": warning}


def write_report(
    path: Path,
    run_id: str,
    mode: str,
    checks: list[CheckResult],
    metrics: dict[str, float],
    baseline_metrics: dict[str, float],
) -> None:
    summary = summarize(checks)
    critical_fail = any(item.status == "fail" and item.critical for item in checks)
    payload = {
        "version": "1.0.0",
        "runId": run_id,
        "mode": mode,
        "createdAtUtc": utc_now(),
        "status": "fail" if critical_fail else "pass",
        "summary": summary,
        "metrics": metrics,
        "baselineMetrics": baseline_metrics,
        "checks": [
            {
                "id": item.check_id,
                "title": item.title,
                "status": item.status,
                "critical": item.critical,
                "durationSeconds": item.duration_seconds,
                "message": item.message,
                "command": item.command,
            }
            for item in checks
        ],
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2), encoding="utf-8")


def main() -> int:
    args = parse_args()
    repo_root = Path.cwd()

    config = read_json(args.config_file)
    baseline = load_baseline(args.baseline_file)
    baseline_metrics: dict[str, float] = baseline.get("metrics", {})
    thresholds: dict[str, dict[str, float]] = config.get("thresholds", {})
    mode_cfg = config.get("modes", {}).get(args.mode)
    if mode_cfg is None:
        raise ValueError(f"Unknown mode: {args.mode}")

    checks_cfg: list[dict[str, Any]] = list(mode_cfg.get("checks", []))
    check_results: list[CheckResult] = []
    check_status_map: dict[str, str] = {}
    metrics: dict[str, float] = {}
    deferred_checks: list[dict[str, Any]] = []

    replacements = {
        "{build_dir}": str(args.build_dir),
        "{ctest_config}": args.ctest_config,
        "{performance_config}": args.performance_config,
    }

    for check in checks_cfg:
        check_id = str(check["id"])
        title = str(check.get("title", check_id))
        check_type = str(check.get("type", "command"))
        critical = bool(check.get("critical", True))

        if check_type == "derived":
            deferred_checks.append(check)
            continue

        if check_type == "manual_checklist":
            start = time.monotonic()
            status, message = evaluate_manual_checklist(Path(str(check["path"])))
            result = CheckResult(
                check_id=check_id,
                title=title,
                status=status,
                critical=critical,
                duration_seconds=time.monotonic() - start,
                message=message,
            )
            check_results.append(result)
            check_status_map[check_id] = status
            continue

        if check_type == "build_integration":
            start = time.monotonic()
            status, message = evaluate_build_integration(Path("build.bat"))
            result = CheckResult(
                check_id=check_id,
                title=title,
                status=status,
                critical=critical,
                duration_seconds=time.monotonic() - start,
                message=message,
            )
            check_results.append(result)
            check_status_map[check_id] = status
            continue

        if check_type != "command":
            result = CheckResult(
                check_id=check_id,
                title=title,
                status="warning",
                critical=critical,
                duration_seconds=0.0,
                message=f"unknown_check_type:{check_type}",
            )
            check_results.append(result)
            check_status_map[check_id] = result.status
            continue

        command = substitute_tokens(list(check["command"]), replacements)
        timeout_seconds = int(check.get("timeoutSeconds", 1800))
        exit_code, output, duration = run_command(command, repo_root, timeout_seconds)
        status = "pass" if exit_code == 0 else "fail"
        messages: list[str] = [clip_output(output)]

        if status == "fail":
            allowed_patterns = [str(item) for item in check.get("allowFailurePatterns", [])]
            for pattern in allowed_patterns:
                if re.search(pattern, output):
                    status = "warning"
                    messages.append(f"allowed_failure_pattern:{pattern}")
                    break

        for metric_cfg in check.get("metrics", []):
            metric_name = str(metric_cfg["name"])
            if "pattern" in metric_cfg:
                parsed = parse_metric(str(metric_cfg["pattern"]), output)
                if parsed is None:
                    status = "fail"
                    messages.append(f"metric_parse_failed:{metric_name}")
                    continue
                metrics[metric_name] = parsed
            else:
                pass_value = float(metric_cfg.get("valueOnPass", 100.0))
                fail_value = float(metric_cfg.get("valueOnFail", 0.0))
                metrics[metric_name] = pass_value if exit_code == 0 else fail_value

            ok, metric_message = evaluate_metric_threshold(
                metric_name, metrics[metric_name], thresholds
            )
            if not ok:
                status = "fail"
                messages.append(metric_message)

        result = CheckResult(
            check_id=check_id,
            title=title,
            status=status,
            critical=critical,
            duration_seconds=duration,
            message=";".join(item for item in messages if item),
            command=" ".join(command),
        )
        check_results.append(result)
        check_status_map[check_id] = status

    for check in deferred_checks:
        check_id = str(check["id"])
        title = str(check.get("title", check_id))
        critical = bool(check.get("critical", True))
        start = time.monotonic()
        kind = str(check.get("kind", ""))
        status = "warning"
        message = "unknown_derived_check"

        if kind == "coverage":
            source_ids = [str(item) for item in check.get("sources", [])]
            metric_name = str(check["metric"])
            if not source_ids:
                status = "fail"
                message = "coverage_sources_missing"
            else:
                covered = sum(
                    1
                    for source_id in source_ids
                    if check_status_map.get(source_id) in {"pass", "warning"}
                )
                coverage = (covered / len(source_ids)) * 100.0
                metrics[metric_name] = coverage
                ok, metric_message = evaluate_metric_threshold(
                    metric_name, coverage, thresholds
                )
                status = "pass" if ok else "fail"
                message = (
                    f"coverage={coverage:.2f}% from {covered}/{len(source_ids)};"
                    f"{metric_message}"
                )

        elif kind == "major_regression_reduction":
            scope_keywords = [
                str(item)
                for item in config.get("regression", {}).get("scopeKeywords", [])
            ]
            bug_registry = Path(
                str(config.get("regression", {}).get("bugRegistry", "conductor/bug_registry.md"))
            )
            historical_count, active_count = parse_major_regression_counts(
                bug_registry, scope_keywords
            )
            baseline_count = float(
                baseline_metrics.get("major_regression_count", float(historical_count))
            )
            metrics["major_regression_count_current"] = float(active_count)
            if baseline_count <= 0.0:
                reduction = 100.0 if active_count <= 0 else 0.0
            else:
                reduction = ((baseline_count - float(active_count)) / baseline_count) * 100.0
            metrics["major_regression_reduction_pct"] = reduction
            ok, metric_message = evaluate_metric_threshold(
                "major_regression_reduction_pct", reduction, thresholds
            )
            status = "pass" if ok else "fail"
            message = (
                f"historical={historical_count},baseline={baseline_count:.0f},"
                f"active={active_count},reduction={reduction:.2f}%;{metric_message}"
            )

        result = CheckResult(
            check_id=check_id,
            title=title,
            status=status,
            critical=critical,
            duration_seconds=time.monotonic() - start,
            message=message,
        )
        check_results.append(result)
        check_status_map[check_id] = status

    run_id = f"combat-gate-{uuid.uuid4()}"
    args.output_dir.mkdir(parents=True, exist_ok=True)
    report_path = args.output_dir / f"combat_gate_report_{args.mode}.json"
    csv_path = args.output_dir / f"combat_gate_report_{args.mode}.csv"

    write_report(
        report_path,
        run_id,
        args.mode,
        check_results,
        metrics,
        {key: float(value) for key, value in baseline_metrics.items()},
    )
    write_csv(csv_path, check_results)

    if args.update_baseline:
        updated = {
            "version": "1.0.0",
            "updatedAtUtc": utc_now(),
            "mode": args.mode,
            "metrics": {
                "combat_frame_p95_ms": float(metrics.get("combat_frame_p95_ms", 0.0)),
                "combat_frame_p99_ms": float(metrics.get("combat_frame_p99_ms", 0.0)),
                "major_regression_count": float(
                    metrics.get(
                        "major_regression_count_current",
                        baseline_metrics.get("major_regression_count", 0.0),
                    )
                ),
            },
        }
        args.baseline_file.write_text(json.dumps(updated, indent=2), encoding="utf-8")

    summary = summarize(check_results)
    critical_fail = any(item.status == "fail" and item.critical for item in check_results)
    print(
        f"[combat-gate] mode={args.mode} checks={summary['total']} "
        f"pass={summary['passed']} warning={summary['warning']} fail={summary['failed']}"
    )
    print(f"[combat-gate] report={report_path.as_posix()}")
    return 1 if critical_fail else 0


if __name__ == "__main__":
    raise SystemExit(main())
