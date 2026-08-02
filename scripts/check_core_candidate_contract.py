#!/usr/bin/env python3
"""Check the MS-7 layered Core manifest and Types CMake boundary."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any, Iterator


SCHEMA_VERSION = "1.1"
CONTRACT_RELATIVE_PATH = Path(
    "docs/reports/modular-split-exe-lib-dll/ms-1/core-candidate-contract.json"
)
FORBIDDEN_INCLUDE_PREFIXES = ("engine/", "game/", "app/")
TYPES_GUARD_COMMAND = "_nmd_ms1_types_boundary_final_guard_7f3c9a"
TYPES_GUARD_DEPENDENT_COMMANDS = {
    "cmake_language",
    "get_target_property",
    "get_filename_component",
    "set",
    "list",
    "message",
}
TYPES_MUTATION_COMMANDS = {
    "target_sources",
    "target_link_libraries",
    "target_precompile_headers",
    "target_include_directories",
    "set_property",
    "set_target_properties",
}
INCLUDE_PATTERN = re.compile(
    r'^\s*#\s*include\s*[<"]((?:engine|game|app)/[^>"]+)[>"]'
)
CMK_IDENTIFIER_PATTERN = re.compile(r"[A-Za-z_][A-Za-z0-9_]*\Z")

# The four STATIC layers must each be defined exactly once in their own
# directory CMakeLists, in the order Core -> Engine -> Game -> App.
LAYER_TARGETS = {
    "NoMoreDayApp": ("src/app/CMakeLists.txt", "STATIC"),
    "NoMoreDayGame": ("src/game/CMakeLists.txt", "STATIC"),
    "NoMoreDayEngine": ("src/engine/CMakeLists.txt", "STATIC"),
    "NoMoreDayCore": ("src/core/CMakeLists.txt", "STATIC"),
}
LAYER_LINK_EDGES = (
    ("src/app/CMakeLists.txt", "NoMoreDayApp", "NoMoreDayGame", "PUBLIC"),
    ("src/game/CMakeLists.txt", "NoMoreDayGame", "NoMoreDayEngine", "PUBLIC"),
    ("src/engine/CMakeLists.txt", "NoMoreDayEngine", "NoMoreDayCore", "PUBLIC"),
    ("src/core/CMakeLists.txt", "NoMoreDayCore", "NoMoreDayTypes", "PUBLIC"),
)


class ContractInputError(ValueError):
    """Raised when the contract or its checked inputs are malformed."""


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    """Parse command-line arguments.

    Args:
        argv: Optional command-line arguments for tests.

    Returns:
        Parsed command-line arguments.
    """
    parser = argparse.ArgumentParser(
        description="Check the MS-7 layered Core manifest contract"
    )
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="Repository root path",
    )
    parser.add_argument(
        "--contract",
        type=Path,
        default=CONTRACT_RELATIVE_PATH,
        help="Contract path relative to --repo-root",
    )
    return parser.parse_args(argv)


def _require_keys(value: Any, keys: set[str], field: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ContractInputError(f"{field} must be an object")
    missing = keys - set(value)
    extra = set(value) - keys
    if missing or extra:
        raise ContractInputError(
            f"{field} fields invalid; missing={sorted(missing)}, "
            f"extra={sorted(extra)}"
        )
    return value


def _require_string(value: Any, field: str) -> str:
    if not isinstance(value, str) or not value:
        raise ContractInputError(f"{field} must be a non-empty string")
    return value


def _require_string_list(value: Any, field: str) -> list[str]:
    if not isinstance(value, list) or not all(
        isinstance(item, str) and item for item in value
    ):
        raise ContractInputError(f"{field} must be a list of non-empty strings")
    if len(value) != len(set(value)):
        raise ContractInputError(f"{field} must not contain duplicate paths")
    return value


def _require_core_path(path: str, field: str, extensions: tuple[str, ...]) -> None:
    if not path.startswith("src/core/") or Path(path).suffix not in extensions:
        expected = ", ".join(extensions)
        raise ContractInputError(
            f"{field} must be a src/core path with extension {expected}"
        )


def _resolve_relative(repo_root: Path, value: str, field: str) -> Path:
    path = Path(value)
    if path.is_absolute() or ".." in path.parts:
        raise ContractInputError(f"{field} must be repository-relative: {value}")
    resolved = (repo_root / path).resolve()
    try:
        resolved.relative_to(repo_root.resolve())
    except ValueError as exc:
        raise ContractInputError(f"{field} escapes repository root: {value}") from exc
    return resolved


def load_contract(contract_path: Path) -> dict[str, Any]:
    """Load and validate the contract schema.

    Args:
        contract_path: JSON contract path.

    Returns:
        The validated contract object.

    Raises:
        ContractInputError: If the contract is missing or malformed.
    """
    if not contract_path.is_file():
        raise ContractInputError(f"contract not found: {contract_path}")
    try:
        loaded = json.loads(contract_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise ContractInputError(
            f"cannot read contract {contract_path}: {exc}"
        ) from exc
    contract = _require_keys(
        loaded,
        {
            "schema_version", "milestone", "status", "layered_targets",
            "types_target", "future_core_candidate", "pch_inventory", "audit_scope",
        },
        "contract",
    )
    if contract["schema_version"] != SCHEMA_VERSION:
        raise ContractInputError("unsupported schema_version")
    if contract["milestone"] != "MS-7":
        raise ContractInputError("milestone must be MS-7")
    _require_string(contract["status"], "status")
    _require_string(contract["audit_scope"], "audit_scope")
    layered = _require_keys(
        contract["layered_targets"], {"targets", "link_chain"}, "layered_targets",
    )
    if not isinstance(layered["targets"], list) or not layered["targets"]:
        raise ContractInputError("layered_targets.targets must be a non-empty list")
    for index, entry in enumerate(layered["targets"]):
        target = _require_keys(
            entry, {"name", "kind", "file"},
            f"layered_targets.targets[{index}]",
        )
        for field in ("name", "kind", "file"):
            _require_string(target[field], f"layered_targets.targets[{index}].{field}")
    expected_names = {
        "NoMoreDayTypes", "NoMoreDayCore", "NoMoreDayEngine",
        "NoMoreDayGame", "NoMoreDayApp",
    }
    if {target["name"] for target in layered["targets"]} != expected_names:
        raise ContractInputError(
            "layered_targets.targets must name the five layered targets"
        )
    if not isinstance(layered["link_chain"], list) or not layered["link_chain"]:
        raise ContractInputError("layered_targets.link_chain must be a non-empty list")
    for index, entry in enumerate(layered["link_chain"]):
        edge = _require_keys(
            entry, {"from", "to", "scope", "file"},
            f"layered_targets.link_chain[{index}]",
        )
        for field in ("from", "to", "scope", "file"):
            _require_string(edge[field], f"layered_targets.link_chain[{index}].{field}")
        if edge["scope"] not in {"PUBLIC", "PRIVATE"}:
            raise ContractInputError(
                f"layered_targets.link_chain[{index}].scope must be PUBLIC or PRIVATE"
            )
    types = _require_keys(
        contract["types_target"],
        {
            "name", "kind", "public_include_root", "sources",
            "precompiled_headers", "link_dependencies", "admission_requirements",
            "explicit_exclusions",
        },
        "types_target",
    )
    if (
        types["name"] != "NoMoreDayTypes" or types["kind"] != "INTERFACE"
        or types["public_include_root"] != "src"
    ):
        raise ContractInputError("types_target identity or public include root is invalid")
    for field in ("sources", "precompiled_headers", "link_dependencies"):
        if types[field] != []:
            raise ContractInputError(f"types_target.{field} must be empty")
    for field in ("admission_requirements", "explicit_exclusions"):
        _require_string_list(types[field], f"types_target.{field}")
    core = _require_keys(
        contract["future_core_candidate"],
        {
            "name", "role", "eligible_implementation_sources",
            "eligible_headers", "deferred_items", "dependency_rule",
        },
        "future_core_candidate",
    )
    if core["name"] != "NoMoreDayCore":
        raise ContractInputError("future core candidate must be NoMoreDayCore")
    _require_string(core["role"], "future_core_candidate.role")
    _require_string(core["dependency_rule"], "future_core_candidate.dependency_rule")
    eligible_sources = _require_string_list(
        core["eligible_implementation_sources"],
        "future_core_candidate.eligible_implementation_sources",
    )
    eligible_headers = _require_string_list(
        core["eligible_headers"], "future_core_candidate.eligible_headers"
    )
    for path in eligible_sources:
        _require_core_path(
            path,
            "future_core_candidate.eligible_implementation_sources",
            (".cpp",),
        )
    for path in eligible_headers:
        _require_core_path(
            path, "future_core_candidate.eligible_headers", (".hpp", ".h")
        )
    if not isinstance(core["deferred_items"], list):
        raise ContractInputError("future_core_candidate.deferred_items must be a list")
    for index, item in enumerate(core["deferred_items"]):
        deferred = _require_keys(
            item, {"path", "reason", "owner_resolution"},
            f"future_core_candidate.deferred_items[{index}]",
        )
        for field in ("path", "reason", "owner_resolution"):
            _require_string(deferred[field], f"deferred_items[{index}].{field}")
        _require_core_path(
            deferred["path"],
            f"future_core_candidate.deferred_items[{index}].path",
            (".cpp", ".hpp", ".h"),
        )
    manifest_paths = (
        eligible_sources
        + eligible_headers
        + [item["path"] for item in core["deferred_items"]]
    )
    if len(manifest_paths) != len(set(manifest_paths)):
        raise ContractInputError("future_core_candidate paths must be mutually unique")
    _validate_pch_inventory(contract["pch_inventory"])
    return contract


def _validate_pch_inventory(value: Any) -> None:
    inventory = _require_keys(
        value,
        {"NoMoreDayTypes", "future_NoMoreDayCore", "current_aggregate_NoMoreDayCore"},
        "pch_inventory",
    )
    for name in ("NoMoreDayTypes", "future_NoMoreDayCore"):
        entry = _require_keys(inventory[name], {"approved_pch", "rule"}, name)
        if entry["approved_pch"] is not None:
            raise ContractInputError(f"{name}.approved_pch must be null")
        _require_string(entry["rule"], f"{name}.rule")
    aggregate = _require_keys(
        inventory["current_aggregate_NoMoreDayCore"],
        {"path", "status", "direct_game_includes", "direct_engine_includes"},
        "current_aggregate_NoMoreDayCore",
    )
    if aggregate["path"] != "src/pch.hpp":
        raise ContractInputError("current aggregate PCH path must be src/pch.hpp")
    _require_string(aggregate["status"], "current aggregate PCH status")
    for field in ("direct_game_includes", "direct_engine_includes"):
        _require_string_list(aggregate[field], f"current aggregate PCH {field}")


def _iter_cmake_commands(content: str) -> Iterator[tuple[str, str]]:
    position = 0
    while position < len(content):
        match = re.search(r"([A-Za-z_][A-Za-z0-9_]*)\s*\(", content[position:])
        if match is None:
            return
        command = _normalize_cmake_name(match.group(1))
        opening = position + match.end() - 1
        depth = 1
        quote = False
        cursor = opening + 1
        while cursor < len(content) and depth:
            character = content[cursor]
            if character == '"' and (cursor == 0 or content[cursor - 1] != "\\"):
                quote = not quote
            elif not quote and character == "(":
                depth += 1
            elif not quote and character == ")":
                depth -= 1
            cursor += 1
        if depth:
            raise ContractInputError(f"unterminated CMake command: {command}")
        yield command, content[opening + 1:cursor - 1]
        if command in {"function", "macro", "if", "foreach", "while", "block"}:
            yield from _iter_cmake_commands(content[opening + 1:cursor - 1])
        position = cursor


def _cmake_tokens(arguments: str) -> list[str]:
    """Return CMake arguments with quoted and bracket literals normalized."""
    tokens: list[str] = []
    position = 0
    while position < len(arguments):
        while position < len(arguments) and arguments[position].isspace():
            position += 1
        if position == len(arguments):
            break
        bracket = re.match(r"\[(=*)\[", arguments[position:])
        if bracket is not None:
            closing = "]" + bracket.group(1) + "]"
            content_start = position + bracket.end()
            content_end = arguments.find(closing, content_start)
            if content_end == -1:
                raise ContractInputError("unterminated CMake bracket argument")
            tokens.append(arguments[content_start:content_end])
            position = content_end + len(closing)
            continue
        if arguments[position] == '"':
            position += 1
            token_start = position
            while position < len(arguments):
                if (
                    arguments[position] == '"'
                    and arguments[position - 1] != "\\"
                ):
                    break
                position += 1
            if position == len(arguments):
                raise ContractInputError("unterminated CMake quoted argument")
            tokens.append(arguments[token_start:position])
            position += 1
            continue
        token_start = position
        while position < len(arguments) and not arguments[position].isspace():
            position += 1
        tokens.append(arguments[token_start:position])
    return tokens


def _normalize_cmake_name(value: str) -> str:
    """Return the case-insensitive form used for CMake command names."""
    return value.lower()


def _first_party_cmake_files(repo_root: Path) -> list[Path]:
    excluded_parts = {"third_party", "build", ".git"}
    return sorted(
        path for path in repo_root.rglob("*")
        if path.is_file()
        and (path.name == "CMakeLists.txt" or path.suffix == ".cmake")
        and not excluded_parts.intersection(path.relative_to(repo_root).parts)
    )


def _is_sanctioned_types_include(
    path: Path, repo_root: Path, command: str, body: str
) -> bool:
    return (
        path == repo_root / "CMakeLists.txt"
        and command == "target_include_directories"
        and _cmake_tokens(body) == [
            "NoMoreDayTypes",
            "INTERFACE",
            "${CMAKE_CURRENT_SOURCE_DIR}/src",
        ]
    )


def _is_sanctioned_core_types_link(
    path: Path, repo_root: Path, command: str, body: str
) -> bool:
    """Return whether Core's PUBLIC link to NoMoreDayTypes is permitted."""
    tokens = _cmake_tokens(body)
    return (
        path == repo_root / "src" / "core" / "CMakeLists.txt"
        and command == "target_link_libraries"
        and len(tokens) >= 3
        and tokens[0] == "NoMoreDayCore"
        and tokens[1] == "PUBLIC"
        and "NoMoreDayTypes" in tokens[2:]
    )


