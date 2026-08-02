import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
CHECKER = REPO_ROOT / "scripts" / "check_module_boundaries.py"


def make_fixture_ledger() -> dict[str, object]:
    return {
        "schema_version": "1.0",
        "scope": {
            "candidate_roots": [
                {
                    "path": "src/engine",
                    "candidate_target": "NoMoreDayEngine",
                    "candidate_layer": "Engine",
                },
                {
                    "path": "src/core",
                    "candidate_target": "NoMoreDayCore",
                    "candidate_layer": "Core",
                },
            ],
            "pch_files": ["src/pch.hpp"],
            "forbidden_include_prefixes": ["game/", "app/"],
        },
        "entries": [],
    }


def make_fixture_entry(
    source: str = "src/engine/Synthetic.cpp",
    line: int = 1,
    include_path: str = "game/components/Common.hpp",
) -> dict[str, object]:
    return {
        "id": f"{source}:{line}:{include_path}",
        "source": source,
        "line": line,
        "include_path": include_path,
        "candidate_target": "NoMoreDayEngine",
        "candidate_layer": "Engine",
        "current_owner": "engine_layer",
        "future_owner_layer": "Game",
        "disposition": "move_to_game",
        "milestone": "MS-3",
        "p0_blocking": None,
    }


class ModuleBoundaryCheckerTest(unittest.TestCase):
    def run_checker(self, root: Path) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [sys.executable, str(CHECKER), "--repo-root", str(root)],
            check=False,
            capture_output=True,
            text=True,
        )

    def test_repository_baseline_passes(self) -> None:
        result = self.run_checker(REPO_ROOT)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("PASS", result.stdout)

    def test_untracked_reverse_edge_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            (root / "src" / "engine").mkdir(parents=True)
            (root / "src" / "core").mkdir(parents=True)
            (root / "src" / "pch.hpp").write_text("#pragma once\n", encoding="utf-8")
            (root / "src" / "engine" / "Synthetic.cpp").write_text(
                '#include "game/components/Common.hpp"\n', encoding="utf-8"
            )
            ledger_path = (
                root
                / "docs"
                / "reports"
                / "modular-split-exe-lib-dll"
                / "ms-0"
                / "reverse-dependency-ledger.json"
            )
            ledger_path.parent.mkdir(parents=True)
            ledger_path.write_text(
                json.dumps(make_fixture_ledger()), encoding="utf-8"
            )
            result = self.run_checker(root)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn("untracked reverse dependency", result.stdout)

    def test_stale_ledger_entry_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            (root / "src" / "engine").mkdir(parents=True)
            (root / "src" / "core").mkdir(parents=True)
            (root / "src" / "pch.hpp").write_text(
                "#pragma once\n", encoding="utf-8"
            )
            ledger = make_fixture_ledger()
            ledger["entries"] = [
                make_fixture_entry(
                    source="src/engine/Absent.cpp",
                    include_path="game/Absent.hpp",
                )
            ]
            ledger_path = (
                root / "docs" / "reports" / "modular-split-exe-lib-dll"
                / "ms-0" / "reverse-dependency-ledger.json"
            )
            ledger_path.parent.mkdir(parents=True)
            ledger_path.write_text(json.dumps(ledger), encoding="utf-8")
            result = self.run_checker(root)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn("stale ledger entry", result.stdout)

    def test_malformed_ledger_returns_input_error(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            (root / "src" / "engine").mkdir(parents=True)
            (root / "src" / "core").mkdir(parents=True)
            (root / "src" / "pch.hpp").write_text(
                "#pragma once\n", encoding="utf-8"
            )
            ledger_path = (
                root / "docs" / "reports" / "modular-split-exe-lib-dll"
                / "ms-0" / "reverse-dependency-ledger.json"
            )
            ledger_path.parent.mkdir(parents=True)
            ledger_path.write_text("{}\n", encoding="utf-8")
            result = self.run_checker(root)
            self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
            self.assertIn("ERROR", result.stdout)

    def test_empty_required_p0_sources_tolerated_after_ms6(self) -> None:
        # REQUIRED_P0_SOURCES is empty once every MS-6 P0 source has been
        # migrated (Batch E removed the last one). A tracked entry on the
        # former P0 source without p0_blocking must be accepted: with an empty
        # frozenset `source in REQUIRED_P0_SOURCES` is always False, so the
        # "source requires P0_BLOCKER" assertion never fires and the ledger
        # entry passes validation.
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            (root / "src" / "engine" / "render" / "passes").mkdir(parents=True)
            (root / "src" / "core").mkdir(parents=True)
            (root / "src" / "pch.hpp").write_text(
                "#pragma once\n", encoding="utf-8"
            )
            source = "src/engine/render/passes/RadianceCascadesPass.cpp"
            (root / source).write_text(
                '#include "game/components/Common.hpp"\n', encoding="utf-8"
            )
            ledger = make_fixture_ledger()
            ledger["entries"] = [make_fixture_entry(source=source)]
            ledger_path = (
                root / "docs" / "reports" / "modular-split-exe-lib-dll"
                / "ms-0" / "reverse-dependency-ledger.json"
            )
            ledger_path.parent.mkdir(parents=True)
            ledger_path.write_text(json.dumps(ledger), encoding="utf-8")
            result = self.run_checker(root)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("PASS", result.stdout)

    def test_invalid_ownership_and_p0_metadata_return_input_error(self) -> None:
        cases = (
            {"future_owner_layer": "Engine"},
            {"candidate_target": "NoMoreDayCore"},
            {"p0_blocking": "removed_p0_blocker"},
            {
                "p0_blocking": "gpu_rendergraph_resource_foundation_20260726",
                "disposition": "move_to_game",
            },
            {
                "p0_blocking": "gpu_rendergraph_resource_foundation_20260726",
                "milestone": "MS-3",
            },
        )
        for updates in cases:
            with (
                self.subTest(updates=updates),
                tempfile.TemporaryDirectory() as temp_dir,
            ):
                root = Path(temp_dir)
                (root / "src" / "engine").mkdir(parents=True)
                (root / "src" / "core").mkdir(parents=True)
                (root / "src" / "pch.hpp").write_text(
                    "#pragma once\n", encoding="utf-8"
                )
                (root / "src" / "engine" / "Synthetic.cpp").write_text(
                    '#include "game/components/Common.hpp"\n', encoding="utf-8"
                )
                ledger = make_fixture_ledger()
                entry = make_fixture_entry()
                entry.update(updates)
                ledger["entries"] = [entry]
                ledger_path = (
                    root / "docs" / "reports" / "modular-split-exe-lib-dll"
                    / "ms-0" / "reverse-dependency-ledger.json"
                )
                ledger_path.parent.mkdir(parents=True)
                ledger_path.write_text(json.dumps(ledger), encoding="utf-8")
                result = self.run_checker(root)
                self.assertEqual(
                    result.returncode, 2, result.stdout + result.stderr
                )
                self.assertIn("ERROR", result.stdout)


if __name__ == "__main__":
    unittest.main()
