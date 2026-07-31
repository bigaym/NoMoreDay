import json
import os
import sys
import tempfile
import unittest
import unittest.mock
from pathlib import Path
from typing import Any


REPO_ROOT = Path(__file__).resolve().parents[2]
SCRIPTS_DIR = REPO_ROOT / "scripts"
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))


import gpu_hardware_validation_gate  # noqa: E402


def _build_minimal_report(gate_status: str = "GO") -> dict[str, Any]:
    """Return a schema-valid GateReport JSON payload for tests."""
    return {
        "revision": "TEST_REV",
        "timestamp": "2026-08-01T00:00:00Z",
        "gate_status": gate_status,
        "capabilities": {
            "vendor": "TestVendor",
            "renderer": "TestRenderer",
            "driver_version": "TestDriver",
            "gl_version": "4.3",
            "compute_shader": True,
            "ssbo": True,
            "persistent_mapping": True,
            "indirect_draw": True,
            "timer_query": True,
            "texture_array": True,
            "rgba16f": True,
            "debug_callback": True,
            "debug_output_installed": True,
            "debug_output_enabled": True,
            "meets_preflight": True,
            "preflight_reason": "ok",
        },
        "resources": {
            "total_tracked_bytes": 0,
            "peak_tracked_bytes": 0,
            "active_resource_count": 0,
            "leak_candidate_count": 0,
        },
        "stress_test": {
            "duration_seconds": 5.0,
            "stress_1min_passed": True,
            "toggle_100_loops_passed": True,
            "start_tracked_bytes": 0,
            "end_tracked_bytes": 0,
            "peak_tracked_bytes": 0,
            "leak_candidate_count": 0,
        },
        "gl_diagnostics": {
            "debug_message_count": 0,
            "severe_error_count": 0,
            "dropped_count": 0,
            "callback_installed": True,
            "callback_enabled": True,
            "messages": [],
        },
        "matrix_results": [],
        "global_failures": [],
    }


