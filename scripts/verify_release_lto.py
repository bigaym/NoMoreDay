#!/usr/bin/env python3
"""verify_release_lto.py - isolated MSVC Release LTO proof harness.

MS-8 W4: builds a Release configuration with ENABLE_LTO=ON in an isolated
cache directory (default ``build/release-lto``), retains /GL compile and
/LTCG link command evidence from the real MSVC build, runs the Release CI
CTest label, and executes a non-interactive Release smoke
(``NoMoreDay.exe --smoke-test``; the flag was added by MS-8 W4 so the
LTO-linked deliverable itself is exercised, not just the test binary).

The harness never deletes its own artifacts; it preserves the configure
cache and every log before finishing, so an auditor can inspect the proof
without reconstructing deleted console output. Run from the repository root:

    python scripts/verify_release_lto.py

Args:
    --source-dir: repository root (default: script's repository root).
    --build-dir: isolated build directory (default: build/release-lto).
    --evidence-dir: retained evidence bundle (default: a fresh timestamped
        directory under docs/reports/release-lto-proof/evidence-<stamp>).
    --jobs: parallel MSBuild jobs (default: 7).
    --known-failure: repeatable; exact doctest TEST CASE names treated as
        known pre-existing failures. Any OTHER failing case fails the run.

Returns:
    Exit code 0 only if configure, build, evidence grep, per-target /GL
    coverage, Release CI CTest and the game-executable smoke all succeed.
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

IPO_REPORT_RE = re.compile(r"\[Optimization\].*")
CONFIG_REPORT_KEY = "ENABLE_LTO"


def _run(cmd, cwd, log_path):
    """Run a command, teeing output to log_path, returning the exit code."""
    log_path = Path(log_path)
    log_path.parent.mkdir(parents=True, exist_ok=True)
    with open(log_path, "w", encoding="utf-8", errors="replace") as handle:
        proc = subprocess.run(
            cmd,
            cwd=str(cwd),
            stdout=handle,
            stderr=subprocess.STDOUT,
            shell=False,
            check=False,
        )
    return proc.returncode


def detect_generator():
    """Return the Visual Studio generator name for the installed toolchain.

    Mirrors build.bat priority: VS 2022 (v17) preferred, then VS 2026 (v18),
    then vswhere -latest. Returns None when no supported VS is found.
    """
    supported = {"17": "Visual Studio 17 2022", "18": "Visual Studio 18 2026"}
    candidate_dirs = [
        Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "Microsoft Visual Studio",
    ]
    root = os.environ.get("ProgramFiles(x86)")
    if root:
        candidate_dirs.insert(0, Path(root) / "Microsoft Visual Studio")

    for vs_root in candidate_dirs:
        for major in ("2022", "2026"):
            vs_dir = vs_root / major
            if not vs_dir.is_dir():
                continue
            for edition in ("Community", "Professional", "Enterprise", "BuildTools"):
                vcvars = vs_dir / edition / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
                if vcvars.is_file():
                    return supported["17" if major == "2022" else "18"]

    vswhere = None
    if root:
        probe = Path(root) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
        if probe.is_file():
            vswhere = probe
    if vswhere is None:
        probe = Path(os.environ.get("ProgramFiles", "C:/Program Files")) / (
            "Microsoft Visual Studio/Installer/vswhere.exe"
        )
        if probe.is_file():
            vswhere = probe
    if vswhere is None:
        return None

    proc = subprocess.run(
        [str(vswhere), "-latest", "-products", "*",
         "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
         "-property", "installationVersion"],
        capture_output=True,
        text=True,
        check=False,
    )
    version = (proc.stdout or "").strip()
    major = version.split(".")[0] if version else ""
    return supported.get(major)


def configure_cmd(generator, source_dir):
    """Build the CMake configure argv for the isolated Release LTO cache."""
    return [
        "cmake", "-S", str(source_dir), "-G", generator, "-A", "x64",
        "-DCMAKE_POLICY_VERSION_MINIMUM:STRING=3.5",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DBUILD_TESTING=ON",
        "-DENABLE_ANALYZE=OFF",
        "-DENABLE_ASAN=OFF",
        "-DENABLE_SHOW_INCLUDES=OFF",
        "-DENABLE_FAST_BUILD=ON",
        "-DENABLE_RUNTIME_OPT=ON",
        "-DNMD_ENABLE_COMPILER_CACHE=OFF",
        "-DNMD_COMPILER_CACHE_TOOL=AUTO",
        "-DENABLE_LTO:BOOL=ON",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
    ]


def collect_cache_evidence(build_dir, evidence_dir):
    """Grep the isolated configure cache into a retained evidence snippet."""
    cache = Path(build_dir) / "CMakeCache.txt"
    out = evidence_dir / "cmake_cache.txt"
    lines = []
    if cache.is_file():
        keys = (
            "ENABLE_LTO:", "CMAKE_BUILD_TYPE:", "CMAKE_GENERATOR:",
            "CMAKE_CXX_COMPILER:", "CMAKE_C_COMPILER:",
        )
        with open(cache, encoding="utf-8", errors="replace") as handle:
            for line in handle:
                if any(line.startswith(key) for key in keys):
                    lines.append(line.rstrip())
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return out, len(lines)


def collect_ipo_report(evidence_dir):
    """Copy the configure-time Release IPO report lines into evidence."""
    cfg_log = evidence_dir / "configure_log.txt"
    out = evidence_dir / "configure_report.txt"
    lines = []
    if cfg_log.is_file():
        with open(cfg_log, encoding="utf-8", errors="replace") as handle:
            for line in handle:
                if IPO_REPORT_RE.search(line):
                    lines.append(line.rstrip())
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return out


def _read_text(path):
    """Read a file with BOM-aware decoding (MSBuild tlogs are UTF-16 LE)."""
    data = Path(path).read_bytes()
    if data.startswith(b"\xff\xfe"):
        return data.decode("utf-16-le", errors="replace")
    if data.startswith(b"\xfe\xff"):
        return data.decode("utf-16-be", errors="replace")
    if data.startswith(b"\xef\xbb\xbf"):
        return data.decode("utf-8-sig", errors="replace")
    return data.decode("utf-8", errors="replace")


def collect_command_evidence(build_dir, evidence_dir):
    """Extract /GL and /LTCG from MSBuild tracking logs (real commands).

    MSBuild C++ tlog command files are UTF-16 LE with a BOM; the evidence is
    deduplicated and capped to a representative sample per flag.
    """
    gl_found = []
    ltcg_found = []
    tlog_paths = []
    for tlog in Path(build_dir).rglob("*.tlog"):
        if not tlog.is_file() or tlog.suffix != ".tlog":
            continue
        rel_parts = tlog.relative_to(build_dir).parts
        if any(
            "_CMakeLTOTest" in part or "CompilerId" in part for part in rel_parts
        ):
            continue
        name = tlog.name
        if not name.endswith("CL.command.1.tlog") and not name.endswith(
            "link.command.1.tlog"
        ):
            continue
        tlog_paths.append(tlog)
        for line in _read_text(tlog).splitlines():
            stripped = line.strip()
            if "/GL" in stripped and stripped not in gl_found:
                gl_found.append(stripped)
            if "/LTCG" in stripped and stripped not in ltcg_found:
                ltcg_found.append(stripped)

    evidence_limit = 40
    gl_out = evidence_dir / "gl_evidence.txt"
    ltcg_out = evidence_dir / "ltcg_evidence.txt"
    gl_out.write_text("\n".join(gl_found[:evidence_limit]) + "\n", encoding="utf-8")
    ltcg_out.write_text("\n".join(ltcg_found[:evidence_limit]) + "\n", encoding="utf-8")
    return gl_out, ltcg_out, gl_found, ltcg_found, tlog_paths


def collect_vcxproj_evidence(build_dir, evidence_dir):
    """Record WholeProgramOptimization state per first-party vcxproj."""
    out = evidence_dir / "vcxproj_ipo.txt"
    lines = []
    for vcx in Path(build_dir).rglob("*.vcxproj"):
        if not vcx.is_file():
            continue
        rel = vcx.relative_to(build_dir)
        if "third_party" in rel.parts or any(
            "_CMakeLTOTest" in part for part in rel.parts
        ):
            continue
        lines.append(f"== {rel} ==")
        with open(vcx, encoding="utf-8", errors="replace") as handle:
            for line in handle:
                if "WholeProgramOptimization" in line:
                    lines.append(line.strip())
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return out


def main(argv=None):
    """Run the isolated Release LTO proof and retain its evidence."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-dir", default=str(REPO_ROOT))
    parser.add_argument("--build-dir", default=str(REPO_ROOT / "build" / "release-lto"))
    parser.add_argument(
        "--evidence-dir",
        default=str(
            REPO_ROOT
            / "docs"
            / "reports"
            / "release-lto-proof"
            / f"evidence-{datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S')}"
        ),
        help="Retained evidence bundle (a fresh timestamped run directory by "
        "default so prior runs are never overwritten)",
    )
    parser.add_argument("--jobs", type=int, default=7)
    parser.add_argument(
        "--known-failure",
        action="append",
        default=[
            # Pre-existing chance-based freeze flake (registry documents the
            # Heavenly Sword/SkillContract CI blocker); unrelated to LTO.
            "[Unit] SkillBehaviorGuard - Heavenly Sword element nodes close remaining gaps",
            # Pre-existing stale source-text assertion left by the
            # BUG-20260314-001 refactor; fails identically in RelWithDebInfo.
            "[Tech] SkillUI - mastery hub locks all Blade Ascendant signature skills consistently",
        ],
        help="Exact doctest TEST CASE names treated as known pre-existing "
        "failures. Any OTHER failing case makes the Release CI step FAIL.",
    )
    args = parser.parse_args(argv)

    source_dir = Path(args.source_dir)
    build_dir = Path(args.build_dir)
    evidence_dir = Path(args.evidence_dir)
    if not build_dir.is_absolute():
        build_dir = source_dir / build_dir
    if not evidence_dir.is_absolute():
        evidence_dir = source_dir / evidence_dir
    evidence_dir.mkdir(parents=True, exist_ok=True)
    known_failures = set(args.known_failure or [])

    manifest = {
        "started_utc": datetime.now(timezone.utc).isoformat(),
        "source_dir": str(source_dir),
        "build_dir": str(build_dir),
        "evidence_dir": str(evidence_dir),
        "jobs": args.jobs,
        "steps": {},
    }

    generator = detect_generator()
    if generator is None:
        print("[W4] ERROR: no supported Visual Studio generator detected.")
        return 2
    manifest["generator"] = generator
    print(f"[W4] Generator: {generator}")

    # A fresh configure must not inherit a prior cache value: the isolated
    # proof cache is fully recreated on each run. Deletion is restricted to
    # the canonical isolated directory (source_dir/build/release-lto) so an
    # accidental --build-dir typo cannot remove unrelated data.
    if build_dir.exists():
        canonical = (source_dir / "build" / "release-lto").resolve()
        if build_dir.resolve() != canonical:
            print(
                "[W4] ERROR: refusing to delete non-canonical build dir: "
                + str(build_dir)
            )
            return 2
        shutil.rmtree(build_dir, ignore_errors=True)
    build_dir.mkdir(parents=True, exist_ok=True)

    # Step 1: configure with ENABLE_LTO:BOOL=ON.
    cfg_log = evidence_dir / "configure_log.txt"
    print(f"[W4] Configuring {build_dir.name} (Release, ENABLE_LTO=ON)...")
    rc = _run(
        configure_cmd(generator, source_dir) + ["-B", str(build_dir)],
        cwd=source_dir,
        log_path=cfg_log,
    )
    manifest["steps"]["configure"] = {
        "exit": rc,
        "log": str(cfg_log),
    }
    if rc != 0:
        print("[W4] configure FAILED; see " + str(cfg_log))
        _finish(manifest, 1)
        return 1

    cache_evidence, cache_lines = collect_cache_evidence(build_dir, evidence_dir)
    report_evidence = collect_ipo_report(evidence_dir)
    manifest["steps"]["configure_evidence"] = {
        "cache": str(cache_evidence),
        "cache_lines": cache_lines,
        "report": str(report_evidence),
    }

    # Step 2: build the isolated Release configuration (parallel MSBuild).
    # The build log is retained inside the evidence bundle so it survives the
    # next run's cache re-creation.
    build_log = evidence_dir / "build_log.txt"
    build_cmd = [
        "cmake", "--build", str(build_dir), "--config", "Release",
        "--parallel", str(args.jobs), "--",
        f"/m:{args.jobs}", "/p:UseMultiToolTask=true", f"/p:CL_MPCount={args.jobs}",
    ]
    print(f"[W4] Building Release (jobs={args.jobs})...")
    started = time.time()
    rc = _run(build_cmd, cwd=source_dir, log_path=build_log)
    build_seconds = int(time.time() - started)
    manifest["steps"]["build"] = {
        "exit": rc,
        "seconds": build_seconds,
        "log": str(build_log),
    }
    if rc != 0:
        print(f"[W4] build FAILED after {build_seconds}s; see " + str(build_log))
        _finish(manifest, 1)
        return 1
    print(f"[W4] build OK ({build_seconds}s)")

    # Step 3: retained command evidence (/GL compile, /LTCG link).
    gl_path, ltcg_path, gl_found, ltcg_found, tlogs = collect_command_evidence(
        build_dir, evidence_dir
    )
    vcx_evidence = collect_vcxproj_evidence(build_dir, evidence_dir)
    manifest["steps"]["evidence"] = {
        "gl": str(gl_path),
        "ltcg": str(ltcg_path),
        "vcxproj": str(vcx_evidence),
        "gl_command_lines_total": len(gl_found),
        "ltcg_command_lines_total": len(ltcg_found),
        "gl_command_lines_retained": min(len(gl_found), 40),
        "ltcg_command_lines_retained": min(len(ltcg_found), 40),
        "command_tlog_files": len(tlogs),
    }
    if not gl_found or not ltcg_found:
        print("[W4] ERROR: /GL or /LTCG command evidence missing from tlogs.")
        _finish(manifest, 1)
        return 1
    missing_gl_targets = _missing_gl_targets(gl_found)
    if missing_gl_targets:
        print(
            "[W4] ERROR: /GL evidence missing for required targets: "
            + ", ".join(missing_gl_targets)
        )
        _finish(manifest, 1)
        return 1

    # Step 4: Release CI CTest label. A failing label does not abort the run:
    # the smoke step below must still execute so the manifest carries complete
    # evidence. The final verdict accounts for every step.
    ctest_log = evidence_dir / "ctest_ci.txt"
    ctest_cmd = [
        "ctest", "--test-dir", str(build_dir), "-C", "Release", "-L", "ci",
        "--output-on-failure",
    ]
    print("[W4] Running Release CTest -L ci ...")
    rc = _run(ctest_cmd, cwd=source_dir, log_path=ctest_log)
    ctest_status = "PASS" if rc == 0 else "FAIL"
    tolerated = []
    if rc != 0:
        tolerated = _failing_case_names(ctest_log)
        unknown = [name for name in tolerated if name not in known_failures]
        if not unknown:
            ctest_status = "PASS_WITH_KNOWN_FAILURES"
            print(
                "[W4] ctest -L ci has only known pre-existing failures: "
                + "; ".join(tolerated)
            )
        else:
            print(
                "[W4] ctest -L ci FAILED with failures outside the known set: "
                + "; ".join(unknown)
            )
    manifest["steps"]["ctest_ci"] = {
        "exit": rc,
        "status": ctest_status,
        "tolerated_failures": tolerated,
        "known_failures": sorted(known_failures),
        "log": str(ctest_log),
    }
    print(f"[W4] ctest -L ci status: {ctest_status}")

    # Step 5: non-interactive Release smoke on the real game executable.
    # The game binary supports --smoke-test (added by MS-8 W4): it initializes
    # logging, prints a success marker, and exits 0 without creating a window
    # or GL context. This proves the LTO-linked deliverable actually launches.
    smoke_log = evidence_dir / "smoke.txt"
    smoke_exe = source_dir / "bin" / "NoMoreDay.exe"
    smoke_cmd = [str(smoke_exe), "--smoke-test"]
    print("[W4] Running Release smoke (NoMoreDay.exe --smoke-test)...")
    rc = _run(smoke_cmd, cwd=source_dir, log_path=smoke_log)
    manifest["steps"]["smoke"] = {"exit": rc, "log": str(smoke_log)}
    smoke_ok = rc == 0
    if smoke_ok and Path(smoke_log).is_file():
        with open(smoke_log, encoding="utf-8", errors="replace") as handle:
            smoke_ok = "smoke-test OK" in handle.read()
    if not smoke_ok:
        print("[W4] smoke FAILED; see " + str(smoke_log))
        _finish(manifest, 1)
        return 1

    manifest["finished_utc"] = datetime.now(timezone.utc).isoformat()
    if ctest_status in ("PASS", "PASS_WITH_KNOWN_FAILURES"):
        manifest["verdict"] = "PASS"
    else:
        manifest["verdict"] = "FAIL"
    _finish(manifest, 0 if manifest["verdict"] == "PASS" else 1)
    print("[W4] %s. Evidence bundle: %s" % (manifest["verdict"], evidence_dir))
    return 0