def _is_types_guard_declaration(command: str, body: str) -> bool:
    """Return whether a function or macro declares the final guard name."""
    tokens = _cmake_tokens(body)
    return (
        command in {"function", "macro"}
        and bool(tokens)
        and _normalize_cmake_name(tokens[0]) == TYPES_GUARD_COMMAND
    )


def _is_sanctioned_types_guard_definition(
    path: Path, repo_root: Path, command: str, body: str
) -> bool:
    """Return whether this is the one permitted root final-guard function."""
    return (
        path == repo_root / "CMakeLists.txt"
        and command == "function"
        and _is_types_guard_declaration(command, body)
    )


def _is_guard_dependent_command_definition(command: str, body: str) -> bool:
    """Return whether a function or macro shadows a final-guard command."""
    tokens = _cmake_tokens(body)
    return (
        command in {"function", "macro"}
        and bool(tokens)
        and _normalize_cmake_name(tokens[0]) in TYPES_GUARD_DEPENDENT_COMMANDS
    )


def _is_static_cmake_definition_name(command: str, body: str) -> bool:
    """Return whether a function or macro name is a literal identifier."""
    if command not in {"function", "macro"}:
        return True
    tokens = _cmake_tokens(body)
    return bool(tokens) and CMK_IDENTIFIER_PATTERN.fullmatch(tokens[0]) is not None


