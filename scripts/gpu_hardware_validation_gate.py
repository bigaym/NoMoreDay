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
    gate_status = parsed_status or "NOT_RUN"
    meets_preflight = gate_succeeded(return_code, parsed_status)

    artifact = {
        "revision": config.revision,
        "timestamp": utc_now_iso(),
        "runner": "C++ NoMoreDayTests GPUHardwareValidationGate",
        "gate_status": gate_status,
        "meets_preflight": meets_preflight,
        "return_code": return_code,
        "stdout_summary": stdout.strip(),
        "stderr_summary": stderr.strip(),
    }

    out_path = write_artifact(config.output_dir, artifact)

    print(f"Gate Status: {gate_status}")
    print(f"Artifact written to: {out_path}")

    return 0 if meets_preflight else 1


if __name__ == "__main__":
    sys.exit(main())
