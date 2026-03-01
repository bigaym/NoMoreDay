import argparse
import json
import re
from pathlib import Path
from typing import Any


REPO_ROOT = Path(__file__).resolve().parent.parent

MAP_AFFIX_HEADER = REPO_ROOT / "src/game/data/MapAffix.hpp"
MAP_AFFIX_REGISTRY = REPO_ROOT / "src/game/systems/world/MapAffixRegistry.hpp"
MAP_MODIFIER_ADAPTER = REPO_ROOT / "src/game/systems/modifier/MapModifierAdapter.cpp"
MONSTER_AFFIX_REGISTRY = REPO_ROOT / "src/game/data/MonsterAffixRegistry.hpp"
STATS_HEADER = REPO_ROOT / "src/game/components/Stats.hpp"

MAP_OUTPUT = REPO_ROOT / "assets/data/modifier_v2/map_modifiers.json"
MONSTER_OUTPUT = REPO_ROOT / "assets/data/modifier_v2/monster_modifiers.json"

MAP_ID_BASE = 4_001_000
MONSTER_ID_BASE = 5_001_000
MAP_NODE_BASE = 700_000
MONSTER_NODE_BASE = 800_000
MAP_PRIORITY = 400
MONSTER_PRIORITY = 500

MONSTER_UPDATE_BEHAVIOR_OPS: dict[str, str] = {
    "Molten": "MONSTER_BEHAVIOR_MOLTEN_UPDATE",
    "Teleporter": "MONSTER_BEHAVIOR_TELEPORTER_UPDATE",
    "Frozen": "MONSTER_BEHAVIOR_FROZEN_UPDATE",
    "ManaSiphon": "MONSTER_BEHAVIOR_MANA_SIPHON_UPDATE",
    "Shielding": "MONSTER_BEHAVIOR_SHIELDING_UPDATE",
    "Vortex": "MONSTER_BEHAVIOR_VORTEX_UPDATE",
    "Waller": "MONSTER_BEHAVIOR_WALLER_UPDATE",
    "Berserker": "MONSTER_BEHAVIOR_BERSERKER_UPDATE",
    "Storm": "MONSTER_BEHAVIOR_STORM_UPDATE",
    "VoidZone": "MONSTER_BEHAVIOR_VOIDZONE_UPDATE",
    "Suppressor": "MONSTER_BEHAVIOR_SUPPRESSOR_UPDATE",
    "SoulLink": "MONSTER_BEHAVIOR_SOUL_LINK_UPDATE",
}

MONSTER_ON_HIT_BEHAVIOR_OPS: dict[str, str] = {
    "Vampiric": "MONSTER_BEHAVIOR_VAMPIRIC_ON_HIT",
    "Nullifier": "MONSTER_BEHAVIOR_NULLIFIER_ON_HIT",
    "Entangler": "MONSTER_BEHAVIOR_ENTANGLER_ON_HIT",
    "MirrorImage": "MONSTER_BEHAVIOR_MIRROR_IMAGE_ON_TAKE_DAMAGE",
    "StormStrider": "MONSTER_BEHAVIOR_STORM_STRIDER_ON_TAKE_DAMAGE",
    "Void": "MONSTER_BEHAVIOR_VOID_ON_HIT",
}

MONSTER_ON_DEATH_BEHAVIOR_OPS: dict[str, str] = {
    "Toxic": "MONSTER_BEHAVIOR_TOXIC_ON_DEATH",
    "SoulEater": "MONSTER_BEHAVIOR_SOUL_EATER_ON_ENEMY_DEATH",
    "Avenger": "MONSTER_BEHAVIOR_AVENGER_ON_NEARBY_DEATH",
}

IMPLEMENTED_BEHAVIOR_HANDLER_AFFIXES: frozenset[str] = frozenset(
    {
        "Molten",
        "Teleporter",
        "Frozen",
        "ManaSiphon",
        "Shielding",
        "Vortex",
        "Waller",
        "Berserker",
        "Storm",
        "VoidZone",
        "Suppressor",
        "SoulLink",
        "Vampiric",
        "Nullifier",
        "Entangler",
        "MirrorImage",
        "StormStrider",
        "Void",
        "Toxic",
        "SoulEater",
        "Avenger",
    }
)