def _is_sanctioned_types_guard_defer(path: Path, repo_root: Path,
                                     body: str) -> bool:
    tokens = _cmake_tokens(body)
    return (
        path == repo_root / "CMakeLists.txt"
        and len(tokens) == 3
        and [_normalize_cmake_name(token) for token in tokens] == [
            "defer", "call", TYPES_GUARD_COMMAND,
        ]
    )


def _is_sanctioned_types_guard_defer_query(path: Path, repo_root: Path,
                                             body: str) -> bool:
    tokens = _cmake_tokens(body)
    return (
        path == repo_root / "CMakeLists.txt"
        and len(tokens) == 5
        and [_normalize_cmake_name(token) for token in tokens[:2]] == [
            "defer", "directory",
        ]
        and tokens[2] == "${CMAKE_SOURCE_DIR}"
        and _normalize_cmake_name(tokens[3]) == "get_call_ids"
        and tokens[4] == "_nmd_types_pending_deferred_calls"
    )


def _is_sanctioned_cmake_language(path: Path, repo_root: Path,
                                  body: str) -> bool:
    """Return whether a CMake language command is required by the final guard."""
    return (
        _is_sanctioned_types_guard_defer(path, repo_root, body)
        or _is_sanctioned_types_guard_defer_query(path, repo_root, body)
    )


