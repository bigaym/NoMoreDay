import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
CHECKER = REPO_ROOT / "scripts" / "check_module_boundaries.py"
LEDGER_PATH = Path(
    "docs/reports/modular-split-exe-lib-dll/ms-0/reverse-dependency-ledger.json"
)


def make_fixture_ledger() -> dict[str, object]:
    return {
        "schema_version": "2.0",
        "scope": {
            "candidate_roots": [
                {
                    "path": "src/core",
                    "candidate_target": "NoMoreDayCore",
                    "candidate_layer": "Core",
                    "forbidden_include_prefixes": ["engine/", "game/", "app/"],
                },
                {
                    "path": "src/engine",
                    "candidate_target": "NoMoreDayEngine",
                    "candidate_layer": "Engine",
                    "forbidden_include_prefixes": ["game/", "app/"],
                },
                {
                    "path": "src/game",
                    "candidate_target": "NoMoreDayGame",
                    "candidate_layer": "Game",
                    "forbidden_include_prefixes": ["app/"],
                },
            ],
            "pch_files": [
                {
                    "path": "src/pch.hpp",
                    "candidate_target": "EngineOwnedPch",
                    "candidate_layer": "Engine-owned PCH",
                    "forbidden_include_prefixes": ["game/", "app/"],
                },
            ],
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
        "forbidden_include_prefixes": ["game/", "app/"],
        "future_owner_layer": "Game",
        "disposition": "move_to_game",
        "milestone": "MS-3",
    }


def make_game_to_app_entry(
    source: str = "src/game/Synthetic.cpp",
    line: int = 1,
    include_path: str = "app/ui/MainMenu.hpp",
) -> dict[str, object]:
    return {
        "id": f"{source}:{line}:{include_path}",
        "source": source,
        "line": line,
        "include_path": include_path,
        "candidate_target": "NoMoreDayGame",
        "candidate_layer": "Game",
        "current_owner": "game_layer",
        "forbidden_include_prefixes": ["app/"],
        "future_owner_layer": "App",
        "disposition": "move_to_app",
        "milestone": "MS-8",
    }


def make_pch_entry(
    source: str = "src/pch.hpp",
    line: int = 1,
    include_path: str = "game/components/Common.hpp",
) -> dict[str, object]:
    return {
        "id": f"{source}:{line}:{include_path}",
        "source": source,
        "line": line,
        "include_path": include_path,
        "candidate_target": "EngineOwnedPch",
        "candidate_layer": "Engine-owned PCH",
        "current_owner": "engine_owned_pch",
        "forbidden_include_prefixes": ["game/", "app/"],
        "future_owner_layer": "Game",
        "disposition": "move_to_game",
        "milestone": "MS-8",
    }


def _scaffold_repo(root: Path) -> None:
    (root / "src" / "core").mkdir(parents=True)
    (root / "src" / "engine").mkdir(parents=True)
    (root / "src" / "game").mkdir(parents=True)
    (root / "src" / "pch.hpp").write_text("#pragma once\n", encoding="utf-8")


def _write_ledger(root: Path, ledger: dict[str, object]) -> Path:
    ledger_path = root / LEDGER_PATH
    ledger_path.parent.mkdir(parents=True)
    ledger_path.write_text(json.dumps(ledger), encoding="utf-8")
    return ledger_path


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

    def test_game_self_include_passes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            _scaffold_repo(root)
            (root / "src" / "game" / "Synthetic.cpp").write_text(
                '#include "game/components/Common.hpp"\n'
                "#include <game/systems/SkillSystem.hpp>\n",
                encoding="utf-8",
            )
            _write_ledger(root, make_fixture_ledger())
            result = self.run_checker(root)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("PASS", result.stdout)

    def test_game_unledgered_app_quote_include_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            _scaffold_repo(root)
            (root / "src" / "game" / "Synthetic.cpp").write_text(
                '#include "app/ui/MainMenu.hpp"\n', encoding="utf-8"
            )
            _write_ledger(root, make_fixture_ledger())
            result = self.run_checker(root)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn("untracked reverse dependency", result.stdout)
            self.assertIn("app/ui/MainMenu.hpp", result.stdout)

    def test_game_unledgered_app_angle_include_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            _scaffold_repo(root)
            (root / "src" / "game" / "Synthetic.cpp").write_text(
                "#include <app/ui/MainMenu.hpp>\n", encoding="utf-8"
            )
            _write_ledger(root, make_fixture_ledger())
            result = self.run_checker(root)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn("untracked reverse dependency", result.stdout)
            self.assertIn("app/ui/MainMenu.hpp", result.stdout)

    def test_game_ledgered_app_include_passes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            _scaffold_repo(root)
            (root / "src" / "game" / "Synthetic.cpp").write_text(
                '#include "app/ui/MainMenu.hpp"\n', encoding="utf-8"
            )
            ledger = make_fixture_ledger()
            ledger["entries"] = [make_game_to_app_entry()]
            _write_ledger(root, ledger)
            result = self.run_checker(root)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("PASS", result.stdout)

    def test_external_system_headers_not_flagged(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            _scaffold_repo(root)
            (root / "src" / "game" / "SystemHeaders.cpp").write_text(
                "#include <algorithm>\n"
                "#include <entt/entity/registry.hpp>\n"
                '#include "Common.hpp"\n'
                '#include "../data/MapAffix.hpp"\n',
                encoding="utf-8",
            )
            _write_ledger(root, make_fixture_ledger())
            result = self.run_checker(root)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("PASS", result.stdout)

    def test_core_rejects_engine_game_app_includes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            _scaffold_repo(root)
            (root / "src" / "core" / "Synthetic.cpp").write_text(
                '#include "engine/render/RenderSystem.hpp"\n'
                '#include "game/components/Common.hpp"\n'
                '#include "app/ui/MainMenu.hpp"\n',
                encoding="utf-8",
            )
            _write_ledger(root, make_fixture_ledger())
            result = self.run_checker(root)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertEqual(result.stdout.count("untracked reverse dependency"), 3)

    def test_engine_rejects_game_app_includes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            _scaffold_repo(root)
            (root / "src" / "engine" / "Synthetic.cpp").write_text(
                '#include "game/components/Common.hpp"\n'
                '#include "app/ui/MainMenu.hpp"\n',
                encoding="utf-8",
            )
            _write_ledger(root, make_fixture_ledger())
            result = self.run_checker(root)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertEqual(result.stdout.count("untracked reverse dependency"), 2)

    def test_engine_self_and_core_includes_pass(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            _scaffold_repo(root)
            (root / "src" / "engine" / "Ok.cpp").write_text(
                '#include "core/utils/HashUtils.hpp"\n'
                "#include <engine/render/RenderSystem.hpp>\n",
                encoding="utf-8",
            )
            _write_ledger(root, make_fixture_ledger())
            result = self.run_checker(root)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("PASS", result.stdout)

    def test_stale_ledger_entry_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            _scaffold_repo(root)
            ledger = make_fixture_ledger()
            ledger["entries"] = [
                make_fixture_entry(
                    source="src/engine/Absent.cpp",
                    include_path="game/Absent.hpp",
                )
            ]
            _write_ledger(root, ledger)
            result = self.run_checker(root)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn("stale ledger entry", result.stdout)

    def test_malformed_ledger_returns_input_error(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            _scaffold_repo(root)
            _write_ledger(root, {})
            result = self.run_checker(root)
            self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
            self.assertIn("ERROR", result.stdout)

    def test_non_string_disposition_and_milestone_return_input_error(self) -> None:
        for field in ("disposition", "milestone"):
            with self.subTest(field=field), tempfile.TemporaryDirectory() as temporary_directory:
                root = Path(temporary_directory)
                _scaffold_repo(root)
                ledger = make_fixture_ledger()
                entry = make_fixture_entry()
                entry[field] = []
                ledger["entries"] = [entry]
                _write_ledger(root, ledger)
                result = self.run_checker(root)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertIn("ERROR", result.stdout)

    def test_pch_unledgered_game_quote_include_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            _scaffold_repo(root)
            (root / "src" / "pch.hpp").write_text(
                '#include "game/components/Common.hpp"\n', encoding="utf-8"
            )
            _write_ledger(root, make_fixture_ledger())
            result = self.run_checker(root)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn("untracked reverse dependency", result.stdout)
            self.assertIn("src/pch.hpp:1", result.stdout)
            self.assertIn("game/components/Common.hpp", result.stdout)

    def test_pch_unledgered_game_angle_include_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            _scaffold_repo(root)
            (root / "src" / "pch.hpp").write_text(
                "#include <game/components/Common.hpp>\n", encoding="utf-8"
            )
            _write_ledger(root, make_fixture_ledger())
            result = self.run_checker(root)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn("untracked reverse dependency", result.stdout)
            self.assertIn("src/pch.hpp:1", result.stdout)
            self.assertIn("game/components/Common.hpp", result.stdout)

    def test_pch_unledgered_app_quote_include_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            _scaffold_repo(root)
            (root / "src" / "pch.hpp").write_text(
                '#include "app/ui/MainMenu.hpp"\n', encoding="utf-8"
            )
            _write_ledger(root, make_fixture_ledger())
            result = self.run_checker(root)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn("untracked reverse dependency", result.stdout)
            self.assertIn("src/pch.hpp:1", result.stdout)
            self.assertIn("app/ui/MainMenu.hpp", result.stdout)

    def test_pch_unledgered_app_angle_include_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            _scaffold_repo(root)
            (root / "src" / "pch.hpp").write_text(
                "#include <app/ui/MainMenu.hpp>\n", encoding="utf-8"
            )
            _write_ledger(root, make_fixture_ledger())
            result = self.run_checker(root)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn("untracked reverse dependency", result.stdout)
            self.assertIn("src/pch.hpp:1", result.stdout)
            self.assertIn("app/ui/MainMenu.hpp", result.stdout)

    def test_pch_ledgered_include_passes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            _scaffold_repo(root)
            (root / "src" / "pch.hpp").write_text(
                '#include "game/components/Common.hpp"\n'
                "#include <app/ui/MainMenu.hpp>\n",
                encoding="utf-8",
            )
            ledger = make_fixture_ledger()
            ledger["entries"] = [
                make_pch_entry(),
                make_pch_entry(line=2, include_path="app/ui/MainMenu.hpp"),
            ]
            _write_ledger(root, ledger)
            result = self.run_checker(root)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("PASS", result.stdout)

    def test_preprocessor_whitespace_includes_flagged(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            _scaffold_repo(root)
            (root / "src" / "game" / "Synthetic.cpp").write_text(
                "# include <app/ui/MainMenu.hpp>\n"
                '#\tinclude\t"app/ui/MainMenu.hpp"\n',
                encoding="utf-8",
            )
            _write_ledger(root, make_fixture_ledger())
            result = self.run_checker(root)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertEqual(result.stdout.count("untracked reverse dependency"), 2)
            self.assertEqual(result.stdout.count("app/ui/MainMenu.hpp"), 2)

    def test_windows_path_casefold_flagged_but_raw_evidence_preserved(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            _scaffold_repo(root)
            (root / "src" / "game" / "Synthetic.cpp").write_text(
                '#include "App/UI/MainMenu.hpp"\n', encoding="utf-8"
            )
            _write_ledger(root, make_fixture_ledger())
            result = self.run_checker(root)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn("untracked reverse dependency", result.stdout)
            self.assertIn("App/UI/MainMenu.hpp", result.stdout)

    def test_windows_path_casefold_matches_ledger_raw_entry(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            _scaffold_repo(root)
            (root / "src" / "game" / "Synthetic.cpp").write_text(
                '#include "App/UI/MainMenu.hpp"\n', encoding="utf-8"
            )
            ledger = make_fixture_ledger()
            ledger["entries"] = [
                make_game_to_app_entry(include_path="App/UI/MainMenu.hpp")
            ]
            _write_ledger(root, ledger)
            result = self.run_checker(root)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("PASS", result.stdout)

    def test_scope_policy_metadata_fails_closed(self) -> None:
        cases = (
            "missing_candidate_roots",
            "wrong_game_forbidden_prefixes",
            "missing_candidate_layer",
            "legacy_pch_files_string_form",
            "legacy_global_forbidden_include_prefixes",
            "duplicate_candidate_root",
        )
        for case in cases:
            with (
                self.subTest(case=case),
                tempfile.TemporaryDirectory() as temp_dir,
            ):
                root = Path(temp_dir)
                _scaffold_repo(root)
                ledger = make_fixture_ledger()
                scope = ledger["scope"]
                if case == "missing_candidate_roots":
                    del scope["candidate_roots"]
                elif case == "wrong_game_forbidden_prefixes":
                    scope["candidate_roots"][2][
                        "forbidden_include_prefixes"
                    ] = ["app/", "game/"]
                elif case == "missing_candidate_layer":
                    del scope["candidate_roots"][0]["candidate_layer"]
                elif case == "legacy_pch_files_string_form":
                    scope["pch_files"] = ["src/pch.hpp"]
                elif case == "legacy_global_forbidden_include_prefixes":
                    scope["forbidden_include_prefixes"] = ["game/", "app/"]
                elif case == "duplicate_candidate_root":
                    scope["candidate_roots"].append(
                        dict(scope["candidate_roots"][0])
                    )
                _write_ledger(root, ledger)
                result = self.run_checker(root)
                self.assertEqual(
                    result.returncode, 2, result.stdout + result.stderr
                )
                self.assertIn("ERROR", result.stdout)

    def test_legacy_p0_blocking_entry_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            _scaffold_repo(root)
            (root / "src" / "engine" / "Synthetic.cpp").write_text(
                '#include "game/components/Common.hpp"\n', encoding="utf-8"
            )
            ledger = make_fixture_ledger()
            entry = make_fixture_entry()
            entry["p0_blocking"] = None
            ledger["entries"] = [entry]
            _write_ledger(root, ledger)
            result = self.run_checker(root)
            self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
            self.assertIn("extra", result.stdout)

    def test_invalid_entry_metadata_returns_input_error(self) -> None:
        cases = (
            {"candidate_target": "NoMoreDayCore"},
            {"forbidden_include_prefixes": ["app/"]},
            {"future_owner_layer": "Engine"},
            {"disposition": "not_a_real_disposition"},
            {"milestone": "MS-99"},
            {"line": 0},
            {"line": True},
            {"id": "bogus"},
            "duplicate_entry",
        )
        for updates in cases:
            with (
                self.subTest(updates=updates),
                tempfile.TemporaryDirectory() as temp_dir,
            ):
                root = Path(temp_dir)
                _scaffold_repo(root)
                (root / "src" / "engine" / "Synthetic.cpp").write_text(
                    '#include "game/components/Common.hpp"\n', encoding="utf-8"
                )
                ledger = make_fixture_ledger()
                if updates == "duplicate_entry":
                    ledger["entries"] = [make_fixture_entry(), make_fixture_entry()]
                else:
                    entry = make_fixture_entry()
                    entry.update(updates)
                    ledger["entries"] = [entry]
                _write_ledger(root, ledger)
                result = self.run_checker(root)
                self.assertEqual(
                    result.returncode, 2, result.stdout + result.stderr
                )
                self.assertIn("ERROR", result.stdout)


if __name__ == "__main__":
    unittest.main()
