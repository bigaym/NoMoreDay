#!/usr/bin/env python3
"""V3 validation and release gate runner."""

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
    task_id: str
    layer: str
    title: str
    status: str
    critical: bool
    duration_seconds: float
    message: str
    command: str = ""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run NoMoreDay V3 release gate checks and export artifacts."
    )
    parser.add_argument(
        "--matrix",
        type=Path,
        default=Path("conductor/validation/v3_gate_matrix.json"),
        help="Gate matrix json path.",
    )
    parser.add_argument(
        "--profiles",
        type=Path,
        default=Path("conductor/validation/v3_perf_profiles.json"),
        help="Performance profile config path.",
    )
    parser.add_argument(
        "--baseline",
        type=Path,
        default=Path("conductor/validation/v3_perf_baseline.json"),
        help="Stored baseline metrics path.",
    )
    parser.add_argument(
        "--waivers",
        type=Path,
        default=Path("conductor/validation/v3_gate_waivers.json"),
        help="Temporary waiver config path.",
    )
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=Path("build"),
        help="Build directory path placeholder.",
    )
    parser.add_argument(
        "--config",
        type=str,
        default="RelWithDebInfo",
        help="Build config placeholder.",
    )
    parser.add_argument(
        "--test-exe",
        type=Path,
        default=None,
        help="Optional explicit NoMoreDayTests executable path.",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("bin/release_gate"),
        help="Output directory for reports.",
    )
    parser.add_argument(
        "--update-baseline",
        action="store_true",
        help="Update baseline json with current profile metrics.",
    )
    parser.add_argument(
        "--allow-missing-screenshots",
        action="store_true",
        help="Do not fail gate when screenshot files are missing.",
    )
    parser.add_argument(
        "--inject-perf-regression",
        action="store_true",
        help="Inject synthetic >10% regression for regression-path verification.",
    )
    parser.add_argument(
        "--final-verification",
        action="store_true",
        help="Mark this run as final build/analyze/perf verification evidence.",
    )
    return parser.parse_args()


def read_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def load_waivers(path: Path) -> dict[str, dict[str, dict[str, Any]]]:
    by_check: dict[str, dict[str, Any]] = {}
    by_task: dict[str, dict[str, Any]] = {}
    if not path.exists():
        return {"by_check": by_check, "by_task": by_task}

    data = read_json(path)
    items = data.get("waivers", [])
    for item in items:
        status = str(item.get("status", "active")).lower()
        if status != "active":
            continue
        check_id = str(item.get("checkId", "")).strip()
        task_id = str(item.get("taskId", "")).strip()
        if check_id:
            by_check[check_id] = item
        if task_id:
            by_task[task_id] = item
    return {"by_check": by_check, "by_task": by_task}


def find_waiver(
    waivers: dict[str, dict[str, dict[str, Any]]],
    check_id: str,
    task_id: str,
) -> dict[str, Any] | None:
    by_check = waivers.get("by_check", {})
    by_task = waivers.get("by_task", {})
    if check_id in by_check:
        return by_check[check_id]
    if task_id in by_task:
        return by_task[task_id]
    return None


def find_test_exe(repo_root: Path, args: argparse.Namespace) -> Path | None:
    candidates: list[Path] = []
    if args.test_exe is not None:
        candidates.append(args.test_exe)
    candidates.append(repo_root / "bin" / "NoMoreDayTests.exe")
    candidates.append(repo_root / "bin" / args.config / "NoMoreDayTests.exe")
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return None


def substitute_tokens(
    tokens: list[str],
    repo_root: Path,
    build_dir: Path,
    config: str,
    test_exe: Path | None,
) -> list[str]:
    resolved: list[str] = []
    replacements = {
        "{repo_root}": str(repo_root),
        "{build_dir}": str(build_dir),
        "{config}": config,
        "{test_exe}": str(test_exe) if test_exe is not None else "",
    }
    for token in tokens:
        value = token
        for key, replacement in replacements.items():
            value = value.replace(key, replacement)
        resolved.append(value)
    return resolved