def validate_cmake_types_contract(repo_root: Path) -> list[str]:
    """Return static CMake policy violations for NoMoreDayTypes.

    Args:
        repo_root: Repository root containing CMakeLists.txt.

    Returns:
        Human-readable policy violations.
    """
    cmake_path = repo_root / "CMakeLists.txt"
    if not cmake_path.is_file():
        raise ContractInputError(f"CMakeLists.txt not found: {cmake_path}")
    try:
        root_content = cmake_path.read_text(encoding="utf-8")
        commands = list(_iter_cmake_commands(root_content))
    except (OSError, UnicodeError) as exc:
        raise ContractInputError(f"cannot read {cmake_path}: {exc}") from exc
    errors: list[str] = []
    cmake_files = _first_party_cmake_files(repo_root)
    parsed_files: list[tuple[Path, list[tuple[str, str]]]] = []
    for listed_path in cmake_files:
        try:
            listed_commands = list(
                _iter_cmake_commands(listed_path.read_text(encoding="utf-8"))
            )
        except (OSError, UnicodeError) as exc:
            raise ContractInputError(f"cannot read {listed_path}: {exc}") from exc
        parsed_files.append((listed_path, listed_commands))
    definitions = [
        (listed_path, body)
        for listed_path, listed_commands in parsed_files
        for command, body in listed_commands
        if command == "add_library" and "NoMoreDayTypes" in body
    ]
    if len(definitions) != 1 or definitions[0][0] != cmake_path or _cmake_tokens(
        definitions[0][1]
    ) != ["NoMoreDayTypes", "INTERFACE"]:
        errors.append("NoMoreDayTypes must be defined exactly as an INTERFACE library")
    includes = [
        body for _, listed_commands in parsed_files
        for command, body in listed_commands
        if command == "target_include_directories"
        and _cmake_tokens(body)[:1] == ["NoMoreDayTypes"]
    ]
    expected_include = [
        "NoMoreDayTypes", "INTERFACE", "${CMAKE_CURRENT_SOURCE_DIR}/src",
    ]
    if len(includes) != 1 or _cmake_tokens(includes[0]) != expected_include:
        errors.append("NoMoreDayTypes must expose only ${CMAKE_CURRENT_SOURCE_DIR}/src")
    guard_declarations = [
        (listed_path, command, body)
        for listed_path, listed_commands in parsed_files
        for command, body in listed_commands
        if _is_types_guard_declaration(command, body)
    ]
    if len(guard_declarations) != 1 or not _is_sanctioned_types_guard_definition(
        guard_declarations[0][0], repo_root, guard_declarations[0][1],
        guard_declarations[0][2],
    ):
        errors.append(
            "NoMoreDayTypes final guard must have one root function definition"
        )
    for listed_path, listed_commands in parsed_files:
        for command, body in listed_commands:
            if not _is_static_cmake_definition_name(command, body):
                errors.append(
                    "first-party CMake function/macro declaration name must be "
                    "a literal identifier: "
                    f"{listed_path.relative_to(repo_root)}:{command}({body})"
                )
            if _is_guard_dependent_command_definition(command, body):
                shadowed_command = _cmake_tokens(body)[0]
                errors.append(
                    "NoMoreDayTypes final guard command redefinition is "
                    f"forbidden: {listed_path.relative_to(repo_root)}:"
                    f"{command}({shadowed_command})"
                )
            if (
                command in TYPES_MUTATION_COMMANDS
                and "NoMoreDayTypes" in body
                and not _is_sanctioned_types_include(
                    listed_path, repo_root, command, body
                )
                and not _is_sanctioned_core_types_link(
                    listed_path, repo_root, command, body
                )
            ):
                errors.append(
                    "NoMoreDayTypes property mutation is forbidden: "
                    f"{listed_path.relative_to(repo_root)}:{command}"
                )
            if (
                command == "cmake_language"
                and not _is_sanctioned_cmake_language(
                    listed_path, repo_root, body
                )
            ):
                errors.append(
                    "NoMoreDayTypes policy forbids non-sanctioned "
                    "cmake_language commands (including EVAL and "
                    f"CANCEL_CALL): {listed_path.relative_to(repo_root)}"
                )
    required_guard_terms = (
        "INTERFACE_SOURCES", "LINK_LIBRARIES", "INTERFACE_LINK_LIBRARIES",
        "PRECOMPILE_HEADERS", "INTERFACE_PRECOMPILE_HEADERS",
        "INTERFACE_INCLUDE_DIRECTORIES", "INTERFACE_SYSTEM_INCLUDE_DIRECTORIES",
        "_nmd_types_expected_include_root", "GET_CALL_IDS",
        "_nmd_types_pending_deferred_calls", TYPES_GUARD_COMMAND,
    )
    for term in required_guard_terms:
        if term not in root_content:
            errors.append(f"NoMoreDayTypes configure-time guard lacks {term}")
    deferred_guard_calls = [
        body for command, body in commands
        if command == "cmake_language"
        and _is_sanctioned_types_guard_defer(cmake_path, repo_root, body)
    ]
    if len(deferred_guard_calls) != 1:
        errors.append(
            "NoMoreDayTypes configure-time guard must use one root-scope DEFER call"
        )
    return errors


