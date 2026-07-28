import sys
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SCRIPTS_DIR = REPO_ROOT / "scripts"
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))


import gpu_hardware_validation_gate  # noqa: E402


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


if __name__ == "__main__":
    unittest.main()