class GpuHardwareValidationGateRunnerTest(unittest.TestCase):
    def test_parses_structured_cpp_status(self) -> None:
        output = "GPU_HARDWARE_GATE_RESULT status=NO_GO\n"

        self.assertEqual(
            gpu_hardware_validation_gate.parse_cpp_gate_status(output),
            "NO_GO",
        )

    def test_ignores_doctest_success_without_cpp_status(self) -> None:
        output = "Status: SUCCESS!\n"

        self.assertIsNone(
            gpu_hardware_validation_gate.parse_cpp_gate_status(output)
        )

    def test_rejects_missing_or_ambiguous_cpp_status(self) -> None:
        self.assertIsNone(
            gpu_hardware_validation_gate.parse_cpp_gate_status("")
        )
        self.assertIsNone(
            gpu_hardware_validation_gate.parse_cpp_gate_status(
                "GPU_HARDWARE_GATE_RESULT status=GO\n"
                "GPU_HARDWARE_GATE_RESULT status=NO_GO\n"
            )
        )

    def test_only_zero_return_code_and_go_succeeds(self) -> None:
        self.assertTrue(
            gpu_hardware_validation_gate.gate_succeeded(0, "GO")
        )
        self.assertFalse(
            gpu_hardware_validation_gate.gate_succeeded(1, "GO")
        )
        self.assertFalse(
            gpu_hardware_validation_gate.gate_succeeded(0, "NO_GO")
        )
        self.assertFalse(
            gpu_hardware_validation_gate.gate_succeeded(0, "NOT_RUN")
        )

    def test_extracts_full_gate_report_json(self) -> None:
        report = _build_minimal_report("NO_GO")
        output = (
            "some doctest noise\n"
            + gpu_hardware_validation_gate.GATE_REPORT_BEGIN_MARKER
            + "\n"
            + json.dumps(report, indent=2)
            + "\n"
            + gpu_hardware_validation_gate.GATE_REPORT_END_MARKER
            + "\n"
        )

        parsed = gpu_hardware_validation_gate.extract_cpp_gate_report(output)

        self.assertIsNotNone(parsed)
        self.assertEqual(parsed["gate_status"], "NO_GO")
        self.assertEqual(parsed["revision"], "TEST_REV")
        self.assertEqual(parsed["capabilities"]["debug_callback"], True)
        self.assertIn("messages", parsed["gl_diagnostics"])

    def test_extract_returns_none_without_markers(self) -> None:
        self.assertIsNone(
            gpu_hardware_validation_gate.extract_cpp_gate_report(
                "GPU_HARDWARE_GATE_RESULT status=NO_GO\n"
            )
        )

    def test_extract_rejects_malformed_json(self) -> None:
        begin = gpu_hardware_validation_gate.GATE_REPORT_BEGIN_MARKER
        end = gpu_hardware_validation_gate.GATE_REPORT_END_MARKER
        output = f"{begin}\n{{not valid json\n{end}\n"

        self.assertIsNone(
            gpu_hardware_validation_gate.extract_cpp_gate_report(output)
        )

    def test_validate_accepts_complete_report(self) -> None:
        self.assertEqual(
            gpu_hardware_validation_gate.validate_gate_report_schema(
                _build_minimal_report("GO")
            ),
            [],
        )

    def test_validate_accepts_fail_closed_not_run_report(self) -> None:
        report = _build_minimal_report("NOT_RUN")
        report["capabilities"]["debug_callback"] = False
        report["capabilities"]["debug_output_installed"] = False
        report["capabilities"]["debug_output_enabled"] = False
        report["gl_diagnostics"]["callback_installed"] = False
        report["gl_diagnostics"]["callback_enabled"] = False

        self.assertEqual(
            gpu_hardware_validation_gate.validate_gate_report_schema(report),
            [],
        )
        self.assertFalse(
            gpu_hardware_validation_gate.gate_succeeded(0, "NOT_RUN")
        )

    def test_validate_rejects_missing_top_level_key(self) -> None:
        report = _build_minimal_report()
        del report["capabilities"]

        errors = gpu_hardware_validation_gate.validate_gate_report_schema(
            report
        )

        self.assertTrue(any("capabilities" in error for error in errors))

    def test_validate_rejects_invalid_status(self) -> None:
        report = _build_minimal_report("GOOD")

        errors = gpu_hardware_validation_gate.validate_gate_report_schema(
            report
        )

        self.assertTrue(any("gate_status" in error for error in errors))

    def test_validate_checks_diagnostic_message_schema(self) -> None:
        report = _build_minimal_report("NO_GO")
        report["gl_diagnostics"]["messages"] = [
            {
                "severity": 0x9146,
                "type": 0x824C,
                "source": 0x824A,
                "message": "synthetic severe diagnostic",
                "id": 1282,
            }
        ]

        errors = gpu_hardware_validation_gate.validate_gate_report_schema(
            report
        )

        self.assertTrue(
            any("missing required key: time" in error for error in errors)
        )

    def test_validate_accepts_structured_diagnostic_message(self) -> None:
        report = _build_minimal_report("NO_GO")
        report["gl_diagnostics"]["messages"] = [
            {
                "severity": 0x9146,
                "type": 0x824C,
                "source": 0x824A,
                "message": "synthetic severe diagnostic",
                "id": 1282,
                "time": "2026-08-01T00:00:00Z",
            }
        ]

        self.assertEqual(
            gpu_hardware_validation_gate.validate_gate_report_schema(report),
            [],
        )

    # --- S8: env-var injection (dead CLI params are wired to the C++ gate) ---

    def test_build_gate_env_forwards_all_cli_knobs(self) -> None:
        config = gpu_hardware_validation_gate.HardwareGateConfig(
            sample_frames=60,
            toggle_loops=50,
            stress_test_1min=False,
        )
        env = gpu_hardware_validation_gate.build_gate_env(config)
        self.assertEqual(env["NMD_GATE_SAMPLES"], "60")
        self.assertEqual(env["NMD_GATE_TOGGLE_LOOPS"], "50")
        self.assertEqual(env["NMD_GATE_STRESS"], "0")

        config_stress = gpu_hardware_validation_gate.HardwareGateConfig(
            sample_frames=120,
            toggle_loops=100,
            stress_test_1min=True,
        )
        env_stress = gpu_hardware_validation_gate.build_gate_env(config_stress)
        self.assertEqual(env_stress["NMD_GATE_SAMPLES"], "120")
        self.assertEqual(env_stress["NMD_GATE_TOGGLE_LOOPS"], "100")
        self.assertEqual(env_stress["NMD_GATE_STRESS"], "1")

    def test_timeout_linked_to_stress_duration(self) -> None:
        base = gpu_hardware_validation_gate.GATE_BASE_TIMEOUT_SECONDS
        added = gpu_hardware_validation_gate.GATE_STRESS_ADDED_SECONDS
        config = gpu_hardware_validation_gate.HardwareGateConfig(
            stress_test_1min=True
        )
        self.assertEqual(
            gpu_hardware_validation_gate.gate_timeout_seconds(config),
            base + added,
        )
        config_short = gpu_hardware_validation_gate.HardwareGateConfig(
            stress_test_1min=False
        )
        self.assertEqual(
            gpu_hardware_validation_gate.gate_timeout_seconds(config_short),
            base,
        )

    def test_run_hardware_gate_cpp_injects_env_and_timeout(self) -> None:
        config = gpu_hardware_validation_gate.HardwareGateConfig(
            revision="TEST_REV",
            sample_frames=60,
            toggle_loops=50,
            stress_test_1min=False,
        )
        sent: dict[str, Any] = {}

        def fake_run(cmd, capture_output, text, env, timeout, check):
            sent["env"] = env
            sent["timeout"] = timeout

            class FakeProc:
                returncode = 0
                stdout = ""
                stderr = ""

            return FakeProc()

        with unittest.mock.patch(
            "gpu_hardware_validation_gate.subprocess.run",
            side_effect=fake_run,
        ), tempfile.TemporaryDirectory() as tmp:
            exe = Path(tmp) / "NoMoreDayTests.exe"
            exe.write_text("fake", encoding="utf-8")
            config.test_exe = exe
            rc, _, _ = gpu_hardware_validation_gate.run_hardware_gate_cpp(config)

        self.assertEqual(rc, 0)
        self.assertIn("NMD_GATE_SAMPLES", sent["env"])
        self.assertEqual(sent["env"]["NMD_GATE_SAMPLES"], "60")
        self.assertEqual(sent["env"]["NMD_GATE_TOGGLE_LOOPS"], "50")
        self.assertEqual(sent["env"]["NMD_GATE_STRESS"], "0")
        self.assertEqual(
            sent["timeout"],
            gpu_hardware_validation_gate.GATE_BASE_TIMEOUT_SECONDS,
        )

    # --- S8: waiver metadata ---

    def test_build_waiver_returns_none_without_fields(self) -> None:
        self.assertIsNone(gpu_hardware_validation_gate.build_waiver())
        self.assertIsNone(
            gpu_hardware_validation_gate.build_waiver(
                authorizer="",
                reason="",
                scope="",
                expiry="",
            )
        )

    def test_build_waiver_writes_metadata_fields(self) -> None:
        waiver = gpu_hardware_validation_gate.build_waiver(
            authorizer="render-lead",
            reason="WARP-only CI runner, no discrete GPU",
            scope="nmd.tests.integration GPU Hardware Validation Gate",
            expiry="2026-09-01",
        )
        self.assertEqual(waiver["authorizer"], "render-lead")
        self.assertEqual(waiver["reason"], "WARP-only CI runner, no discrete GPU")
        self.assertEqual(
            waiver["scope"], "nmd.tests.integration GPU Hardware Validation Gate"
        )
        self.assertEqual(waiver["expiry"], "2026-09-01")

    def test_waiver_does_not_change_go_determination(self) -> None:
        waiver = gpu_hardware_validation_gate.build_waiver(
            authorizer="render-lead",
            reason="temporary",
            scope="gate",
            expiry="2026-09-01",
        )
        self.assertIsNotNone(waiver)
        # NOT_RUN / NO_GO with a waiver present must still NOT pass as GO.
        self.assertFalse(gpu_hardware_validation_gate.gate_succeeded(0, "NOT_RUN"))
        self.assertFalse(gpu_hardware_validation_gate.gate_succeeded(0, "NO_GO"))
        # Even with waiver metadata, GO semantics stay return_code==0 AND "GO".
        self.assertTrue(gpu_hardware_validation_gate.gate_succeeded(0, "GO"))

    def test_waiver_written_into_artifact_but_verdict_unchanged(self) -> None:
        report = _build_minimal_report("NO_GO")
        output = (
            gpu_hardware_validation_gate.GATE_REPORT_BEGIN_MARKER
            + "\n"
            + json.dumps(report)
            + "\n"
            + gpu_hardware_validation_gate.GATE_REPORT_END_MARKER
            + "\n"
        )

        def fake_run(cmd, capture_output, text, env, timeout, check):
            class FakeProc:
                returncode = 0
                stdout = output
                stderr = ""

            return FakeProc()

        with unittest.mock.patch(
            "gpu_hardware_validation_gate.subprocess.run",
            side_effect=fake_run,
        ), tempfile.TemporaryDirectory() as tmp:
            exe = Path(tmp) / "NoMoreDayTests.exe"
            exe.write_text("fake", encoding="utf-8")
            out_dir = Path(tmp) / "out"
            argv = [
                "gpu_hardware_validation_gate.py",
                "--test-exe",
                str(exe),
                "--revision",
                "TEST_REV_WAIVER",
                "--output-dir",
                str(out_dir),
                "--waiver-authorizer",
                "render-lead",
                "--waiver-reason",
                "WARP-only",
                "--waiver-scope",
                "gate",
                "--waiver-expiry",
                "2026-09-01",
            ]
            with unittest.mock.patch.object(sys, "argv", argv):
                exit_code = gpu_hardware_validation_gate.main()
            artifact_path = out_dir / "gpu_hardware_validation_artifact.json"
            self.assertTrue(artifact_path.exists())
            artifact = json.loads(artifact_path.read_text(encoding="utf-8"))
            self.assertEqual(artifact["gate_status"], "NO_GO")
            self.assertFalse(artifact["gate_succeeded"])
            self.assertFalse(artifact["meets_preflight"])
            self.assertEqual(artifact["waiver"]["authorizer"], "render-lead")
            self.assertEqual(artifact["waiver"]["reason"], "WARP-only")

        self.assertEqual(exit_code, 1)

    # --- S8: archive path ---

    def test_default_output_dir_is_artifacts_gpu_gate_revision(self) -> None:
        report = _build_minimal_report("GO")
        output = (
            gpu_hardware_validation_gate.GATE_REPORT_BEGIN_MARKER
            + "\n"
            + json.dumps(report)
            + "\n"
            + gpu_hardware_validation_gate.GATE_REPORT_END_MARKER
            + "\n"
        )

        def fake_run(cmd, capture_output, text, env, timeout, check):
            class FakeProc:
                returncode = 0
                stdout = output
                stderr = ""

            return FakeProc()

        with unittest.mock.patch(
            "gpu_hardware_validation_gate.subprocess.run",
            side_effect=fake_run,
        ), tempfile.TemporaryDirectory() as tmp:
            original_cwd = Path.cwd()
            try:
                os.chdir(tmp)
                fake_exe = Path(tmp) / "NoMoreDayTests.exe"
                fake_exe.write_text("fake", encoding="utf-8")
                argv = [
                    "gpu_hardware_validation_gate.py",
                    "--revision",
                    "abcdef1234",
                    "--test-exe",
                    str(fake_exe),
                ]
                with unittest.mock.patch.object(sys, "argv", argv):
                    exit_code = gpu_hardware_validation_gate.main()
            finally:
                os.chdir(original_cwd)

            artifact_path = (
                Path(tmp)
                / "artifacts"
                / "gpu-gate"
                / "abcdef1234"
                / "gpu_hardware_validation_artifact.json"
            )
            self.assertTrue(artifact_path.exists())

        self.assertEqual(exit_code, 0)

    def test_validate_archived_artifact_accepts_valid_archive(self) -> None:
        report = _build_minimal_report("NO_GO")
        artifact = {
            "gate_status": "NO_GO",
            "gate_succeeded": False,
            "gate_report": report,
        }
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "artifact.json"
            path.write_text(json.dumps(artifact), encoding="utf-8")
            exit_code = gpu_hardware_validation_gate.validate_archived_artifact(
                path
            )

        self.assertEqual(exit_code, 0)

    def test_validate_archived_artifact_rejects_missing_report(self) -> None:
        artifact = {"gate_status": "GO", "gate_succeeded": True}
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "artifact.json"
            path.write_text(json.dumps(artifact), encoding="utf-8")
            exit_code = gpu_hardware_validation_gate.validate_archived_artifact(
                path
            )

        self.assertEqual(exit_code, 1)

    def test_validate_archived_artifact_rejects_broken_report(self) -> None:
        report = _build_minimal_report("GO")
        del report["capabilities"]
        artifact = {
            "gate_status": "GO",
            "gate_succeeded": True,
            "gate_report": report,
        }
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "artifact.json"
            path.write_text(json.dumps(artifact), encoding="utf-8")
            exit_code = gpu_hardware_validation_gate.validate_archived_artifact(
                path
            )

        self.assertEqual(exit_code, 1)


if __name__ == "__main__":
    unittest.main()
