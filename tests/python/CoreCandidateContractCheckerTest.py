import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
CHECKER = REPO_ROOT / "scripts" / "check_core_candidate_contract.py"
CONTRACT_PATH = (
    "docs/reports/modular-split-exe-lib-dll/ms-1/core-candidate-contract.json"
)


def make_contract() -> dict[str, object]:
    return {
        "schema_version": "1.1",
        "milestone": "MS-7",
        "status": "implementation_complete_review_pending",
        "layered_targets": {
            "targets": [
                {
                    "name": "NoMoreDayTypes",
                    "kind": "INTERFACE",
                    "file": "CMakeLists.txt",
                },
                {
                    "name": "NoMoreDayCore",
                    "kind": "STATIC",
                    "file": "src/core/CMakeLists.txt",
                },
                {
                    "name": "NoMoreDayEngine",
                    "kind": "STATIC",
                    "file": "src/engine/CMakeLists.txt",
                },
                {
                    "name": "NoMoreDayGame",
                    "kind": "STATIC",
                    "file": "src/game/CMakeLists.txt",
                },
                {
                    "name": "NoMoreDayApp",
                    "kind": "STATIC",
                    "file": "src/app/CMakeLists.txt",
                },
            ],
            "link_chain": [
                {
                    "from": "NoMoreDayApp",
                    "to": "NoMoreDayGame",
                    "scope": "PUBLIC",
                    "file": "src/app/CMakeLists.txt",
                },
                {
                    "from": "NoMoreDayGame",
                    "to": "NoMoreDayEngine",
                    "scope": "PUBLIC",
                    "file": "src/game/CMakeLists.txt",
                },
                {
                    "from": "NoMoreDayEngine",
                    "to": "NoMoreDayCore",
                    "scope": "PUBLIC",
                    "file": "src/engine/CMakeLists.txt",
                },
                {
                    "from": "NoMoreDayCore",
                    "to": "NoMoreDayTypes",
                    "scope": "PUBLIC",
                    "file": "src/core/CMakeLists.txt",
                },
            ],
        },
        "types_target": {
            "name": "NoMoreDayTypes",
            "kind": "INTERFACE",
            "public_include_root": "src",
            "sources": [],
            "precompiled_headers": [],
            "link_dependencies": [],
            "admission_requirements": ["Cross-target value type."],
            "explicit_exclusions": ["Gameplay components."],
        },
        "future_core_candidate": {
            "name": "NoMoreDayCore",
            "role": "Contract only.",
            "eligible_implementation_sources": ["src/core/Logger.cpp"],
            "eligible_headers": ["src/core/Logger.hpp"],
            "deferred_items": [{
                "path": "src/core/Deferred.hpp",
                "reason": "Deferred.",
                "owner_resolution": "Later audit.",
            }],
            "dependency_rule": "No Engine, Game, or App dependencies.",
        },
        "pch_inventory": {
            "NoMoreDayTypes": {"approved_pch": None, "rule": "No PCH."},
            "future_NoMoreDayCore": {
                "approved_pch": None,
                "rule": "No PCH initially.",
            },
            "current_aggregate_NoMoreDayCore": {
                "path": "src/pch.hpp",
                "status": "aggregate-only",
                "direct_game_includes": ["game/Common.hpp"],
                "direct_engine_includes": ["engine/Resource.hpp"],
            },
        },
        "audit_scope": "Direct includes and current CMake ownership only.",
    }


