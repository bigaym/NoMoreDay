#!/usr/bin/env python3
"""Capture best-effort host hardware profile for perf baselines."""

from __future__ import annotations

import argparse
import datetime as dt
import json
import os
import platform
import subprocess
import sys
from pathlib import Path


def run_powershell_json(command: str) -> dict[str, object] | None:
    completed = subprocess.run(
        ["powershell", "-NoProfile", "-Command", command],
        check=False,
        capture_output=True,
        text=True,
    )
    if completed.returncode != 0:
        return None
    text = completed.stdout.strip()
    if not text:
        return None
    try:
        parsed = json.loads(text)
    except json.JSONDecodeError:
        return None
    if isinstance(parsed, list):
        return parsed[0] if parsed else None
    if isinstance(parsed, dict):
        return parsed
    return None


def mib_from_bytes(value: object) -> int | None:
    try:
        num = int(str(value))
    except (TypeError, ValueError):
        return None
    if num < 0:
        return None
    return num // (1024 * 1024)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Capture host profile for perf evidence"
    )
    parser.add_argument(
        "--output-json",
        default="docs/reports/four-pillars/phase-0/P0-2/host-profile.json",
        help="Output JSON path",
    )
    parsed = parser.parse_args()

    os_info = {
        "platform": platform.platform(),
        "system": platform.system(),
        "release": platform.release(),
        "version": platform.version(),
    }

    cpu_info = (
        run_powershell_json(
            "Get-CimInstance Win32_Processor | "
            "Select-Object Name,NumberOfLogicalProcessors | ConvertTo-Json -Compress"
        )
        or {}
    )
    gpu_info = (
        run_powershell_json(
            "Get-CimInstance Win32_VideoController | "
            "Select-Object Name,DriverVersion | ConvertTo-Json -Compress"
        )
        or {}
    )
    ram_info = (
        run_powershell_json(
            "Get-CimInstance Win32_ComputerSystem | "
            "Select-Object TotalPhysicalMemory | ConvertTo-Json -Compress"
        )
        or {}
    )

    profile = {
        "captured_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "os": os_info,
        "cpu": {
            "name": cpu_info.get("Name") if isinstance(cpu_info, dict) else None,
            "logical_cores": (
                int(cpu_info.get("NumberOfLogicalProcessors"))
                if isinstance(cpu_info, dict)
                and cpu_info.get("NumberOfLogicalProcessors") is not None
                else os.cpu_count()
            ),
        },
        "gpu": {
            "name": gpu_info.get("Name") if isinstance(gpu_info, dict) else None,
            "driver_version": (
                gpu_info.get("DriverVersion") if isinstance(gpu_info, dict) else None
            ),
        },
        "ram": {
            "total_physical_memory_mib": (
                mib_from_bytes(ram_info.get("TotalPhysicalMemory"))
                if isinstance(ram_info, dict)
                else None
            )
        },
    }

    output_path = Path(parsed.output_json)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(profile, indent=2), encoding="utf-8")
    print(f"wrote {output_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
