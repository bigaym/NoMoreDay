#!/usr/bin/env python3
"""GPU Hardware Validation Gate runner and artifact exporter.

W6 (M0-C): executes the production game-binary gate ``NoMoreDay.exe --gpu-gate``
(normal Game/App initialization, real GL context/registry/hooks/render path),
verifies the emitted versioned JSON report against the fail-closed schema, and
outputs a structured artifact. Pass only when the process returns 0, the report
is schema-valid and the status is exactly ``GO``; ``NO_GO``/``NOT_RUN`` are
failures. The standalone doctest binary is contract/diagnostic only and is not
invoked here.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


GATE_REPORT_BEGIN_MARKER = "GPU_HARDWARE_GATE_REPORT_BEGIN"
GATE_REPORT_END_MARKER = "GPU_HARDWARE_GATE_REPORT_END"
GATE_STATUS_VALUES = frozenset({"GO", "NO_GO", "NOT_RUN"})

# S8 (M0-C R6): the runner forwards its CLI knobs to the C++ gate through these
# environment variables. `tests/integration/GPUHardwareValidationGateTest.cpp`
# reads them (with the same defaults) and passes them to `RunGate`.
GATE_ENV_SAMPLES = "NMD_GATE_SAMPLES"
GATE_ENV_TOGGLE_LOOPS = "NMD_GATE_TOGGLE_LOOPS"
GATE_ENV_STRESS = "NMD_GATE_STRESS"

# S8: timeout budget is linked to the stress duration. A 60s stress loop adds
# its full duration to the base budget so the subprocess never times out just
# because stress is enabled.
GATE_BASE_TIMEOUT_SECONDS = 120
GATE_STRESS_ADDED_SECONDS = 60

# S8: a waiver records who accepted a deviation, for what scope, and until when.
# It is archival metadata only and can never turn NOT_RUN/NO_GO into GO.
WAIVER_FIELD_NAMES = ("authorizer", "reason", "scope", "expiry")

REQUIRED_TOP_LEVEL_KEYS = frozenset({
    "revision",
    "timestamp",
    "gate_status",
    "capabilities",
    "resources",
    "stress_test",
    "gl_diagnostics",
    "matrix_results",
    "global_failures",
    # W6 (M0-C): requested/actual sampling budget + non-exhaustive flag, and
    # occupancy/disocclusion evidence (fail-closed when missing).
    "run_config",
    "occupancy",
})

REQUIRED_RUN_CONFIG_KEYS = frozenset({
    "requested_sample_frames",
    "actual_sample_frames",
    "requested_toggle_loops",
    "actual_toggle_loops",
    "non_exhaustive",
})

REQUIRED_OCCUPANCY_KEYS = frozenset({"status", "reason", "blocks_go"})

SDF_READBACK_STATUSES = frozenset({"passed", "not_applicable", "missing", "failed"})

REQUIRED_CAPABILITY_KEYS = frozenset({
    "vendor",
    "renderer",
    "driver_version",
    "gl_version",
    "compute_shader",
    "ssbo",
    "persistent_mapping",
    "indirect_draw",
    "timer_query",
    "texture_array",
    "rgba16f",
    "debug_callback",
    "debug_output_installed",
    "debug_output_enabled",
    "meets_preflight",
    "preflight_reason",
    # W6 (M0-C): whether the driver supplied real gameplay render hooks.
    "render_hooks_supplied",
})

REQUIRED_GL_DIAGNOSTIC_KEYS = frozenset({
    "debug_message_count",
    "severe_error_count",
    "dropped_count",
    "callback_installed",
    "callback_enabled",
    "messages",
})

REQUIRED_DIAGNOSTIC_MESSAGE_KEYS = frozenset({
    "severity",
    "type",
    "source",
    "message",
    "id",
    "time",
})

# W6 (M0-C): each matrix cell must pin the reproducible input (fixture, tier,
# GI mode, extent, camera and ROI) plus the pass/readback verdicts and timings;
# a cell missing any of these is a schema violation (fail-closed, never
# default-filled).
REQUIRED_MATRIX_CELL_KEYS = frozenset({
    "fixture",
    "tier",
    "gi_enabled",
    "resolution",
    "camera",
    "roi",
    "pass_trace_valid",
    # W6 (M0-C) Blocker-1: real executed pass order + provenance, per-cell
    # paired GI delta, and the real SDF evidence (status + probe stats).
    "pass_trace_source",
    "executed_pass_order",
    "non_black_roi_passed",
    "roi_mean_brightness",
    "gi_indirect_passed",
    "gi_paired_delta",
    "gi_paired_passed",
    "sdf_readback_status",
    "sdf_evidence_source",
    "sdf_readback_passed",
    "overall_passed",
    "scene_input_hash",
    "fixture_version",
    "scene_source",
    "timings",
    "failures",
})

REQUIRED_CAMERA_KEYS = frozenset({"target_x", "target_y", "zoom"})
REQUIRED_ROI_KEYS = frozenset({"x", "y", "width", "height"})


def utc_now_iso() -> str:
    """Return current UTC timestamp in ISO-8601 format."""
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


@dataclass
class HardwareGateConfig:
    """Configuration options for GPU hardware gate execution."""

    revision: str = "HEAD"
    test_exe: Path = Path("bin/NoMoreDay.exe")
    output_dir: Path = Path("artifacts/gpu-gate/HEAD")
    sample_frames: int = 120
    toggle_loops: int = 100
    stress_test_1min: bool = True
    waiver: dict[str, str] | None = None


def gate_timeout_seconds(config: HardwareGateConfig) -> int:
    """Return the subprocess timeout budget for a gate config.

    The timeout is linked to the stress duration: a 1-minute stress run adds
    60s to the base budget, so a stress-enabled run always gets at least
    `GATE_BASE_TIMEOUT_SECONDS + GATE_STRESS_ADDED_SECONDS`.
    """
    extra = GATE_STRESS_ADDED_SECONDS if config.stress_test_1min else 0
    return GATE_BASE_TIMEOUT_SECONDS + extra


def build_gate_env(config: HardwareGateConfig) -> dict[str, str]:
    """Assemble the environment variables forwarded to the C++ gate.

    ``NoMoreDay.exe --gpu-gate`` reads these (with matching defaults) and feeds
    them into ``RunGate``; argv (``--revision``) is passed on the command line
    while the sample/toggle/stress knobs travel through the environment, wiring
    the runner CLI parameters into the game-binary gate.
    """
    return {
        GATE_ENV_SAMPLES: str(config.sample_frames),
        GATE_ENV_TOGGLE_LOOPS: str(config.toggle_loops),
        GATE_ENV_STRESS: "1" if config.stress_test_1min else "0",
    }


def build_waiver(
    authorizer: str = "",
    reason: str = "",
    scope: str = "",
    expiry: str = "",
) -> dict[str, str] | None:
    """Build waiver metadata, or ``None`` when no waiver fields are provided.

    A waiver is archival metadata describing who accepted a deviation, its
    scope and expiry. It never changes the gate verdict: ``gate_succeeded``
    still requires ``return_code == 0`` and ``status == "GO"``.
    """
    values = {
        "authorizer": authorizer,
        "reason": reason,
        "scope": scope,
        "expiry": expiry,
    }
    if not any(values.values()):
        return None
    return {key: value for key, value in values.items() if value}


def parse_cpp_gate_status(output: str) -> str | None:
    """Parse the exact structured GateReport status from C++ output.

    Args:
        output: Combined or captured C++ runner output.

    Returns:
        The unique C++ gate status, or ``None`` if it is missing or invalid.
    """
    matches = re.findall(
        r"^GPU_HARDWARE_GATE_RESULT\s+status=(GO|NO_GO|NOT_RUN)\s*$",
        output,
        flags=re.MULTILINE,
    )
    if len(matches) != 1:
        return None
    return matches[0]


def extract_cpp_gate_report(output: str) -> dict[str, Any] | None:
    """Extract the full C++ GateReport JSON from runner output.

    The C++ gate runner emits the complete GateReport between
    ``GPU_HARDWARE_GATE_REPORT_BEGIN`` and ``GPU_HARDWARE_GATE_REPORT_END``
    markers. Parsing this source (rather than only the status line) is the S3
    contract: the archived artifact must contain matrix/timer/resource/GL
    diagnostics, not just a verdict.

    Args:
        output: Combined C++ runner stdout.

    Returns:
        The parsed report object, or ``None`` when the markers are absent or
        the payload is not a single JSON object.
    """
    start = output.find(GATE_REPORT_BEGIN_MARKER)
    end = output.find(GATE_REPORT_END_MARKER)
    if start == -1 or end == -1 or end <= start:
        return None
    payload = output[start + len(GATE_REPORT_BEGIN_MARKER):end].strip()
    try:
        parsed = json.loads(payload)
    except (json.JSONDecodeError, ValueError):
        return None
    if not isinstance(parsed, dict):
        return None
    return parsed


def validate_gate_report_schema(report: dict[str, Any]) -> list[str]:
    """Return schema violations for a parsed C++ GateReport.

    Args:
        report: Parsed GateReport JSON object.

    Returns:
        A list of violation strings; an empty list means the report is valid.
        ``NOT_RUN`` reports (e.g. missing debug callback) are still schema
        valid -- they carry the fail-closed verdict and never count as GO.
    """
    errors: list[str] = []
    if not isinstance(report, dict):
        return ["gate report must be a JSON object"]

    for key in sorted(REQUIRED_TOP_LEVEL_KEYS):
        if key not in report:
            errors.append(f"missing required top-level key: {key}")

    status = report.get("gate_status")
    if status not in GATE_STATUS_VALUES:
        errors.append(
            f"gate_status must be one of {sorted(GATE_STATUS_VALUES)}, "
            f"got {status!r}"
        )

    capabilities = report.get("capabilities")
    if not isinstance(capabilities, dict):
        errors.append("capabilities must be an object")
    else:
        for key in sorted(REQUIRED_CAPABILITY_KEYS):
            if key not in capabilities:
                errors.append(f"capabilities missing required key: {key}")
        # W6 (M0-C) High-3: GPU identity is mandatory evidence - an empty
        # vendor/driver_version means the context did not expose its identity
        # and must be rejected (never default-filled).
        vendor = capabilities.get("vendor")
        if not isinstance(vendor, str) or not vendor.strip():
            errors.append("capabilities.vendor must be a non-empty string")
        driver_version = capabilities.get("driver_version")
        if not isinstance(driver_version, str) or not driver_version.strip():
            errors.append(
                "capabilities.driver_version must be a non-empty string"
            )

    run_config = report.get("run_config")
    if not isinstance(run_config, dict):
        errors.append("run_config must be an object")
    else:
        for key in sorted(REQUIRED_RUN_CONFIG_KEYS):
            if key not in run_config:
                errors.append(f"run_config missing required key: {key}")
        non_exhaustive = run_config.get("non_exhaustive")
        # W6 (M0-C) Medium-5: a GO verdict is impossible on a non-exhaustive
        # (below-production-floor) run; reject it at the schema level too.
        if status == "GO" and non_exhaustive is True:
            errors.append(
                "GO is impossible while run_config.non_exhaustive is true "
                "(diagnostic sampling below production floor)"
            )

    occupancy = report.get("occupancy")
    if not isinstance(occupancy, dict):
        errors.append("occupancy must be an object")
    else:
        for key in sorted(REQUIRED_OCCUPANCY_KEYS):
            if key not in occupancy:
                errors.append(f"occupancy missing required key: {key}")
        # W6 (M0-C) Blocker-1: occupancy evidence missing_pending_m0a (M0-A R3
        # not implemented) blocks GO; a GO verdict must never be accepted while
        # occupancy evidence is missing or flagged as blocking.
        occ_status = occupancy.get("status")
        if not isinstance(occ_status, str) or not occ_status.strip():
            errors.append("occupancy.status must be a non-empty string")
        if status == "GO" and occupancy.get("blocks_go") is not False:
            errors.append(
                "GO is impossible while occupancy evidence is missing/blocked "
                f"(occupancy.status={occ_status!r})"
            )

    gl_diagnostics = report.get("gl_diagnostics")
    if not isinstance(gl_diagnostics, dict):
        errors.append("gl_diagnostics must be an object")
    else:
        for key in sorted(REQUIRED_GL_DIAGNOSTIC_KEYS):
            if key not in gl_diagnostics:
                errors.append(f"gl_diagnostics missing required key: {key}")
        messages = gl_diagnostics.get("messages")
        if not isinstance(messages, list):
            errors.append("gl_diagnostics.messages must be an array")
        else:
            for index, message in enumerate(messages):
                if not isinstance(message, dict):
                    errors.append(
                        f"gl_diagnostics.messages[{index}] must be an object"
                    )
                    continue
                for key in sorted(REQUIRED_DIAGNOSTIC_MESSAGE_KEYS):
                    if key not in message:
                        errors.append(
                            "gl_diagnostics.messages["
                            f"{index}] missing required key: {key}"
                        )

    resources = report.get("resources")
    if not isinstance(resources, dict):
        errors.append("resources must be an object")

    stress_test = report.get("stress_test")
    if not isinstance(stress_test, dict):
        errors.append("stress_test must be an object")

    matrix_results = report.get("matrix_results")
    if not isinstance(matrix_results, list):
        errors.append("matrix_results must be an array")
    else:
        for index, cell in enumerate(matrix_results):
            if not isinstance(cell, dict):
                errors.append(f"matrix_results[{index}] must be an object")
                continue
            for key in sorted(REQUIRED_MATRIX_CELL_KEYS):
                if key not in cell:
                    errors.append(
                        f"matrix_results[{index}] missing required key: {key}"
                    )
            camera = cell.get("camera")
            if not isinstance(camera, dict):
                errors.append(f"matrix_results[{index}].camera must be an object")
            else:
                for key in sorted(REQUIRED_CAMERA_KEYS):
                    if key not in camera:
                        errors.append(
                            f"matrix_results[{index}].camera missing required key: {key}"
                        )
            roi = cell.get("roi")
            if not isinstance(roi, dict):
                errors.append(f"matrix_results[{index}].roi must be an object")
            else:
                for key in sorted(REQUIRED_ROI_KEYS):
                    if key not in roi:
                        errors.append(
                            f"matrix_results[{index}].roi missing required key: {key}"
                        )
            # W6 (M0-C) Blocker-1: real pass trace must be an array, and the
            # SDF evidence status must be one of the declared values.
            executed_order = cell.get("executed_pass_order")
            if not isinstance(executed_order, list):
                errors.append(
                    f"matrix_results[{index}].executed_pass_order must be an array"
                )
            sdf_status = cell.get("sdf_readback_status")
            if sdf_status not in SDF_READBACK_STATUSES:
                errors.append(
                    f"matrix_results[{index}].sdf_readback_status must be one of "
                    f"{sorted(SDF_READBACK_STATUSES)}, got {sdf_status!r}"
                )
            pass_trace_source = cell.get("pass_trace_source")
            if not isinstance(pass_trace_source, str) or not pass_trace_source.strip():
                errors.append(
                    f"matrix_results[{index}].pass_trace_source must be a "
                    "non-empty string"
                )
            sdf_evidence_source = cell.get("sdf_evidence_source")
            if not isinstance(sdf_evidence_source, str) or not sdf_evidence_source.strip():
                errors.append(
                    f"matrix_results[{index}].sdf_evidence_source must be a "
                    "non-empty string"
                )

    global_failures = report.get("global_failures")
    if not isinstance(global_failures, list):
        errors.append("global_failures must be an array")

    return errors


def gate_succeeded(return_code: int, gate_status: str | None) -> bool:
    """Return whether the C++ process and GateReport both passed.

    Args:
        return_code: C++ runner process return code.
        gate_status: Parsed C++ GateReport status.

    Returns:
        ``True`` only for a zero return code and an exact ``GO`` status.
    """
    return return_code == 0 and gate_status == "GO"


def run_hardware_gate_cpp(
    config: HardwareGateConfig,
) -> tuple[int, str, str]:
    """Execute the production game-binary GPU Hardware Validation Gate.

    W6 (M0-C): the production gate is ``NoMoreDay.exe --gpu-gate`` (normal
    Game/App initialization, real GL context/registry/hooks/render path). The
    standalone test binary is contract/diagnostic only and is no longer invoked
    here. The doctest filter is gone; the game binary emits exactly one
    ``GPU_HARDWARE_GATE_RESULT status=...`` marker and one versioned JSON
    report between BEGIN/END markers.

    Args:
        config: Configuration parameters for execution.

    Returns:
        Tuple of (return_code, stdout, stderr).
    """
    if not config.test_exe.exists():
        return (
            1,
            "",
            f"Executable not found at {config.test_exe}",
        )

    cmd = [
        str(config.test_exe),
        "--gpu-gate",
        "--revision",
        config.revision,
    ]

    # W6 (M0-C): forward the CLI knobs to the game binary. The binary reads
    # NMD_GATE_* (argv wins over env, env wins over defaults) and passes them
    # into RunGate, so --samples/--toggle-loops/--stress-test-1min stay wired.
    gate_env = os.environ.copy()
    gate_env.update(build_gate_env(config))

    # S8: timeout is derived from the stress duration so a 1-minute stress run
    # always fits inside the subprocess budget.
    timeout = gate_timeout_seconds(config)

    try:
        # W6 (M0-C): decode the game binary output as UTF-8 with replacement so
        # a locale-codepage mismatch (Windows default is GBK) can never crash
        # the reader threads and silently null out stdout/stderr. The ASCII
        # status marker and the UTF-8 JSON report survive intact.
        proc = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            env=gate_env,
            timeout=timeout,
            check=False,
        )
        return (proc.returncode, proc.stdout, proc.stderr)
    except subprocess.TimeoutExpired:
        return (
            1,
            "",
            f"Execution timed out after {timeout}s; increase --samples or "
            "disable --stress-test-1min if the gate needs more budget",
        )
    except Exception as exc:
        return (1, "", f"Execution exception: {exc}")


def write_artifact(output_dir: Path, artifact: dict[str, Any]) -> Path:
    """Write artifact JSON to specified output directory.

    Args:
        output_dir: Target output directory.
        artifact: Artifact payload.

    Returns:
        Path to written file.
    """
    output_dir.mkdir(parents=True, exist_ok=True)
    out_file = output_dir / "gpu_hardware_validation_artifact.json"

    with open(out_file, "w", encoding="utf-8") as file_handle:
        json.dump(artifact, file_handle, indent=2, ensure_ascii=False)

    return out_file


def validate_archived_artifact(path: Path) -> int:
    """Validate an archived artifact JSON against the gate report schema.

    CI entry point for post-archive schema checks: loads the artifact, runs the
    same schema validator the runner uses, and returns a process exit code.
    """
    try:
        artifact = json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError, ValueError) as exc:
        print(f"cannot read artifact {path}: {exc}")
        return 1

    report = artifact.get("gate_report")
    errors = (
        validate_gate_report_schema(report)
        if isinstance(report, dict)
        else ["artifact is missing a gate_report object"]
    )
    if errors:
        print(f"schema violations in {path}:")
        for error in errors:
            print(f"  - {error}")
        return 1

    status = artifact.get("gate_status")
    succeeded = artifact.get("gate_succeeded", False)
    print(
        f"[Schema] {path}: gate_status={status} gate_succeeded={succeeded} "
        "schema OK"
    )
    return 0


def main() -> int:
    """Main CLI entry point for hardware validation gate runner."""
    parser = argparse.ArgumentParser(
        description="GPU Hardware Validation Gate Runner"
    )
    parser.add_argument(
        "--test-exe",
        type=Path,
        default=Path("bin/NoMoreDay.exe"),
        help="Path to the game binary (default: bin/NoMoreDay.exe; the "
        "production gate runs NoMoreDay.exe --gpu-gate, not the doctest "
        "binary)",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=None,
        help="Output directory for gate artifacts (default: "
        "artifacts/gpu-gate/<revision>/ on the archive path)",
    )
    parser.add_argument(
        "--revision",
        type=str,
        default="HEAD",
        help="Git revision or commit SHA",
    )
    parser.add_argument(
        "--samples",
        type=int,
        default=120,
        help="Sample frames per fixture (forwarded via NMD_GATE_SAMPLES)",
    )
    parser.add_argument(
        "--toggle-loops",
        type=int,
        default=100,
        help="Number of GI/tier/resize toggle loops to execute "
        "(forwarded via NMD_GATE_TOGGLE_LOOPS)",
    )
    parser.add_argument(
        "--stress-test-1min",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Run the 60-second stress loop (forwarded via NMD_GATE_STRESS)",
    )
    parser.add_argument(
        "--waiver-authorizer",
        type=str,
        default="",
        help="Waiver authorizer (who approved the deviation)",
    )
    parser.add_argument(
        "--waiver-reason",
        type=str,
        default="",
        help="Waiver reason (why the deviation is accepted)",
    )
    parser.add_argument(
        "--waiver-scope",
        type=str,
        default="",
        help="Waiver scope (which config/hardware the waiver applies to)",
    )
    parser.add_argument(
        "--waiver-expiry",
        type=str,
        default="",
        help="Waiver expiry (date or 'N/A'); a waiver never turns "
        "NOT_RUN/NO_GO into GO",
    )
    parser.add_argument(
        "--validate-schema",
        type=Path,
        default=None,
        metavar="ARTIFACT_JSON",
        help="Validate an archived artifact JSON against the gate report "
        "schema and exit (CI post-archive check); no C++ run",
    )

    args = parser.parse_args()

    if args.validate_schema is not None:
        return validate_archived_artifact(args.validate_schema)

    output_dir = args.output_dir or Path("artifacts") / "gpu-gate" / args.revision

    config = HardwareGateConfig(
        revision=args.revision,
        test_exe=args.test_exe,
        output_dir=output_dir,
        sample_frames=args.samples,
        toggle_loops=args.toggle_loops,
        stress_test_1min=args.stress_test_1min,
        waiver=build_waiver(
            authorizer=args.waiver_authorizer,
            reason=args.waiver_reason,
            scope=args.waiver_scope,
            expiry=args.waiver_expiry,
        ),
    )

    print(f"=== GPU Hardware Validation Gate Runner ({config.revision}) ===")
    print(f"Test Exe: {config.test_exe}")
    print(f"Output Dir: {config.output_dir}")
    print(
        f"Params: samples={config.sample_frames} "
        f"toggle_loops={config.toggle_loops} "
        f"stress_1min={config.stress_test_1min}"
    )

    return_code, stdout, stderr = run_hardware_gate_cpp(config)

    parsed_status = parse_cpp_gate_status(stdout)
    gate_report = extract_cpp_gate_report(stdout)
    schema_errors = (
        validate_gate_report_schema(gate_report)
        if gate_report is not None
        else ["full gate report JSON not found in C++ output"]
    )

    if gate_report is not None:
        report_status = gate_report.get("gate_status", "NOT_RUN")
        if report_status not in GATE_STATUS_VALUES:
            report_status = parsed_status or "NOT_RUN"
    else:
        report_status = parsed_status or "NOT_RUN"

    gate_status = report_status
    succeeded = gate_succeeded(return_code, gate_status)
    meets_preflight = succeeded and not schema_errors

    artifact = {
        "revision": config.revision,
        "timestamp": utc_now_iso(),
        "runner": "NoMoreDay.exe --gpu-gate (production game-binary gate)",
        "gate_status": gate_status,
        "gate_succeeded": succeeded,
        "meets_preflight": meets_preflight,
        "waiver": config.waiver,
        "return_code": return_code,
        "gate_report": gate_report,
        "gate_report_schema_errors": schema_errors,
        "stdout_summary": stdout.strip(),
        "stderr_summary": stderr.strip(),
    }

    out_path = write_artifact(config.output_dir, artifact)

    print(f"Gate Status: {gate_status}")
    print(f"Artifact written to: {out_path}")
    if config.waiver:
        print(
            f"Waiver recorded (metadata only; does not change verdict): "
            f"{config.waiver}"
        )
    if schema_errors:
        print("Gate report schema violations:")
        for error in schema_errors:
            print(f"  - {error}")

    return 0 if meets_preflight else 1


if __name__ == "__main__":
    sys.exit(main())
