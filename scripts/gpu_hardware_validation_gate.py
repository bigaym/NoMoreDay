#!/usr/bin/env python3
"""GPU Hardware Validation Gate runner and artifact exporter.

Executes the C++ GPU Hardware Validation Gate matrix, verifies preflight,
timing budgets, resource registries, and outputs structured artifact reports.
"""

from __future__ import annotations

import argparse
import json
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
})

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


def utc_now_iso() -> str:
    """Return current UTC timestamp in ISO-8601 format."""
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


@dataclass
class HardwareGateConfig:
    """Configuration options for GPU hardware gate execution."""

    revision: str = "HEAD"
    test_exe: Path = Path("bin/NoMoreDayTests.exe")
    output_dir: Path = Path("bin/gpu_hardware_gate")
    sample_frames: int = 120
    toggle_loops: int = 100
    stress_test_1min: bool = True


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
    """Execute C++ GPU Hardware Validation Gate runner.

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
        "--test-case=*GPU Hardware Validation Gate*",
    ]

    try:
        proc = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=180,
            check=False,
        )
        return (proc.returncode, proc.stdout, proc.stderr)
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


def main() -> int:
    """Main CLI entry point for hardware validation gate runner."""
    parser = argparse.ArgumentParser(
        description="GPU Hardware Validation Gate Runner"
    )
    parser.add_argument(
        "--test-exe",
        type=Path,
        default=Path("bin/NoMoreDayTests.exe"),
        help="Path to NoMoreDayTests executable",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("bin/gpu_hardware_gate"),
        help="Output directory for gate artifacts",
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
        help="Sample frames per fixture",
    )
    parser.add_argument(
        "--toggle-loops",
        type=int,
        default=100,
        help="Number of GI/tier/resize toggle loops to execute",
    )

    args = parser.parse_args()

    config = HardwareGateConfig(
        revision=args.revision,
        test_exe=args.test_exe,
        output_dir=args.output_dir,
        sample_frames=args.samples,
        toggle_loops=args.toggle_loops,
    )

    print(f"=== GPU Hardware Validation Gate Runner ({config.revision}) ===")
    print(f"Test Exe: {config.test_exe}")
    print(f"Output Dir: {config.output_dir}")

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
    meets_preflight = (
        gate_succeeded(return_code, gate_status) and not schema_errors
    )

    artifact = {
        "revision": config.revision,
        "timestamp": utc_now_iso(),
        "runner": "C++ NoMoreDayTests GPUHardwareValidationGate",
        "gate_status": gate_status,
        "meets_preflight": meets_preflight,
        "return_code": return_code,
        "gate_report": gate_report,
        "gate_report_schema_errors": schema_errors,
        "stdout_summary": stdout.strip(),
        "stderr_summary": stderr.strip(),
    }

    out_path = write_artifact(config.output_dir, artifact)

    print(f"Gate Status: {gate_status}")
    print(f"Artifact written to: {out_path}")
    if schema_errors:
        print("Gate report schema violations:")
        for error in schema_errors:
            print(f"  - {error}")

    return 0 if meets_preflight else 1


if __name__ == "__main__":
    sys.exit(main())
