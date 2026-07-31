import json
import sys
import unittest
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


if __name__ == "__main__":
    unittest.main()