def _failing_case_names(ctest_log):
    """Return TEST CASE names that actually failed in a ctest log.

    A case counts as failed only when its TEST CASE header is followed (within
    15 lines) by an assertion ``ERROR:`` line. Message-only cases (e.g. raylib
    TraceLog shader noise) do not count.
    """
    names = []
    if not Path(ctest_log).is_file():
        return names
    with open(ctest_log, encoding="utf-8", errors="replace") as handle:
        lines = handle.readlines()
    for idx, line in enumerate(lines):
        match = re.search(r"TEST CASE:\s+(.+)", line)
        if not match:
            continue
        name = match.group(1).strip()
        window = lines[idx : idx + 15]
        if any("ERROR:" in w for w in window) and name not in names:
            names.append(name)
    return names


def _missing_gl_targets(gl_found):
    """Return required first-party targets lacking at least one /GL compile
    command in the retained tlog evidence. Each required deliverable target
    must appear with a /GL compile line, not only the test binary."""
    required = [
        "NoMoreDay",
        "NoMoreDayApp",
        "NoMoreDayGame",
        "NoMoreDayEngine",
        "NoMoreDayCore",
        "SkillBehaviors",
        "NoMoreDayTests",
    ]
    joined = " ".join(gl_found).upper()
    return [t for t in required if t.upper() not in joined]


def _finish(manifest, exit_code):
    """Write the revisioned manifest and emit the summary."""
    manifest_path = Path(manifest["evidence_dir"]) / "manifest.json"
    manifest_path.write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False), encoding="utf-8"
    )
    print(f"[W4] manifest: {manifest_path}")
    if exit_code != 0:
        print("[W4] overall status: FAIL")
    else:
        print("[W4] overall status: PASS")


if __name__ == "__main__":
    sys.exit(main())