def validate_layered_target_graph(repo_root: Path) -> list[str]:
    """Return layered-target definition and link-chain violations.

    Args:
        repo_root: Repository root containing the layer CMakeLists files.

    Returns:
        Human-readable layered-target policy violations.
    """
    cmake_files = _first_party_cmake_files(repo_root)
    parsed_files: dict[str, list[tuple[str, str]]] = {}
    for listed_path in cmake_files:
        relative = listed_path.relative_to(repo_root).as_posix()
        try:
            parsed_files[relative] = list(
                _iter_cmake_commands(listed_path.read_text(encoding="utf-8"))
            )
        except (OSError, UnicodeError) as exc:
            raise ContractInputError(
                f"cannot read {listed_path}: {exc}"
            ) from exc
    errors: list[str] = []
    for name, (relative, kind) in LAYER_TARGETS.items():
        commands = parsed_files.get(relative, [])
        definitions = [
            body for command, body in commands
            if command == "add_library" and _cmake_tokens(body)[:2] == [name, kind]
        ]
        if len(definitions) != 1:
            errors.append(
                f"layered target {name} must be defined exactly once as {kind} "
                f"in {relative}"
            )
        for other, other_commands in parsed_files.items():
            if other == relative:
                continue
            if any(
                command == "add_library" and _cmake_tokens(body)[:1] == [name]
                for command, body in other_commands
            ):
                errors.append(
                    f"layered target {name} must not be defined outside {relative}"
                )
    for relative, from_target, to_target, scope in LAYER_LINK_EDGES:
        commands = parsed_files.get(relative, [])
        matches = [
            body for command, body in commands
            if command == "target_link_libraries"
            and _cmake_tokens(body)[:1] == [from_target]
            and scope in _cmake_tokens(body)[1:]
            and to_target in _cmake_tokens(body)[1:]
        ]
        if len(matches) != 1:
            errors.append(
                f"layered link {from_target} {scope} {to_target} must appear "
                f"exactly once in {relative}"
            )
    return errors