INTENTIONALLY_BEHAVIOR_LESS_AFFIXES: frozenset[str] = frozenset(
    {
        "Fast",
        "Tanky",
        "Powerful",
        "Accurate",
    }
)


def _build_behavior_opcodes_by_affix() -> dict[str, frozenset[str]]:
    behavior_opcodes: dict[str, set[str]] = {}

    for affix_name, opcode in MONSTER_UPDATE_BEHAVIOR_OPS.items():
        behavior_opcodes.setdefault(affix_name, set()).add(opcode)
    for affix_name, opcode in MONSTER_ON_HIT_BEHAVIOR_OPS.items():
        behavior_opcodes.setdefault(affix_name, set()).add(opcode)
    for affix_name, opcode in MONSTER_ON_DEATH_BEHAVIOR_OPS.items():
        behavior_opcodes.setdefault(affix_name, set()).add(opcode)

    return {
        affix_name: frozenset(sorted(opcodes))
        for affix_name, opcodes in sorted(behavior_opcodes.items())
    }


MONSTER_BEHAVIOR_OPCODES_BY_AFFIX: dict[str, frozenset[str]] = (
    _build_behavior_opcodes_by_affix()
)

WEAPON_CLASS_MASK_ALL = 0xFFFFFFFF


def _load_text(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except OSError as err:
        raise RuntimeError(f"failed to read '{path}': {err}") from err


def _strip_cpp_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return re.sub(r"//.*", "", text)


def _extract_balanced(
    text: str, open_index: int, open_char: str, close_char: str
) -> tuple[str, int]:
    depth = 0
    in_string = False
    escaped = False
    for index in range(open_index, len(text)):
        char = text[index]

        if in_string:
            if escaped:
                escaped = False
                continue
            if char == "\\":
                escaped = True
                continue
            if char == '"':
                in_string = False
            continue

        if char == '"':
            in_string = True
            continue

        if char == open_char:
            depth += 1
        elif char == close_char:
            depth -= 1
            if depth == 0:
                return text[open_index : index + 1], index + 1

    raise RuntimeError(f"unterminated block starting at index {open_index}")


def _split_top_level_csv(text: str) -> list[str]:
    parts: list[str] = []
    current: list[str] = []
    brace_depth = 0
    paren_depth = 0
    bracket_depth = 0
    in_string = False
    escaped = False

    for char in text:
        if in_string:
            current.append(char)
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
            continue

        if char == '"':
            in_string = True
            current.append(char)
            continue

        if char == "{":
            brace_depth += 1
        elif char == "}":
            brace_depth -= 1
        elif char == "(":
            paren_depth += 1
        elif char == ")":
            paren_depth -= 1
        elif char == "[":
            bracket_depth += 1
        elif char == "]":
            bracket_depth -= 1

        if char == "," and brace_depth == 0 and paren_depth == 0 and bracket_depth == 0:
            value = "".join(current).strip()
            if value:
                parts.append(value)
            current = []
        else:
            current.append(char)

    value = "".join(current).strip()
    if value:
        parts.append(value)
    return parts


def _parse_cpp_number(text: str) -> float:
    cleaned = text.strip()
    if cleaned.endswith(("f", "F")):
        cleaned = cleaned[:-1]
    try:
        return float(cleaned)
    except ValueError as err:
        raise RuntimeError(f"unable to parse numeric literal '{text}'") from err


def _parse_cpp_int(text: str) -> int:
    cleaned = text.strip()
    try:
        return int(cleaned, 0)
    except ValueError as err:
        raise RuntimeError(f"unable to parse integer literal '{text}'") from err


def _parse_enum_values(text: str, enum_name: str) -> dict[str, int]:
    stripped = _strip_cpp_comments(text)
    enum_match = re.search(rf"enum\s+class\s+{enum_name}\b[^{{]*{{", stripped)
    if not enum_match:
        raise RuntimeError(f"could not locate enum '{enum_name}'")

    start = stripped.find("{", enum_match.start())
    if start < 0:
        raise RuntimeError(f"could not find body start for enum '{enum_name}'")

    enum_block, _ = _extract_balanced(stripped, start, "{", "}")
    entries = _split_top_level_csv(enum_block[1:-1])

    values: dict[str, int] = {}
    current = 0
    for entry in entries:
        raw = entry.strip()
        if not raw:
            continue
        if "=" in raw:
            name, value_text = raw.split("=", 1)
            name = name.strip()
            current = _parse_cpp_int(value_text)
        else:
            name = raw
        values[name] = current
        current += 1

    if not values:
        raise RuntimeError(f"enum '{enum_name}' has no values")
    return values


def _parse_map_registry_values(
    map_registry_text: str, map_affix_enum: dict[str, int]
) -> dict[str, float]:
    array_marker = map_registry_text.find("G_AFFIX_DEFINITIONS")
    if array_marker < 0:
        raise RuntimeError(
            "failed to locate G_AFFIX_DEFINITIONS in MapAffixRegistry.hpp"
        )
    assign_index = map_registry_text.find("=", array_marker)
    if assign_index < 0:
        raise RuntimeError("failed to locate G_AFFIX_DEFINITIONS initializer")
    array_start = map_registry_text.find("{", assign_index)
    if array_start < 0:
        raise RuntimeError("failed to locate G_AFFIX_DEFINITIONS brace block")
    array_block, _ = _extract_balanced(map_registry_text, array_start, "{", "}")

    entries: list[str] = []
    search_index = 0
    while True:
        match = re.search(r"MapAffixDefinition\s*\{", array_block[search_index:])
        if not match:
            break
        brace_start = search_index + match.end() - 1
        entry_block, next_index = _extract_balanced(array_block, brace_start, "{", "}")
        entries.append(entry_block[1:-1])
        search_index = next_index

    if not entries:
        raise RuntimeError(
            "failed to locate MapAffixDefinition entries in MapAffixRegistry.hpp"
        )

    count_value = map_affix_enum.get("Count")
    if count_value is None:
        raise RuntimeError("MapAffixType::Count not found")
    if len(entries) < count_value:
        raise RuntimeError(
            f"MapAffixRegistry.hpp has {len(entries)} definitions, expected at least {count_value}"
        )

    ordered_names = sorted(
        ((name, value) for name, value in map_affix_enum.items() if name != "Count"),
        key=lambda item: item[1],
    )

    val_t1_by_affix: dict[str, float] = {}
    for index, (affix_name, _) in enumerate(ordered_names):
        fields = _split_top_level_csv(entries[index])
        if len(fields) < 7:
            raise RuntimeError(f"malformed MapAffixDefinition entry for '{affix_name}'")
        val_t1_by_affix[affix_name] = _parse_cpp_number(fields[5])

    return val_t1_by_affix


def _parse_map_adapter_enemy_templates(adapter_text: str) -> dict[str, str]:
    cases = re.findall(
        r"case\s+MapAffixType::(\w+)\s*:(.*?)break\s*;", adapter_text, flags=re.DOTALL
    )
    if not cases:
        raise RuntimeError(
            "no MapAffixType case blocks found in MapModifierAdapter.cpp"
        )

    mappings: dict[str, str] = {}
    for affix_name, body in cases:
        op_match = re.search(
            r"AddPercentMultRecord\s*\(\s*records\s*,\s*nodeId\s*,\s*StatType::(\w+)\s*,\s*affix\.value\s*\)",
            body,
            flags=re.DOTALL,
        )
        if op_match:
            mappings[affix_name] = op_match.group(1)

    if not mappings:
        raise RuntimeError(
            "no combat-relevant AddPercentMultRecord mappings found in MapModifierAdapter.cpp"
        )
    return mappings


def _parse_stat_mode_value_triplets(
    stat_mods_expr: str,
) -> list[tuple[str, str, float]]:
    triplets = re.findall(
        r"\{\s*StatType::(\w+)\s*,\s*ModifierMode::(\w+)\s*,\s*([-+]?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?f?)\s*\}",
        stat_mods_expr,
    )
    parsed: list[tuple[str, str, float]] = []
    for stat_type, mode, value_text in triplets:
        parsed.append((stat_type, mode, _parse_cpp_number(value_text)))
    return parsed


def _parse_flags(flags_expr: str) -> tuple[bool, bool, bool]:
    booleans = re.findall(r"\b(true|false)\b", flags_expr)
    if not booleans and flags_expr.strip() == "{}":
        return False, False, False
    if len(booleans) < 3:
        raise RuntimeError(
            f"failed to parse AffixFlags expression: '{flags_expr.strip()}'"
        )
    return booleans[0] == "true", booleans[1] == "true", booleans[2] == "true"


def _parse_monster_affix_defs(
    registry_text: str, monster_affix_enum: dict[str, int]
) -> dict[str, dict[str, Any]]:
    stripped = _strip_cpp_comments(registry_text)

    array_match = re.search(r"kAffixData\s*=\s*\{\{", stripped)
    if not array_match:
        raise RuntimeError("could not locate MonsterAffixRegistry::kAffixData")

    array_start = stripped.find("{{", array_match.start())
    array_block, _ = _extract_balanced(stripped, array_start, "{", "}")

    entries: dict[str, dict[str, Any]] = {}
    search_index = 0
    while True:
        match = re.search(
            r"\{\s*MonsterAffixType::(\w+)\s*,", array_block[search_index:]
        )
        if not match:
            break
        entry_start = search_index + match.start()
        entry_block, next_index = _extract_balanced(array_block, entry_start, "{", "}")
        search_index = next_index

        fields = _split_top_level_csv(entry_block[1:-1])
        if len(fields) < 7:
            raise RuntimeError(
                "malformed MonsterAffixDef initializer in MonsterAffixRegistry.hpp"
            )

        type_match = re.search(r"MonsterAffixType::(\w+)", fields[0])
        if not type_match:
            raise RuntimeError("failed to parse MonsterAffixType in affix initializer")
        affix_name = type_match.group(1)

        if affix_name not in monster_affix_enum:
            raise RuntimeError(
                f"affix '{affix_name}' not found in MonsterAffixType enum"
            )

        stat_mods = _parse_stat_mode_value_triplets(fields[4])
        stat_mod_count = _parse_cpp_int(fields[5])
        if stat_mod_count != len(stat_mods):
            raise RuntimeError(
                f"statModCount mismatch for '{affix_name}': declared {stat_mod_count}, parsed {len(stat_mods)}"
            )

        has_update, has_on_hit, has_on_death = _parse_flags(fields[6])

        entries[affix_name] = {
            "stat_mods": stat_mods,
            "has_update": has_update,
            "has_on_hit": has_on_hit,
            "has_on_death": has_on_death,
        }

    if not entries:
        raise RuntimeError(
            "no MonsterAffixDef entries parsed from MonsterAffixRegistry.hpp"
        )
    return entries


def _normalize_percent(value: float) -> float:
    return round(value / 100.0, 6)


def _make_filters(node_id: int) -> dict[str, Any]:
    return {
        "profession_mask": 0,
        "skill_id_whitelist": [],
        "required_skill_tags_all": 0,
        "forbidden_skill_tags_any": 0,
        "weapon_class_mask": WEAPON_CLASS_MASK_ALL,
        "equip_slot_mask": 0,
        "node_id_whitelist": [node_id],
    }


def _make_constraints() -> dict[str, int]:
    return {"exclusive_group": 0, "max_active": 0}


def _append_behavior_op(
    ops: list[dict[str, Any]], behavior_opcode: str | None, affix_id: int
) -> None:
    if behavior_opcode is None:
        return
    if any(op.get("opcode") == behavior_opcode for op in ops):
        return
    ops.append(
        {
            "opcode": behavior_opcode,
            "target": "enemy",
            "param_u32": affix_id,
            "param_f32": 0.0,
        }
    )


def _validate_behavior_affix_classification(
    monster_affix_enum: dict[str, int],
    monster_defs: dict[str, dict[str, Any]],
) -> None:
    mapped_affixes = set(MONSTER_BEHAVIOR_OPCODES_BY_AFFIX)
    overlap = IMPLEMENTED_BEHAVIOR_HANDLER_AFFIXES & INTENTIONALLY_BEHAVIOR_LESS_AFFIXES
    if overlap:
        raise RuntimeError(
            "behavior affix classification overlap: " + ", ".join(sorted(overlap))
        )

    behavior_less_with_mapping = INTENTIONALLY_BEHAVIOR_LESS_AFFIXES & mapped_affixes
    if behavior_less_with_mapping:
        raise RuntimeError(
            "behavior-less affixes must not have behavior op mappings: "
            + ", ".join(sorted(behavior_less_with_mapping))
        )

    missing_implemented = mapped_affixes - IMPLEMENTED_BEHAVIOR_HANDLER_AFFIXES
    if missing_implemented:
        raise RuntimeError(
            "behavior op mappings missing implemented classification: "
            + ", ".join(sorted(missing_implemented))
        )

    known_affixes = set(monster_affix_enum) - {"None", "Count"}
    invalid_implemented = IMPLEMENTED_BEHAVIOR_HANDLER_AFFIXES - known_affixes
    if invalid_implemented:
        raise RuntimeError(
            "implemented behavior affix classification references unknown affixes: "
            + ", ".join(sorted(invalid_implemented))
        )

    invalid_behavior_less = INTENTIONALLY_BEHAVIOR_LESS_AFFIXES - known_affixes
    if invalid_behavior_less:
        raise RuntimeError(
            "behavior-less affix classification references unknown affixes: "
            + ", ".join(sorted(invalid_behavior_less))
        )

    flagged_affixes = {
        affix_name
        for affix_name, affix_def in monster_defs.items()
        if affix_def["has_update"]
        or affix_def["has_on_hit"]
        or affix_def["has_on_death"]
    }
    classified_flagged = (
        IMPLEMENTED_BEHAVIOR_HANDLER_AFFIXES | INTENTIONALLY_BEHAVIOR_LESS_AFFIXES
    )
    unclassified_flagged = flagged_affixes - classified_flagged
    if unclassified_flagged:
        raise RuntimeError(
            "flagged behavior affixes missing classification: "
            + ", ".join(sorted(unclassified_flagged))
        )

    unclassified_known_affixes = known_affixes - classified_flagged
    if unclassified_known_affixes:
        raise RuntimeError(
            "affixes missing behavior classification: "
            + ", ".join(sorted(unclassified_known_affixes))
        )


def _collect_behavior_opcodes(ops: list[dict[str, Any]]) -> frozenset[str]:
    return frozenset(
        sorted(
            {
                str(opcode)
                for opcode in (op.get("opcode") for op in ops)
                if isinstance(opcode, str) and opcode.startswith("MONSTER_BEHAVIOR_")
            }
        )
    )


def _validate_behavior_opcode_contract(
    emitted_behavior_opcodes: dict[str, frozenset[str]],
) -> None:
    failures: list[str] = []

    for affix_name in sorted(IMPLEMENTED_BEHAVIOR_HANDLER_AFFIXES):
        emitted = emitted_behavior_opcodes.get(affix_name, frozenset())
        if not emitted:
            failures.append(
                f"implemented behavior affix '{affix_name}' emitted no behavior opcode"
            )

    for affix_name in sorted(INTENTIONALLY_BEHAVIOR_LESS_AFFIXES):
        emitted = emitted_behavior_opcodes.get(affix_name, frozenset())
        if emitted:
            failures.append(
                f"behavior-less affix '{affix_name}' emitted unexpected behavior opcode(s): "
                + ", ".join(sorted(emitted))
            )

    if failures:
        raise RuntimeError(
            "behavior opcode contract violations:\n  - " + "\n  - ".join(failures)
        )


def _build_map_records(
    map_affix_enum: dict[str, int],
    stat_type_enum: dict[str, int],
    map_val_t1: dict[str, float],
    map_templates: dict[str, str],
) -> list[dict[str, Any]]:
    records: list[dict[str, Any]] = []
    for affix_name, affix_value in sorted(
        ((name, value) for name, value in map_affix_enum.items() if name != "Count"),
        key=lambda item: item[1],
    ):
        stat_name = map_templates.get(affix_name)
        if not stat_name:
            continue
        if stat_name not in stat_type_enum:
            raise RuntimeError(
                f"unknown StatType '{stat_name}' referenced by MapModifierAdapter"
            )
        if affix_name not in map_val_t1:
            raise RuntimeError(
                f"affix '{affix_name}' missing from MapAffixRegistry definitions"
            )

        node_id = MAP_NODE_BASE + affix_value
        record = {
            "id": MAP_ID_BASE + affix_value,
            "domain": "map",
            "priority": MAP_PRIORITY,
            "filters": _make_filters(node_id),
            "constraints": _make_constraints(),
            "ops": [
                {
                    "opcode": "ADD_STAT_PERCENT_MULT",
                    "target": "enemy",
                    "param_u32": stat_type_enum[stat_name],
                    "param_f32": _normalize_percent(map_val_t1[affix_name]),
                }
            ],
            "debug": {
                "name": f"Map_{affix_name}",
                "source": "map_affix",
            },
        }
        records.append(record)

    if not records:
        raise RuntimeError("map generation produced zero records")
    return records


def _mode_to_opcode(mode_name: str) -> str:
    if mode_name == "Flat":
        return "ADD_STAT_FLAT"
    if mode_name == "PercentAdd":
        return "ADD_STAT_PERCENT_ADD"
    if mode_name == "PercentMult":
        return "ADD_STAT_PERCENT_MULT"
    raise RuntimeError(f"unsupported ModifierMode '{mode_name}'")


def _mode_normalize_value(mode_name: str, value: float) -> float:
    if mode_name == "Flat":
        return round(value, 6)
    return _normalize_percent(value)


def _build_monster_records(
    monster_affix_enum: dict[str, int],
    stat_type_enum: dict[str, int],
    monster_defs: dict[str, dict[str, Any]],
) -> tuple[list[dict[str, Any]], dict[str, frozenset[str]]]:
    records: list[dict[str, Any]] = []
    emitted_behavior_opcodes: dict[str, frozenset[str]] = {}
    for affix_name, affix_value in sorted(
        (
            (name, value)
            for name, value in monster_affix_enum.items()
            if name not in {"None", "Count"}
        ),
        key=lambda item: item[1],
    ):
        affix_def = monster_defs.get(affix_name)
        if affix_def is None:
            if affix_name in MONSTER_BEHAVIOR_OPCODES_BY_AFFIX:
                affix_def = {
                    "stat_mods": [],
                    "has_update": affix_name in MONSTER_UPDATE_BEHAVIOR_OPS,
                    "has_on_hit": affix_name in MONSTER_ON_HIT_BEHAVIOR_OPS,
                    "has_on_death": affix_name in MONSTER_ON_DEATH_BEHAVIOR_OPS,
                }
            else:
                continue

        ops: list[dict[str, Any]] = []
        for stat_name, mode_name, raw_value in affix_def["stat_mods"]:
            if stat_name not in stat_type_enum:
                raise RuntimeError(
                    f"unknown StatType '{stat_name}' in affix '{affix_name}'"
                )

            ops.append(
                {
                    "opcode": _mode_to_opcode(mode_name),
                    "target": "enemy",
                    "param_u32": stat_type_enum[stat_name],
                    "param_f32": _mode_normalize_value(mode_name, raw_value),
                }
            )

        affix_id = affix_value
        if affix_def["has_update"]:
            ops.append(
                {
                    "opcode": "MONSTER_EVENT_ON_UPDATE",
                    "target": "enemy",
                    "param_u32": affix_id,
                    "param_f32": 0.0,
                }
            )
            _append_behavior_op(
                ops, MONSTER_UPDATE_BEHAVIOR_OPS.get(affix_name), affix_id
            )
        if affix_def["has_on_hit"]:
            ops.append(
                {
                    "opcode": "MONSTER_EVENT_ON_HIT",
                    "target": "enemy",
                    "param_u32": affix_id,
                    "param_f32": 0.0,
                }
            )
            _append_behavior_op(
                ops, MONSTER_ON_HIT_BEHAVIOR_OPS.get(affix_name), affix_id
            )
        if affix_def["has_on_death"]:
            ops.append(
                {
                    "opcode": "MONSTER_EVENT_ON_DEATH",
                    "target": "enemy",
                    "param_u32": affix_id,
                    "param_f32": 0.0,
                }
            )
            _append_behavior_op(
                ops, MONSTER_ON_DEATH_BEHAVIOR_OPS.get(affix_name), affix_id
            )

        expected_behavior_opcodes = MONSTER_BEHAVIOR_OPCODES_BY_AFFIX.get(
            affix_name, frozenset()
        )
        for behavior_opcode in expected_behavior_opcodes:
            _append_behavior_op(ops, behavior_opcode, affix_id)

        emitted_behavior_opcodes[affix_name] = _collect_behavior_opcodes(ops)

        if not ops:
            continue

        node_id = MONSTER_NODE_BASE + affix_value
        record = {
            "id": MONSTER_ID_BASE + affix_value,
            "domain": "monster",
            "priority": MONSTER_PRIORITY,
            "filters": _make_filters(node_id),
            "constraints": _make_constraints(),
            "ops": ops,
            "debug": {
                "name": f"Monster_{affix_name}",
                "source": "monster_affix",
            },
        }
        records.append(record)

    if not records:
        raise RuntimeError("monster generation produced zero records")
    return records, emitted_behavior_opcodes


def _render_json(payload: dict[str, Any]) -> str:
    return json.dumps(payload, indent=2, ensure_ascii=False) + "\n"


def _write(path: Path, text: str) -> None:
    path.write_text(text, encoding="utf-8", newline="\n")


def _check_matches(path: Path, expected_text: str) -> bool:
    if not path.exists():
        print(f"[modifier_v2] drift: missing file '{path}'")
        return False
    actual_text = path.read_text(encoding="utf-8")
    if actual_text != expected_text:
        print(f"[modifier_v2] drift: '{path}' is out of date")
        return False
    return True


def _generate() -> tuple[str, str, int, int]:
    map_affix_enum = _parse_enum_values(_load_text(MAP_AFFIX_HEADER), "MapAffixType")
    stat_type_enum = _parse_enum_values(_load_text(STATS_HEADER), "StatType")
    monster_affix_enum = _parse_enum_values(
        _load_text(MONSTER_AFFIX_REGISTRY), "MonsterAffixType"
    )

    map_val_t1 = _parse_map_registry_values(
        _load_text(MAP_AFFIX_REGISTRY), map_affix_enum
    )
    map_templates = _parse_map_adapter_enemy_templates(_load_text(MAP_MODIFIER_ADAPTER))
    monster_defs = _parse_monster_affix_defs(
        _load_text(MONSTER_AFFIX_REGISTRY), monster_affix_enum
    )
    _validate_behavior_affix_classification(monster_affix_enum, monster_defs)
    monster_records, emitted_behavior_opcodes = _build_monster_records(
        monster_affix_enum, stat_type_enum, monster_defs
    )
    _validate_behavior_opcode_contract(emitted_behavior_opcodes)

    map_payload = {
        "schema_version": 2,
        "domain": "map",
        "records": _build_map_records(
            map_affix_enum, stat_type_enum, map_val_t1, map_templates
        ),
    }
    monster_payload = {
        "schema_version": 2,
        "domain": "monster",
        "records": monster_records,
    }

    map_text = _render_json(map_payload)
    monster_text = _render_json(monster_payload)
    return (
        map_text,
        monster_text,
        len(map_payload["records"]),
        len(monster_payload["records"]),
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate modifier_v2 map/monster templates from C++ canonical registries."
    )
    parser.add_argument(
        "--check", action="store_true", help="fail if generated output differs"
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    try:
        (
            map_text,
            monster_text,
            map_count,
            monster_count,
        ) = _generate()
    except RuntimeError as err:
        print(f"[modifier_v2] generation failed: {err}")
        return 1

    if args.check:
        ok_map = _check_matches(MAP_OUTPUT, map_text)
        ok_monster = _check_matches(MONSTER_OUTPUT, monster_text)
        if ok_map and ok_monster:
            print(
                f"[modifier_v2] check passed: map={map_count} records, monster={monster_count} records"
            )
            return 0
        print(
            "[modifier_v2] check failed. Run: python scripts/gen_map_monster_modifier_v2.py"
        )
        return 1

    _write(MAP_OUTPUT, map_text)
    _write(MONSTER_OUTPUT, monster_text)
    print(f"[modifier_v2] generated '{MAP_OUTPUT}' ({map_count} records)")
    print(f"[modifier_v2] generated '{MONSTER_OUTPUT}' ({monster_count} records)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