def run_command(
    command: list[str],
    repo_root: Path,
    timeout_seconds: int,
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
        output = (completed.stdout or "") + ("\n" + completed.stderr if completed.stderr else "")
        return completed.returncode, output, time.monotonic() - start
    except subprocess.TimeoutExpired as exc:
        output = (exc.stdout or "") + ("\n" + exc.stderr if exc.stderr else "")
        return 124, output + "\ncommand_timeout", time.monotonic() - start


def parse_metric(metric: dict[str, Any], output: str) -> float | None:
    pattern = metric.get("pattern")
    if not pattern:
        return None
    match = re.search(pattern, output)
    if match is None:
        return None
    return float(match.group(1))


def clip_output(output: str, max_lines: int = 12) -> str:
    lines = [line.strip() for line in output.splitlines() if line.strip()]
    if len(lines) <= max_lines:
        return " | ".join(lines)
    tail = lines[-max_lines:]
    return " | ".join(tail)


def evaluate_file_exists(path: Path) -> tuple[str, str]:
    if path.exists():
        return "pass", "file_exists"
    return "fail", f"file_missing:{path.as_posix()}"


def evaluate_build_integration(build_bat: Path) -> tuple[str, str]:
    if not build_bat.exists():
        return "fail", "build_bat_missing"
    text = build_bat.read_text(encoding="utf-8", errors="replace")
    has_arg = 'if /i "%~1"=="gate"' in text
    has_runner = "scripts\\v3_release_gate.py" in text
    if has_arg and has_runner:
        return "pass", "build_gate_arg_integrated"
    return "fail", "build_gate_arg_or_runner_missing"


def run_screenshot_check(
    repo_root: Path,
    manifest_path: Path,
    output_path: Path,
    allow_missing: bool,
) -> tuple[str, str, float]:
    command = [
        "python",
        "scripts/v3_screenshot_diff.py",
        "--manifest",
        str(manifest_path),
        "--output",
        str(output_path),
    ]
    if allow_missing:
        command.append("--allow-missing")
    code, output, duration = run_command(command, repo_root, timeout_seconds=180)
    status = "pass"
    message = clip_output(output)
    if not output_path.exists():
        return "fail", "screenshot_report_missing", duration
    report = read_json(output_path)
    summary = report.get("summary", {})
    fail_count = int(summary.get("fail", 0))
    warning_count = int(summary.get("warning", 0))
    if fail_count > 0 or code != 0:
        status = "fail"
    elif warning_count > 0:
        status = "warning"
    return status, message, duration


def compare_with_baseline(
    metrics: dict[str, float],
    baseline: dict[str, Any],
    profiles: dict[str, Any],
    inject_regression: bool,
) -> tuple[str, str, list[str]]:
    limit_pct = float(profiles.get("regressionLimitPercent", 10.0))
    baseline_metrics = baseline.get("metrics", {})
    profile_items = profiles.get("profiles", [])

    warnings: list[str] = []
    failures: list[str] = []
    for profile in profile_items:
        metric_name = str(profile["metric"])
        if metric_name not in metrics:
            failures.append(f"missing_metric:{metric_name}")
            continue
        current = float(metrics[metric_name])
        if inject_regression:
            current *= 0.8
        baseline_value = baseline_metrics.get(metric_name)
        if baseline_value is None:
            warnings.append(f"missing_baseline:{metric_name}")
            continue
        floor = float(baseline_value) * (1.0 - limit_pct / 100.0)
        if current < floor:
            failures.append(
                f"regression:{metric_name}:current={current:.4f}:floor={floor:.4f}"
            )

    if failures:
        return "fail", ";".join(failures), warnings
    if warnings:
        return "warning", ";".join(warnings), warnings
    return "pass", "baseline_regression_check_ok", warnings


def evaluate_perf_injection_self_test() -> tuple[str, str]:
    baseline = {"metrics": {"m": 200.0}}
    profiles = {"profiles": [{"id": "p", "metric": "m"}], "regressionLimitPercent": 10.0}
    current = {"m": 180.0}
    status, _, _ = compare_with_baseline(
        metrics=current,
        baseline=baseline,
        profiles=profiles,
        inject_regression=True,
    )
    if status == "fail":
        return "pass", "perf_injection_regression_path_verified"
    return "fail", "perf_injection_regression_path_not_triggered"


def write_csv(path: Path, checks: list[CheckResult]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as out:
        writer = csv.writer(out)
        writer.writerow(
            [
                "id",
                "taskId",
                "layer",
                "status",
                "critical",
                "durationSeconds",
                "message",
                "command",
            ]
        )
        for check in checks:
            writer.writerow(
                [
                    check.check_id,
                    check.task_id,
                    check.layer,
                    check.status,
                    "1" if check.critical else "0",
                    f"{check.duration_seconds:.3f}",
                    check.message,
                    check.command,
                ]
            )


def summarize(checks: list[CheckResult]) -> dict[str, int]:
    total = len(checks)
    passed = sum(1 for check in checks if check.status == "pass")
    failed = sum(1 for check in checks if check.status == "fail")
    warning = sum(1 for check in checks if check.status == "warning")
    return {"total": total, "passed": passed, "failed": failed, "warning": warning}


def should_trigger_fallback(check: CheckResult) -> bool:
    if check.status != "fail" or not check.critical:
        return False
    if check.task_id in {"F2.1", "F2.2", "F2.6", "F4.5", "F7.1"}:
        return True
    return check.task_id.startswith("F4.")


def write_report(
    path: Path,
    run_id: str,
    checks: list[CheckResult],
    metrics: dict[str, float],
    fallback_reasons: list[str],
    waiver_hits: list[dict[str, Any]],
) -> None:
    summary = summarize(checks)
    has_critical_failure = any(
        check.status == "fail" and check.critical for check in checks
    )
    report = {
        "version": "1.0.0",
        "runId": run_id,
        "createdAtUtc": utc_now(),
        "status": "fail" if has_critical_failure else "pass",
        "fallbackTriggered": bool(fallback_reasons),
        "fallbackReasons": fallback_reasons,
        "waiverHits": waiver_hits,
        "summary": summary,
        "checks": [
            {
                "id": check.check_id,
                "taskId": check.task_id,
                "layer": check.layer,
                "title": check.title,
                "status": check.status,
                "critical": check.critical,
                "durationSeconds": check.duration_seconds,
                "message": check.message,
                "command": check.command,
            }
            for check in checks
        ],
        "metrics": metrics,
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(report, indent=2), encoding="utf-8")


def main() -> int:
    args = parse_args()
    repo_root = Path.cwd()

    matrix = read_json(args.matrix)
    profiles = read_json(args.profiles)
    baseline = read_json(args.baseline)
    waivers = load_waivers(args.waivers)
    screenshot_manifest_path = Path(matrix["screenshotManifest"])

    test_exe = find_test_exe(repo_root, args)
    metrics: dict[str, float] = {}
    check_results: list[CheckResult] = []
    deferred: list[tuple[str, str, dict[str, Any], dict[str, Any], str, bool]] = []

    screenshot_report_path = args.output_dir / "screenshots" / "screenshot_report.json"

    profile_min_fps: dict[str, float] = {
        str(item["metric"]): float(item["minFps"])
        for item in profiles.get("profiles", [])
    }
    cluster_metric_name = str(
        profiles.get("clusteredUpliftGate", {}).get("metric", "clustered_128_improvement_pct")
    )
    cluster_min_improvement = float(
        profiles.get("clusteredUpliftGate", {}).get("minImprovementPercent", 5.0)
    )

    for layer in matrix.get("layers", []):
        layer_id = str(layer["id"])
        for check in layer.get("checks", []):
            check_id = str(check["id"])
            task_id = str(check["taskId"])
            title = str(check["title"])
            check_type = str(check["type"])
            critical = bool(check.get("critical", False))

            if check_type == "derived" and str(check.get("derived", {}).get("kind")) in {
                "report_export",
                "final_verification",
            }:
                deferred.append((check_id, task_id, check, layer, title, critical))
                continue

            if check_type == "command":
                command_tokens = list(check["command"])
                if "{test_exe}" in " ".join(command_tokens) and test_exe is None:
                    check_results.append(
                        CheckResult(
                            check_id=check_id,
                            task_id=task_id,
                            layer=layer_id,
                            title=title,
                            status="fail",
                            critical=critical,
                            duration_seconds=0.0,
                            message="test_exe_not_found",
                            command=" ".join(command_tokens),
                        )
                    )
                    continue

                resolved = substitute_tokens(
                    tokens=command_tokens,
                    repo_root=repo_root,
                    build_dir=args.build_dir,
                    config=args.config,
                    test_exe=test_exe,
                )
                timeout_seconds = int(check.get("timeoutSeconds", 300))
                code, output, duration = run_command(resolved, repo_root, timeout_seconds)
                status = "pass" if code == 0 else "fail"
                message = clip_output(output)
                metric_cfg = check.get("metric")
                if metric_cfg:
                    value = parse_metric(metric_cfg, output)
                    if value is None:
                        status = "fail"
                        message = f"metric_parse_failed:{metric_cfg.get('name')}"
                    else:
                        metric_name = str(metric_cfg["name"])
                        metrics[metric_name] = value
                        if metric_name in profile_min_fps:
                            if value < profile_min_fps[metric_name]:
                                status = "fail"
                                message = (
                                    f"fps_below_threshold:{metric_name}:{value:.3f}"
                                )
                        if metric_name == cluster_metric_name:
                            if value < cluster_min_improvement:
                                status = "fail"
                                message = (
                                    f"cluster_uplift_below_threshold:"
                                    f"{value:.3f}<{cluster_min_improvement:.3f}"
                                )

                check_results.append(
                    CheckResult(
                        check_id=check_id,
                        task_id=task_id,
                        layer=layer_id,
                        title=title,
                        status=status,
                        critical=critical,
                        duration_seconds=duration,
                        message=message,
                        command=" ".join(resolved),
                    )
                )
                continue

            if check_type == "screenshot":
                start = time.monotonic()
                status, message, _ = run_screenshot_check(
                    repo_root=repo_root,
                    manifest_path=screenshot_manifest_path,
                    output_path=screenshot_report_path,
                    allow_missing=args.allow_missing_screenshots,
                )
                check_results.append(
                    CheckResult(
                        check_id=check_id,
                        task_id=task_id,
                        layer=layer_id,
                        title=title,
                        status=status,
                        critical=critical,
                        duration_seconds=time.monotonic() - start,
                        message=message,
                        command="python scripts/v3_screenshot_diff.py",
                    )
                )
                continue

            if check_type == "derived":
                start = time.monotonic()
                derived = check.get("derived", {})
                kind = str(derived.get("kind", ""))
                status = "pass"
                message = "derived_ok"
                if kind == "baseline_regression":
                    status, message, _ = compare_with_baseline(
                        metrics=metrics,
                        baseline=baseline,
                        profiles=profiles,
                        inject_regression=args.inject_perf_regression,
                    )
                elif kind == "perf_injection":
                    status, message = evaluate_perf_injection_self_test()
                elif kind == "screenshot_review":
                    if screenshot_report_path.exists():
                        report = read_json(screenshot_report_path)
                        summary = report.get("summary", {})
                        fail_count = int(summary.get("fail", 0))
                        if fail_count > 0:
                            status = "pass"
                            message = "review_required_and_diff_generated"
                        else:
                            status = "pass"
                            message = "no_review_required"
                    else:
                        status = "warning"
                        message = "screenshot_report_not_generated"
                elif kind == "build_integration":
                    status, message = evaluate_build_integration(Path("build.bat"))
                elif kind == "file_exists":
                    status, message = evaluate_file_exists(Path(str(derived["path"])))
                elif kind == "final_verification":
                    if args.final_verification:
                        status = "pass"
                        message = "final_verification_flag_present"
                    else:
                        status = "warning"
                        message = "final_verification_flag_missing"
                else:
                    status = "warning"
                    message = f"unknown_derived_kind:{kind}"

                check_results.append(
                    CheckResult(
                        check_id=check_id,
                        task_id=task_id,
                        layer=layer_id,
                        title=title,
                        status=status,
                        critical=critical,
                        duration_seconds=time.monotonic() - start,
                        message=message,
                    )
                )
                continue

            check_results.append(
                CheckResult(
                    check_id=check_id,
                    task_id=task_id,
                    layer=layer_id,
                    title=title,
                    status="warning",
                    critical=critical,
                    duration_seconds=0.0,
                    message=f"unknown_check_type:{check_type}",
                )
            )

    args.output_dir.mkdir(parents=True, exist_ok=True)
    report_path = args.output_dir / "v3_gate_report.json"
    csv_path = args.output_dir / "v3_gate_report.csv"
    run_id = f"v3-gate-{uuid.uuid4()}"

    # Defer report-export/final-verification checks until report files exist.
    write_report(
        path=report_path,
        run_id=run_id,
        checks=check_results,
        metrics=metrics,
        fallback_reasons=[],
        waiver_hits=[],
    )
    write_csv(csv_path, check_results)

    for check_id, task_id, check, layer, title, critical in deferred:
        start = time.monotonic()
        kind = str(check.get("derived", {}).get("kind", ""))
        status = "pass"
        message = "deferred_ok"
        if kind == "report_export":
            if report_path.exists() and csv_path.exists():
                status = "pass"
                message = "report_json_csv_exists"
            else:
                status = "fail"
                message = "report_json_csv_missing"
        elif kind == "final_verification":
            if args.final_verification:
                status = "pass"
                message = "final_verification_flag_present"
            else:
                status = "warning"
                message = "final_verification_flag_missing"
        else:
            status = "warning"
            message = f"unknown_deferred_kind:{kind}"

        check_results.append(
            CheckResult(
                check_id=check_id,
                task_id=task_id,
                layer=str(layer["id"]),
                title=title,
                status=status,
                critical=critical,
                duration_seconds=time.monotonic() - start,
                message=message,
            )
        )

    if args.update_baseline:
        baseline.setdefault("metrics", {})
        baseline["version"] = str(baseline.get("version", "1.0.0"))
        baseline["updatedAtUtc"] = utc_now()
        for item in profiles.get("profiles", []):
            metric_name = str(item["metric"])
            if metric_name in metrics:
                baseline["metrics"][metric_name] = metrics[metric_name]
        if cluster_metric_name in metrics:
            baseline["metrics"][cluster_metric_name] = metrics[cluster_metric_name]
        args.baseline.write_text(json.dumps(baseline, indent=2), encoding="utf-8")

    waiver_hits: list[dict[str, Any]] = []
    for check in check_results:
        waiver = find_waiver(waivers, check.check_id, check.task_id)
        if waiver is None or check.status != "fail":
            continue
        level = str(waiver.get("level", "warning")).lower()
        if level == "pass":
            check.status = "pass"
        else:
            check.status = "warning"
        waiver_id = str(waiver.get("id", "unknown-waiver"))
        check.message = f"waived:{waiver_id}:{check.message}"
        waiver_hits.append(
            {
                "waiverId": waiver_id,
                "checkId": check.check_id,
                "taskId": check.task_id,
                "expiresOn": str(waiver.get("expiresOn", "")),
                "reason": str(waiver.get("reason", "")),
            }
        )

    fallback_reasons = [
        f"{check.task_id}:{check.check_id}"
        for check in check_results
        if should_trigger_fallback(check)
    ]

    write_report(
        path=report_path,
        run_id=run_id,
        checks=check_results,
        metrics=metrics,
        fallback_reasons=fallback_reasons,
        waiver_hits=waiver_hits,
    )
    write_csv(csv_path, check_results)

    baseline_snapshot_path = args.output_dir / "v3_gate_baseline_snapshot.json"
    baseline_snapshot = {
        "createdAtUtc": utc_now(),
        "metrics": {
            key: value
            for key, value in metrics.items()
            if key in {str(p["metric"]) for p in profiles.get("profiles", [])}
        },
    }
    baseline_snapshot_path.write_text(
        json.dumps(baseline_snapshot, indent=2), encoding="utf-8"
    )

    summary = summarize(check_results)
    critical_fail = any(
        check.status == "fail" and check.critical for check in check_results
    )
    print(
        f"[release-gate] checks={summary['total']} pass={summary['passed']} "
        f"warning={summary['warning']} fail={summary['failed']}"
    )
    print(f"[release-gate] report={report_path.as_posix()}")
    return 1 if critical_fail else 0


if __name__ == "__main__":
    raise SystemExit(main())