def validate_contract_paths(repo_root: Path, contract: dict[str, Any]) -> list[str]:
    """Return contract path, PCH inventory, and dependency violations.

    Args:
        repo_root: Repository root containing contract paths.
        contract: Schema-validated contract object.

    Returns:
        Human-readable policy violations.
    """
    core = contract["future_core_candidate"]
    paths = (
        core["eligible_implementation_sources"] + core["eligible_headers"]
    )
    paths += [item["path"] for item in core["deferred_items"]]
    paths.append(contract["pch_inventory"]["current_aggregate_NoMoreDayCore"]["path"])
    errors: list[str] = []
    for path in paths:
        resolved = _resolve_relative(repo_root, path, "contract path")
        if not resolved.is_file():
            errors.append(f"contract path does not exist: {path}")
    for path in core["eligible_implementation_sources"] + core["eligible_headers"]:
        resolved = _resolve_relative(repo_root, path, "eligible candidate path")
        if not resolved.is_file():
            continue
        try:
            lines = resolved.read_text(encoding="utf-8-sig").splitlines()
        except (OSError, UnicodeError) as exc:
            raise ContractInputError(f"cannot read candidate {path}: {exc}") from exc
        for line_number, line in enumerate(lines, start=1):
            match = INCLUDE_PATTERN.match(line)
            if match and match.group(1).startswith(FORBIDDEN_INCLUDE_PREFIXES):
                errors.append(
                    f"eligible candidate includes forbidden dependency: "
                    f"{path}:{line_number} -> {match.group(1)}"
                )
    manifest_paths = set(
        core["eligible_implementation_sources"]
        + core["eligible_headers"]
        + [item["path"] for item in core["deferred_items"]]
    )
    core_paths = {
        path.relative_to(repo_root).as_posix()
        for path in (repo_root / "src" / "core").rglob("*")
        if path.is_file() and path.suffix in {".cpp", ".hpp", ".h"}
    }
    missing_paths = sorted(core_paths - manifest_paths)
    unexpected_paths = sorted(manifest_paths - core_paths)
    for path in missing_paths:
        errors.append(f"src/core file missing from manifest: {path}")
    for path in unexpected_paths:
        errors.append(f"manifest path is not a direct src/core file: {path}")
    pch = contract["pch_inventory"]["current_aggregate_NoMoreDayCore"]
    pch_path = _resolve_relative(repo_root, pch["path"], "aggregate PCH path")
    if pch_path.is_file():
        includes = [
            match.group(1) for line in pch_path.read_text(encoding="utf-8-sig").splitlines()
            if (match := INCLUDE_PATTERN.match(line))
        ]
        expected = pch["direct_game_includes"] + pch["direct_engine_includes"]
        if includes != expected:
            errors.append("aggregate PCH direct Engine/Game include inventory drifted")
    return errors


def main(argv: list[str] | None = None) -> int:
    """Run the MS-7 layered contract check and return its process status."""
    args = parse_args(argv)
    repo_root = args.repo_root.resolve()
    contract_path = args.contract
    if not contract_path.is_absolute():
        contract_path = repo_root / contract_path
    try:
        contract = load_contract(contract_path.resolve())
        errors = validate_contract_paths(repo_root, contract)
        errors.extend(validate_cmake_types_contract(repo_root))
        errors.extend(validate_layered_target_graph(repo_root))
    except (ContractInputError, OSError) as exc:
        print(f"[MS-7 Contract] ERROR: {exc}")
        return 2
    if errors:
        print(f"[MS-7 Contract] FAILED: {len(errors)} violation(s).")
        for error in errors:
            print(f"[MS-7 Contract] - {error}")
        return 1
    print("[MS-7 Contract] PASS: layered targets, Core manifest, and Types CMake boundary match.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