class CoreCandidateContractCheckerTest(unittest.TestCase):
    def run_checker(self, root: Path) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [sys.executable, str(CHECKER), "--repo-root", str(root)],
            check=False,
            capture_output=True,
            text=True,
        )

    def run_cmake(self, root: Path) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            ["cmake", "-S", str(root), "-B", str(root / "build")],
            check=False,
            capture_output=True,
            text=True,
        )

    def run_cmake_with_contract_check(
        self, root: Path
    ) -> subprocess.CompletedProcess[str]:
        preflight = root / "contract_preflight.cmake"
        preflight.write_text(
            "execute_process(\n"
            f"  COMMAND \"{Path(sys.executable).as_posix()}\" "
            f"\"{CHECKER.as_posix()}\"\n"
            "          --repo-root \"${CMAKE_SOURCE_DIR}\"\n"
            "  RESULT_VARIABLE contract_result\n"
            "  OUTPUT_VARIABLE contract_output\n"
            "  ERROR_VARIABLE contract_error\n"
            ")\n"
            "if(NOT contract_result EQUAL 0)\n"
            "  message(FATAL_ERROR \"Core candidate contract rejected:\\n"
            "${contract_output}${contract_error}\")\n"
            "endif()\n",
            encoding="utf-8",
        )
        return subprocess.run(
            [
                "cmake", "-S", str(root), "-B", str(root / "build"),
                f"-DCMAKE_PROJECT_TOP_LEVEL_INCLUDES={preflight}",
            ],
            check=False,
            capture_output=True,
            text=True,
        )

    def make_fixture(self) -> tuple[tempfile.TemporaryDirectory[str], Path, Path]:
        temporary_directory = tempfile.TemporaryDirectory()
        root = Path(temporary_directory.name)
        (root / "src" / "core").mkdir(parents=True)
        (root / "src" / "core" / "Logger.cpp").write_text(
            '#include "core/Logger.hpp"\n', encoding="utf-8"
        )
        (root / "src" / "core" / "Logger.hpp").write_text(
            "#pragma once\n", encoding="utf-8"
        )
        (root / "src" / "core" / "Deferred.hpp").write_text(
            "#pragma once\n", encoding="utf-8"
        )
        (root / "src" / "core" / "CMakeLists.txt").write_text(
            "add_library(NoMoreDayCore STATIC Logger.cpp)\n"
            "target_link_libraries(NoMoreDayCore PUBLIC NoMoreDayTypes)\n",
            encoding="utf-8",
        )
        for layer_dir, layer_target, layer_source in (
            ("engine", "NoMoreDayEngine", "Engine.cpp"),
            ("game", "NoMoreDayGame", "GameSystem.cpp"),
            ("app", "NoMoreDayApp", "Game.cpp"),
        ):
            layer_path = root / "src" / layer_dir
            layer_path.mkdir(parents=True)
            (layer_path / layer_source).write_text("// fixture source\n", encoding="utf-8")
            (layer_path / "CMakeLists.txt").write_text(
                f"add_library({layer_target} STATIC {layer_source})\n",
                encoding="utf-8",
            )
        (root / "src" / "app" / "CMakeLists.txt").write_text(
            "add_library(NoMoreDayApp STATIC Game.cpp)\n"
            "target_link_libraries(NoMoreDayApp PUBLIC NoMoreDayGame)\n",
            encoding="utf-8",
        )
        (root / "src" / "game" / "CMakeLists.txt").write_text(
            "add_library(NoMoreDayGame STATIC GameSystem.cpp)\n"
            "target_link_libraries(NoMoreDayGame PUBLIC NoMoreDayEngine)\n",
            encoding="utf-8",
        )
        (root / "src" / "engine" / "CMakeLists.txt").write_text(
            "add_library(NoMoreDayEngine STATIC Engine.cpp)\n"
            "target_link_libraries(NoMoreDayEngine PUBLIC NoMoreDayCore)\n",
            encoding="utf-8",
        )
        (root / "src" / "pch.hpp").write_text(
            '#include "game/Common.hpp"\n#include "engine/Resource.hpp"\n',
            encoding="utf-8",
        )
        (root / "tests").mkdir()
        (root / "tests" / "CMakeLists.txt").write_text(
            "# fixture child\n", encoding="utf-8"
        )
        (root / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.20)\n"
            "project(TypesGuardFixture CXX)\n"
            "add_library(NoMoreDayTypes INTERFACE)\n"
            "target_include_directories(NoMoreDayTypes INTERFACE\n"
            "    ${CMAKE_CURRENT_SOURCE_DIR}/src\n)\n"
            "function(_nmd_ms1_types_boundary_final_guard_7f3c9a)\n"
            "  cmake_language(DEFER DIRECTORY \"${CMAKE_SOURCE_DIR}\" GET_CALL_IDS _nmd_types_pending_deferred_calls)\n"
            "  get_target_property(_kind NoMoreDayTypes TYPE)\n"
            "  get_target_property(_sources NoMoreDayTypes SOURCES)\n"
            "  get_target_property(_interface_sources NoMoreDayTypes INTERFACE_SOURCES)\n"
            "  get_target_property(_links NoMoreDayTypes LINK_LIBRARIES)\n"
            "  get_target_property(_interface_links NoMoreDayTypes INTERFACE_LINK_LIBRARIES)\n"
            "  get_target_property(_pch NoMoreDayTypes PRECOMPILE_HEADERS)\n"
            "  get_target_property(_interface_pch NoMoreDayTypes INTERFACE_PRECOMPILE_HEADERS)\n"
            "  get_target_property(_includes NoMoreDayTypes INTERFACE_INCLUDE_DIRECTORIES)\n"
            "  get_target_property(_system_includes NoMoreDayTypes INTERFACE_SYSTEM_INCLUDE_DIRECTORIES)\n"
            "  get_filename_component(_nmd_types_expected_include_root ${CMAKE_CURRENT_SOURCE_DIR}/src REALPATH)\n"
            "  list(LENGTH _includes _include_count)\n"
            "  if(NOT _kind STREQUAL INTERFACE_LIBRARY OR _sources OR _interface_sources OR _links OR _interface_links OR _pch OR _interface_pch)\n"
            "    message(FATAL_ERROR NoMoreDayTypes_property_contract_failed)\n"
            "  endif()\n"
            "  if(NOT _include_count EQUAL 1 OR NOT \"${_includes}\" STREQUAL \"${_nmd_types_expected_include_root}\" OR _system_includes)\n"
            "    message(FATAL_ERROR NoMoreDayTypes_include_contract_failed)\n"
            "  endif()\n"
            "  if(_nmd_types_pending_deferred_calls)\n"
            "    message(FATAL_ERROR NoMoreDayTypes_deferred_contract_failed)\n"
            "  endif()\n"
            "endfunction()\n"
            "cmake_language(DEFER CALL _nmd_ms1_types_boundary_final_guard_7f3c9a)\n"
            "add_subdirectory(src/core)\n"
            "add_subdirectory(src/engine)\n"
            "add_subdirectory(src/game)\n"
            "add_subdirectory(src/app)\n"
            "add_subdirectory(tests)\n",
            encoding="utf-8",
        )
        contract_path = root / CONTRACT_PATH
        contract_path.parent.mkdir(parents=True)
        contract_path.write_text(json.dumps(make_contract()), encoding="utf-8")
        return temporary_directory, root, contract_path

    def test_repository_contract_passes(self) -> None:
        result = self.run_checker(REPO_ROOT)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("PASS", result.stdout)

    def test_fixture_accepts_explicit_root_and_default_contract_path(self) -> None:
        temporary_directory, root, _ = self.make_fixture()
        with temporary_directory:
            result = self.run_checker(root)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_fixture_configures_normally(self) -> None:
        temporary_directory, root, _ = self.make_fixture()
        with temporary_directory:
            result = self.run_cmake(root)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_forbidden_candidate_dependency_fails(self) -> None:
        temporary_directory, root, _ = self.make_fixture()
        with temporary_directory:
            (root / "src" / "core" / "Logger.hpp").write_text(
                '#include "game/components/Common.hpp"\n', encoding="utf-8"
            )
            result = self.run_checker(root)
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("forbidden dependency", result.stdout)

    def test_contract_pch_and_cmake_boundary_drift_fail(self) -> None:
        temporary_directory, root, contract_path = self.make_fixture()
        with temporary_directory:
            contract = make_contract()
            contract["pch_inventory"]["NoMoreDayTypes"]["approved_pch"] = "src/pch.hpp"
            contract_path.write_text(json.dumps(contract), encoding="utf-8")
            result = self.run_checker(root)
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertIn("approved_pch must be null", result.stdout)

        temporary_directory, root, _ = self.make_fixture()
        with temporary_directory:
            (root / "CMakeLists.txt").write_text(
                "add_library(NoMoreDayTypes INTERFACE)\n"
                "target_include_directories(NoMoreDayTypes INTERFACE src extra)\n",
                encoding="utf-8",
            )
            result = self.run_checker(root)
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("expose only", result.stdout)

    def test_target_sources_and_property_bypasses_fail(self) -> None:
        for mutation in (
            "target_sources(NoMoreDayTypes INTERFACE extra.cpp)\n",
            "set_property(TARGET NoMoreDayTypes APPEND PROPERTY "
            "INTERFACE_SOURCES extra.cpp)\n",
            "set_target_properties(NoMoreDayTypes PROPERTIES "
            "INTERFACE_SOURCES extra.cpp)\n",
        ):
            temporary_directory, root, _ = self.make_fixture()
            with temporary_directory:
                (root / "tests" / "CMakeLists.txt").write_text(
                    mutation, encoding="utf-8"
                )
                result = self.run_checker(root)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn("property mutation", result.stdout)

    def test_static_checker_scans_first_party_cmake_modules(self) -> None:
        temporary_directory, root, _ = self.make_fixture()
        with temporary_directory:
            module = root / "cmake" / "types_mutation.cmake"
            module.parent.mkdir()
            module.write_text(
                "target_sources(NoMoreDayTypes INTERFACE extra.cpp)\n",
                encoding="utf-8",
            )
            result = self.run_checker(root)
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("types_mutation.cmake", result.stdout)

    def test_link_and_pch_mutations_fail(self) -> None:
        for mutation in (
            "target_link_libraries(NoMoreDayTypes PRIVATE dependency)\n",
            "target_link_libraries(NoMoreDayTypes INTERFACE dependency)\n",
            "target_precompile_headers(NoMoreDayTypes PRIVATE pch.hpp)\n",
            "target_precompile_headers(NoMoreDayTypes INTERFACE pch.hpp)\n",
        ):
            temporary_directory, root, _ = self.make_fixture()
            with temporary_directory:
                (root / "tests" / "CMakeLists.txt").write_text(
                    mutation, encoding="utf-8"
                )
                result = self.run_checker(root)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn("property mutation", result.stdout)

    def test_deferred_guard_catches_dynamic_mutations(self) -> None:
        cases = {
            "variable target": (
                "set(_target NoMoreDayTypes)\n"
                "target_sources(${_target} INTERFACE extra.cpp)\n"
            ),
            "bracket argument": (
                "set(_target NoMoreDayTypes)\n"
                "target_sources(${_target} INTERFACE [=[extra.cpp]=])\n"
            ),
            "included module": (
                "include(${CMAKE_CURRENT_LIST_DIR}/mutation.cmake)\n"
            ),
            "system include": (
                "set(_target NoMoreDayTypes)\n"
                "target_include_directories(${_target} SYSTEM INTERFACE extra)\n"
            ),
            "interface link": (
                "set(_target NoMoreDayTypes)\n"
                "target_link_libraries(${_target} INTERFACE dependency)\n"
            ),
            "interface PCH": (
                "set(_target NoMoreDayTypes)\n"
                "target_precompile_headers(${_target} INTERFACE pch.hpp)\n"
            ),
        }
        for name, mutation in cases.items():
            with self.subTest(name=name):
                temporary_directory, root, _ = self.make_fixture()
                with temporary_directory:
                    if name == "included module":
                        (root / "tests" / "mutation.cmake").write_text(
                            "set(_target NoMoreDayTypes)\n"
                            "target_sources(${_target} INTERFACE extra.cpp)\n",
                            encoding="utf-8",
                        )
                    (root / "tests" / "CMakeLists.txt").write_text(
                        mutation, encoding="utf-8"
                    )
                    result = self.run_cmake(root)
                self.assertNotEqual(
                    result.returncode, 0, result.stdout + result.stderr
                )
                self.assertIn("NoMoreDayTypes", result.stdout + result.stderr)

    def test_guard_redefinition_fails_preconfigure_contract(self) -> None:
        temporary_directory, root, _ = self.make_fixture()
        with temporary_directory:
            (root / "tests" / "CMakeLists.txt").write_text(
                "function(_nmd_ms1_types_boundary_final_guard_7f3c9a)\n"
                "endfunction()\n",
                encoding="utf-8",
            )
            result = self.run_cmake_with_contract_check(root)
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("NoMoreDayTypes final guard", result.stdout + result.stderr)
        self.assertIn("definition", result.stdout + result.stderr)

    def test_final_guard_macro_and_case_variants_fail_preconfigure_contract(
        self,
    ) -> None:
        cases = {
            "macro": (
                "macro(_nmd_ms1_types_boundary_final_guard_7f3c9a)\n"
                "endmacro()\n"
            ),
            "case-variant function": (
                "function(_NMD_MS1_TYPES_BOUNDARY_FINAL_GUARD_7F3C9A)\n"
                "endfunction()\n"
            ),
            "case-variant macro": (
                "macro(_NMD_MS1_TYPES_BOUNDARY_FINAL_GUARD_7F3C9A)\n"
                "endmacro()\n"
            ),
        }
        for name, declaration in cases.items():
            with self.subTest(name=name):
                temporary_directory, root, _ = self.make_fixture()
                with temporary_directory:
                    (root / "tests" / "CMakeLists.txt").write_text(
                        declaration
                        + "set(_target NoMoreDayTypes)\n"
                        + "target_sources(${_target} INTERFACE extra.cpp)\n",
                        encoding="utf-8",
                    )
                    result = self.run_cmake_with_contract_check(root)
                self.assertNotEqual(
                    result.returncode, 0, result.stdout + result.stderr
                )
                self.assertIn(
                    "NoMoreDayTypes final guard", result.stdout + result.stderr
                )
                self.assertIn("definition", result.stdout + result.stderr)

    def test_guard_command_shadowing_and_variable_mutation_fail_preconfigure(
        self,
    ) -> None:
        temporary_directory, root, _ = self.make_fixture()
        with temporary_directory:
            (root / "tests" / "CMakeLists.txt").write_text(
                "function(get_target_property output target property)\n"
                "  if(\"${property}\" STREQUAL \"TYPE\")\n"
                "    set(${output} INTERFACE_LIBRARY PARENT_SCOPE)\n"
                "  elseif(\"${property}\" STREQUAL \"INTERFACE_INCLUDE_DIRECTORIES\")\n"
                "    set(${output} \"${CMAKE_SOURCE_DIR}/src\" PARENT_SCOPE)\n"
                "  else()\n"
                "    unset(${output} PARENT_SCOPE)\n"
                "  endif()\n"
                "endfunction()\n"
                "set(_target NoMoreDayTypes)\n"
                "target_sources(${_target} INTERFACE extra.cpp)\n",
                encoding="utf-8",
            )
            result = self.run_cmake_with_contract_check(root)
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("command redefinition", result.stdout + result.stderr)
        self.assertIn("get_target_property", result.stdout + result.stderr)

    def test_guard_dependent_macro_shadowing_fails_static_contract(self) -> None:
        temporary_directory, root, _ = self.make_fixture()
        with temporary_directory:
            (root / "tests" / "CMakeLists.txt").write_text(
                "macro(list)\nendmacro()\n", encoding="utf-8"
            )
            result = self.run_checker(root)
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("command redefinition", result.stdout)
        self.assertIn("macro(list)", result.stdout)

    def test_bracket_guard_command_shadowing_and_variable_mutation_fail(self) -> None:
        temporary_directory, root, _ = self.make_fixture()
        with temporary_directory:
            (root / "tests" / "CMakeLists.txt").write_text(
                "function([=[get_target_property]=] output target property)\n"
                "  if(\"${property}\" STREQUAL \"TYPE\")\n"
                "    set(${output} INTERFACE_LIBRARY PARENT_SCOPE)\n"
                "  elseif(\"${property}\" STREQUAL \"INTERFACE_INCLUDE_DIRECTORIES\")\n"
                "    set(${output} \"${CMAKE_SOURCE_DIR}/src\" PARENT_SCOPE)\n"
                "  else()\n"
                "    unset(${output} PARENT_SCOPE)\n"
                "  endif()\n"
                "endfunction()\n"
                "set(_target NoMoreDayTypes)\n"
                "target_sources(${_target} INTERFACE extra.cpp)\n",
                encoding="utf-8",
            )
            result = self.run_cmake_with_contract_check(root)
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("command redefinition", result.stdout + result.stderr)
        self.assertIn("get_target_property", result.stdout + result.stderr)

    def test_variable_guard_command_shadowing_and_variable_mutation_fail(self) -> None:
        temporary_directory, root, _ = self.make_fixture()
        with temporary_directory:
            (root / "tests" / "CMakeLists.txt").write_text(
                "set(_command get_target_property)\n"
                "function(${_command} output target property)\n"
                "  if(\"${property}\" STREQUAL \"TYPE\")\n"
                "    set(${output} INTERFACE_LIBRARY PARENT_SCOPE)\n"
                "  elseif(\"${property}\" STREQUAL \"INTERFACE_INCLUDE_DIRECTORIES\")\n"
                "    set(${output} \"${CMAKE_SOURCE_DIR}/src\" PARENT_SCOPE)\n"
                "  else()\n"
                "    unset(${output} PARENT_SCOPE)\n"
                "  endif()\n"
                "endfunction()\n"
                "set(_target NoMoreDayTypes)\n"
                "target_sources(${_target} INTERFACE extra.cpp)\n",
                encoding="utf-8",
            )
            result = self.run_cmake_with_contract_check(root)
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("literal identifier", result.stdout + result.stderr)
        self.assertIn("function(${_command}", result.stdout + result.stderr)

    def test_root_deferred_types_mutation_fails_configure(self) -> None:
        temporary_directory, root, _ = self.make_fixture()
        with temporary_directory:
            (root / "tests" / "CMakeLists.txt").write_text(
                "cmake_language(DEFER DIRECTORY \"${CMAKE_SOURCE_DIR}\" "
                "CALL target_sources NoMoreDayTypes INTERFACE extra.cpp)\n",
                encoding="utf-8",
            )
            checker_result = self.run_checker(root)
            result = self.run_cmake(root)
        self.assertEqual(
            checker_result.returncode, 1,
            checker_result.stdout + checker_result.stderr,
        )
        self.assertIn("forbids non-sanctioned", checker_result.stdout)
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("deferred_contract_failed", result.stdout + result.stderr)

    def test_eval_guard_redefinition_fails_preconfigure_contract(self) -> None:
        temporary_directory, root, _ = self.make_fixture()
        with temporary_directory:
            (root / "tests" / "CMakeLists.txt").write_text(
                "cmake_language(EVAL CODE [=[\n"
                "function(_nmd_ms1_types_boundary_final_guard_7f3c9a)\n"
                "endfunction()\n]=])\n",
                encoding="utf-8",
            )
            result = self.run_cmake_with_contract_check(root)
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("forbids non-sanctioned", result.stdout + result.stderr)
        self.assertIn("EVAL", result.stdout + result.stderr)

    def test_eval_deferred_guard_cancellation_and_mutation_fail(self) -> None:
        temporary_directory, root, _ = self.make_fixture()
        with temporary_directory:
            (root / "tests" / "CMakeLists.txt").write_text(
                "cmake_language(EVAL CODE [=[\n"
                "cmake_language(DEFER DIRECTORY \"${CMAKE_SOURCE_DIR}\" "
                "GET_CALL_IDS _guard_call_ids)\n"
                "cmake_language(DEFER DIRECTORY \"${CMAKE_SOURCE_DIR}\" "
                "CANCEL_CALL ${_guard_call_ids})\n"
                "target_sources(NoMoreDayTypes INTERFACE extra.cpp)\n]=])\n",
                encoding="utf-8",
            )
            result = self.run_cmake_with_contract_check(root)
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("forbids non-sanctioned", result.stdout + result.stderr)
        self.assertIn("CANCEL_CALL", result.stdout + result.stderr)

    def test_manifest_completeness_and_aggregate_type_fail(self) -> None:
        temporary_directory, root, contract_path = self.make_fixture()
        with temporary_directory:
            (root / "src" / "core" / "Unlisted.hpp").write_text(
                "#pragma once\n", encoding="utf-8"
            )
            result = self.run_checker(root)
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("missing from manifest", result.stdout)

        temporary_directory, root, contract_path = self.make_fixture()
        with temporary_directory:
            contract = make_contract()
            contract["layered_targets"]["targets"] = 1
            contract_path.write_text(json.dumps(contract), encoding="utf-8")
            result = self.run_checker(root)
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertIn("layered_targets.targets", result.stdout)


if __name__ == "__main__":
    unittest.main()
