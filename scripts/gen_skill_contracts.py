"""
Generate skill_contract blocks in assets/data/skills.json from a compact config.

Usage:
  python scripts/gen_skill_contracts.py
  python scripts/gen_skill_contracts.py --check
  python scripts/gen_skill_contracts.py --check --check-idempotency --check-determinism
"""

from __future__ import annotations

import argparse
import copy
from dataclasses import dataclass
import json
from pathlib import Path
import re
import sys
from typing import Any

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SKILLS_JSON = ROOT / "assets" / "data" / "skills.json"
DEFAULT_MASTERY_SKILLS_JSON = ROOT / "assets" / "data" / "mastery_skill_trees.json"
DEFAULT_COMPACT_JSON = ROOT / "assets" / "data" / "skill_contracts_compact.json"
MIN_PYTHON = (3, 10)

# A-01 Phase 3：SpecState 生成器输入/产物与校验常量。
DEFAULT_SPECSTATE_DESCRIPTOR_DIR = ROOT / "assets" / "data" / "skill_specstate"
DEFAULT_SPECSTATE_OUTPUT_DIR = (
    ROOT / "src" / "game" / "systems" / "skill" / "behaviors" / "generated"
)
DEFAULT_MECHANICS_JSON = ROOT / "assets" / "data" / "skill_mechanics.json"
DEFAULT_MECHANICS_SCHEMA_JSON = ROOT / "assets" / "data" / "skill_mechanics_schema.json"

SPECSTATE_DESCRIPTOR_VERSION = 1
SPECSTATE_BANNER = (
    "本文件由 scripts/gen_skill_contracts.py --gen-specstate 生成，请勿手动编辑。"
)
IDENTIFIER_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
SPECSTATE_KIND_POINT = "point"
SPECSTATE_KIND_FLAG = "flag"
SPECSTATE_KINDS = (SPECSTATE_KIND_POINT, SPECSTATE_KIND_FLAG)

ROLE_PASSIVE = "Passive"
ROLE_KEYSTONE = "Keystone"
ROLE_TRIGGER = "Trigger"
ROLE_SYNERGY = "Synergy"
ROLE_TRANSMUTER = "Transmuter"

RESIST_NONE = "None"
SCOPE_SKILL_ONLY = "SkillOnly"

VALID_ROLES = {
    ROLE_PASSIVE,
    ROLE_KEYSTONE,
    ROLE_TRIGGER,
    ROLE_SYNERGY,
    ROLE_TRANSMUTER,
}

VALID_RESIST = {
    "None",
    "TypeA_Penetration",
    "TypeB_Shred",
    "TypeC_Exposure",
    "TypeD_StatToPenetration",
    "TypeE_CapSuppression",
}

VALID_SCOPE = {
    "SkillOnly",
    "GlobalWhileBuffActive",
    "GlobalAlways",
}

VALID_COST_AFFIX = {
    "None",
    "GlassCannonCrit",
    "HeavyMomentum",
}

VALID_TRIGGER_WINDOW = {
    "None",
    "PhantomTrance",
    "DeathSeal",
}


def _load_json(path: Path) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise ValueError(
            f"invalid JSON in {path}: {exc.msg} (line {exc.lineno}, col {exc.colno})"
        ) from exc


def _serialize_json(doc: Any) -> str:
    return json.dumps(doc, ensure_ascii=False, indent=2) + "\n"


def _require_object(value: Any, context: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ValueError(f"{context} must be an object")
    return value


def _require_list(value: Any, context: str) -> list[Any]:
    if not isinstance(value, list):
        raise ValueError(f"{context} must be a list")
    return value


def _read_int(value: Any, context: str) -> int:
    try:
        return int(value)
    except (TypeError, ValueError) as exc:
        raise ValueError(f"{context} must be an integer, got: {value!r}") from exc


def _validate_skills_doc(skills_doc: Any, skills_path: Path) -> None:
    doc = _require_object(skills_doc, str(skills_path))
    skills = _require_list(doc.get("skills"), f"{skills_path}:skills")
    seen_ids: set[int] = set()
    for idx, skill in enumerate(skills):
        entry = _require_object(skill, f"{skills_path}:skills[{idx}]")
        skill_id = _read_int(entry.get("id"), f"{skills_path}:skills[{idx}].id")
        if skill_id in seen_ids:
            raise ValueError(f"{skills_path}: duplicate skill id {skill_id}")
        seen_ids.add(skill_id)
        talent_tree = entry.get("talent_tree")
        if talent_tree is None:
            continue
        talent_tree = _require_list(
            talent_tree, f"{skills_path}:skills[{idx}].talent_tree"
        )
        node_ids: set[int] = set()
        for node_idx, node in enumerate(talent_tree):
            node_obj = _require_object(
                node, f"{skills_path}:skills[{idx}].talent_tree[{node_idx}]"
            )
            node_id = _read_int(
                node_obj.get("id"),
                f"{skills_path}:skills[{idx}].talent_tree[{node_idx}].id",
            )
            if node_id in node_ids:
                raise ValueError(
                    f"{skills_path}: duplicate node id {node_id} in skill {skill_id}"
                )
            node_ids.add(node_id)


def _validate_mastery_skills_doc(
    mastery_doc: Any,
    mastery_path: Path,
    base_skill_ids: set[int],
) -> None:
    doc = _require_object(mastery_doc, str(mastery_path))
    skills = _require_list(doc.get("skills"), f"{mastery_path}:skills")
    seen_ids: set[int] = set()
    for idx, skill in enumerate(skills):
        entry = _require_object(skill, f"{mastery_path}:skills[{idx}]")
        skill_id = _read_int(
            entry.get("skill_id"), f"{mastery_path}:skills[{idx}].skill_id"
        )
        if skill_id in seen_ids:
            raise ValueError(f"{mastery_path}: duplicate mastery skill_id {skill_id}")
        if skill_id not in base_skill_ids:
            raise ValueError(f"{mastery_path}: unknown mastery skill_id {skill_id}")
        seen_ids.add(skill_id)

        talent_tree = _require_list(
            entry.get("talent_tree"), f"{mastery_path}:skills[{idx}].talent_tree"
        )
        node_ids: set[int] = set()
        for node_idx, node in enumerate(talent_tree):
            node_obj = _require_object(
                node, f"{mastery_path}:skills[{idx}].talent_tree[{node_idx}]"
            )
            node_id = _read_int(
                node_obj.get("id"),
                f"{mastery_path}:skills[{idx}].talent_tree[{node_idx}].id",
            )
            if node_id in node_ids:
                raise ValueError(
                    f"{mastery_path}: duplicate node id {node_id} in skill {skill_id}"
                )
            node_ids.add(node_id)


def _validate_compact_doc(compact_doc: Any, compact_path: Path) -> None:
    doc = _require_object(compact_doc, str(compact_path))
    skills = _require_list(doc.get("skills"), f"{compact_path}:skills")
    seen_ids: set[int] = set()
    for idx, item in enumerate(skills):
        entry = _require_object(item, f"{compact_path}:skills[{idx}]")
        skill_id = _read_int(
            entry.get("skill_id"), f"{compact_path}:skills[{idx}].skill_id"
        )
        if skill_id in seen_ids:
            raise ValueError(f"{compact_path}: duplicate compact skill_id {skill_id}")
        seen_ids.add(skill_id)


def _as_int_set(values: list[Any]) -> set[int]:
    return {int(v) for v in values}


def _validate_enum_set(name: str, values: dict[str, str], allowed: set[str]) -> None:
    for node_id, value in values.items():
        if value not in allowed:
            raise ValueError(f"{name}[{node_id}] has invalid value: {value}")


def _validate_node_refs(
    *,
    skill_id: int,
    refs: set[int],
    node_ids: set[int],
    ref_name: str,
) -> None:
    invalid = sorted(ref for ref in refs if ref not in node_ids)
    if invalid:
        raise ValueError(
            f"skill {skill_id}: {ref_name} references unknown node ids: {invalid}"
        )


def _build_contract_for_skill(
    skill: dict[str, Any], cfg: dict[str, Any], verbose: bool
) -> dict[str, Any]:
    skill_id = _read_int(skill.get("skill_id", skill.get("id")), "skill.id")
    if skill_id != _read_int(
        cfg.get("skill_id"), f"compact(skill_id={skill_id}).skill_id"
    ):
        raise ValueError("skill_id mismatch between skill and compact config")

    talent_tree = _require_list(
        skill.get("talent_tree"), f"skill {skill_id}.talent_tree"
    )
    nodes_by_id: dict[int, dict[str, Any]] = {}
    for idx, node in enumerate(talent_tree):
        node_obj = _require_object(node, f"skill {skill_id}.talent_tree[{idx}]")
        node_id = _read_int(
            node_obj.get("id"), f"skill {skill_id}.talent_tree[{idx}].id"
        )
        if node_id in nodes_by_id:
            raise ValueError(f"skill {skill_id} has duplicate node id: {node_id}")
        nodes_by_id[node_id] = node_obj
    if not nodes_by_id:
        raise ValueError(f"skill {skill_id} has no talent_tree nodes")

    min_nodes = _read_int(
        cfg.get("min_nodes", len(nodes_by_id)), f"skill {skill_id}.min_nodes"
    )
    max_nodes = _read_int(
        cfg.get("max_nodes", len(nodes_by_id)), f"skill {skill_id}.max_nodes"
    )
    if min_nodes > max_nodes:
        raise ValueError(
            f"skill {skill_id} has min_nodes > max_nodes ({min_nodes} > {max_nodes})"
        )

    max_transmuters = _read_int(
        cfg.get("max_transmuters", 2), f"skill {skill_id}.max_transmuters"
    )
    max_triggers = _read_int(
        cfg.get("max_triggers", 1), f"skill {skill_id}.max_triggers"
    )
    if max_transmuters < 0 or max_triggers < 0:
        raise ValueError(f"skill {skill_id} has negative max_transmuters/max_triggers")

    has_sword_intent_node = bool(cfg.get("has_sword_intent_node", True))
    has_synergy_node = bool(cfg.get("has_synergy_node", True))

    transmuter_cfg = _require_list(
        cfg.get("transmuter_node_ids", []), f"skill {skill_id}.transmuter_node_ids"
    )
    if len(transmuter_cfg) > 2:
        raise ValueError(f"skill {skill_id} has more than 2 transmuter_node_ids")

    transmuter_ids = [
        _read_int(v, f"skill {skill_id}.transmuter_node_ids") for v in transmuter_cfg
    ][:2]
    while len(transmuter_ids) < 2:
        transmuter_ids.append(0)

    synergy_ids = _as_int_set(
        _require_list(
            cfg.get("synergy_node_ids", []), f"skill {skill_id}.synergy_node_ids"
        )
    )
    sword_intent_ids = _as_int_set(
        _require_list(
            cfg.get("sword_intent_node_ids", []),
            f"skill {skill_id}.sword_intent_node_ids",
        )
    )
    sword_step_ids = _as_int_set(
        _require_list(
            cfg.get("sword_step_node_ids", []), f"skill {skill_id}.sword_step_node_ids"
        )
    )
    explicit_keystone_ids = _as_int_set(
        _require_list(
            cfg.get("keystone_node_ids", []), f"skill {skill_id}.keystone_node_ids"
        )
    )
    explicit_passive_ids = _as_int_set(
        _require_list(
            cfg.get("passive_node_ids", []), f"skill {skill_id}.passive_node_ids"
        )
    )

    node_ids = set(nodes_by_id.keys())
    _validate_node_refs(
        skill_id=skill_id,
        refs={node_id for node_id in transmuter_ids if node_id != 0},
        node_ids=node_ids,
        ref_name="transmuter_node_ids",
    )
    _validate_node_refs(
        skill_id=skill_id,
        refs=synergy_ids,
        node_ids=node_ids,
        ref_name="synergy_node_ids",
    )
    _validate_node_refs(
        skill_id=skill_id,
        refs=sword_intent_ids,
        node_ids=node_ids,
        ref_name="sword_intent_node_ids",
    )
    _validate_node_refs(
        skill_id=skill_id,
        refs=sword_step_ids,
        node_ids=node_ids,
        ref_name="sword_step_node_ids",
    )
    _validate_node_refs(
        skill_id=skill_id,
        refs=explicit_keystone_ids,
        node_ids=node_ids,
        ref_name="keystone_node_ids",
    )
    _validate_node_refs(
        skill_id=skill_id,
        refs=explicit_passive_ids,
        node_ids=node_ids,
        ref_name="passive_node_ids",
    )

    trigger_nodes_cfg = _require_list(
        cfg.get("trigger_nodes", []), f"skill {skill_id}.trigger_nodes"
    )
    trigger_nodes: dict[int, dict[str, Any]] = {}
    for t in trigger_nodes_cfg:
        trigger_obj = _require_object(t, f"skill {skill_id}.trigger_nodes[]")
        node_id = _read_int(
            trigger_obj.get("node_id"), f"skill {skill_id}.trigger_nodes[].node_id"
        )
        if node_id not in node_ids:
            raise ValueError(
                f"skill {skill_id}: trigger_nodes references unknown node id {node_id}"
            )
        trigger_nodes[node_id] = {
            "trigger_skill_id": _read_int(
                trigger_obj.get("trigger_skill_id", 0),
                f"skill {skill_id}.trigger_nodes[{node_id}].trigger_skill_id",
            ),
            "effectiveness": float(trigger_obj.get("effectiveness", 0.0)),
            "range_mult": float(trigger_obj.get("range_mult", 1.0)),
            "internal_cooldown": float(trigger_obj.get("internal_cooldown", 0.0)),
            "consumes_mana": bool(trigger_obj.get("consumes_mana", False)),
            "requires_crit": bool(trigger_obj.get("requires_crit", False)),
        }
        # 仅当 compact 显式声明时写出新增触发字段，默认 base_chance=1.0、
        # requires_melee_hit=false、requires_window="None"，从而不影响其他技能既有契约
        if "base_chance" in trigger_obj:
            trigger_nodes[node_id]["base_chance"] = float(trigger_obj["base_chance"])
        if "requires_melee_hit" in trigger_obj:
            trigger_nodes[node_id]["requires_melee_hit"] = bool(
                trigger_obj["requires_melee_hit"]
            )
        if "requires_window" in trigger_obj:
            required_window = str(trigger_obj["requires_window"])
            if required_window not in VALID_TRIGGER_WINDOW:
                raise ValueError(
                    f"skill {skill_id}.trigger_nodes[{node_id}].requires_window "
                    f"has invalid value: {required_window}"
                )
            trigger_nodes[node_id]["required_window"] = required_window
    if len(trigger_nodes) > max_triggers:
        raise ValueError(
            f"skill {skill_id} has {len(trigger_nodes)} trigger nodes beyond max_triggers={max_triggers}"
        )

    resist_cfg = _require_object(
        cfg.get("resist_models", {}), f"skill {skill_id}.resist_models"
    )
    scope_cfg = _require_object(
        cfg.get("scope_policies", {}), f"skill {skill_id}.scope_policies"
    )
    exclusion_cfg = _require_object(
        cfg.get("keystone_exclusion_groups", {}),
        f"skill {skill_id}.keystone_exclusion_groups",
    )
    cost_affix_cfg = _require_object(
        cfg.get("cost_affixes", {}), f"skill {skill_id}.cost_affixes"
    )
    resist_models = {str(k): str(v) for k, v in resist_cfg.items()}
    scope_policies = {str(k): str(v) for k, v in scope_cfg.items()}
    keystone_exclusion_groups: dict[str, int] = {}
    for raw_node_id, raw_group_id in exclusion_cfg.items():
        node_id = _read_int(
            raw_node_id, f"skill {skill_id}.keystone_exclusion_groups node_id"
        )
        group_id = _read_int(
            raw_group_id,
            f"skill {skill_id}.keystone_exclusion_groups[{node_id}]",
        )
        if group_id < 0 or group_id > 255:
            raise ValueError(
                f"skill {skill_id}.keystone_exclusion_groups[{node_id}] must be in [0,255], got {group_id}"
            )
        keystone_exclusion_groups[str(node_id)] = group_id
    cost_affixes = {str(k): str(v) for k, v in cost_affix_cfg.items()}
    _validate_enum_set("resist_models", resist_models, VALID_RESIST)
    _validate_enum_set("scope_policies", scope_policies, VALID_SCOPE)
    _validate_enum_set("cost_affixes", cost_affixes, VALID_COST_AFFIX)
    _validate_node_refs(
        skill_id=skill_id,
        refs={int(node_id) for node_id in resist_models.keys()},
        node_ids=node_ids,
        ref_name="resist_models keys",
    )
    _validate_node_refs(
        skill_id=skill_id,
        refs={int(node_id) for node_id in scope_policies.keys()},
        node_ids=node_ids,
        ref_name="scope_policies keys",
    )
    _validate_node_refs(
        skill_id=skill_id,
        refs={int(node_id) for node_id in keystone_exclusion_groups.keys()},
        node_ids=node_ids,
        ref_name="keystone_exclusion_groups keys",
    )
    _validate_node_refs(
        skill_id=skill_id,
        refs={int(node_id) for node_id in cost_affixes.keys()},
        node_ids=node_ids,
        ref_name="cost_affixes keys",
    )

    node_contracts: list[dict[str, Any]] = []
    for node_id in sorted(nodes_by_id.keys()):
        node = nodes_by_id[node_id]
        max_points = _read_int(
            node.get("max_points", 1), f"skill {skill_id}.node {node_id}.max_points"
        )

        role = ROLE_PASSIVE
        if node_id in transmuter_ids:
            role = ROLE_TRANSMUTER
        elif node_id in synergy_ids:
            role = ROLE_SYNERGY
        elif node_id in explicit_keystone_ids:
            role = ROLE_KEYSTONE
        elif node_id in trigger_nodes:
            role = ROLE_TRIGGER

        if node_id in explicit_passive_ids:
            role = ROLE_PASSIVE

        if role not in VALID_ROLES:
            raise ValueError(f"role resolution failed for node {node_id}")

        resist_model = resist_models.get(str(node_id), RESIST_NONE)
        scope_policy = scope_policies.get(str(node_id), SCOPE_SKILL_ONLY)
        keystone_exclusion_group = keystone_exclusion_groups.get(str(node_id), 0)
        cost_affix = cost_affixes.get(str(node_id), "None")
        trigger = trigger_nodes.get(
            node_id,
            {
                "trigger_skill_id": 0,
                "effectiveness": 0.0,
                "range_mult": 1.0,
                "internal_cooldown": 0.0,
                "consumes_mana": False,
                "requires_crit": False,
            },
        )

        is_default_trigger = (
            trigger["trigger_skill_id"] == 0
            and trigger["effectiveness"] == 0.0
            and trigger["range_mult"] == 1.0
            and trigger["internal_cooldown"] == 0.0
            and trigger["consumes_mana"] is False
            and trigger["requires_crit"] is False
        )

        emits_non_default = (
            role != ROLE_PASSIVE
            or (node_id in explicit_passive_ids)
            or (max_points == 1 and role == ROLE_PASSIVE)
            or resist_model != RESIST_NONE
            or scope_policy != SCOPE_SKILL_ONLY
            or (keystone_exclusion_group != 0)
            or (cost_affix != "None")
            or (node_id in sword_intent_ids)
            or (node_id in sword_step_ids)
            or (not is_default_trigger)
        )
        if not emits_non_default:
            continue

        node_contract: dict[str, Any] = {
            "node_id": node_id,
            "role": role,
            "resist_model": resist_model,
            "scope_policy": scope_policy,
            "affects_sword_intent": node_id in sword_intent_ids,
            "affects_sword_step": node_id in sword_step_ids,
            "trigger": trigger,
        }
        if keystone_exclusion_group != 0:
            node_contract["keystone_exclusion_group"] = keystone_exclusion_group
        if cost_affix != "None":
            node_contract["cost_affix"] = cost_affix
        node_contracts.append(node_contract)

    return {
        "version": 1,
        "skill_id": skill_id,
        "min_nodes": min_nodes,
        "max_nodes": max_nodes,
        "max_transmuters": max_transmuters,
        "max_triggers": max_triggers,
        "has_sword_intent_node": has_sword_intent_node,
        "has_synergy_node": has_synergy_node,
        "transmuter_node_ids": transmuter_ids,
        "nodes": node_contracts,
    }


def _apply_contracts(
    *,
    entries_by_skill: dict[int, dict[str, Any]],
    compact_by_skill: dict[int, dict[str, Any]],
    verbose: bool,
) -> tuple[bool, list[int]]:
    changed = False
    changed_skill_ids: list[int] = []
    for skill_id, cfg in compact_by_skill.items():
        skill = entries_by_skill.get(skill_id)
        if skill is None:
            continue
        contract = _build_contract_for_skill(skill, cfg, verbose)
        if skill.get("skill_contract") != contract:
            skill["skill_contract"] = contract
            changed = True
            changed_skill_ids.append(skill_id)
            if verbose:
                print(
                    f"[DRIFT] skill_id={skill_id} contract block differs from generated result."
                )
    return changed, changed_skill_ids


def _verify_determinism(
    *,
    baseline_skills_doc: dict[str, Any],
    baseline_mastery_doc: dict[str, Any],
    compact_by_skill: dict[int, dict[str, Any]],
) -> None:
    run1_doc = copy.deepcopy(baseline_skills_doc)
    run2_doc = copy.deepcopy(baseline_skills_doc)
    run1_mastery = copy.deepcopy(baseline_mastery_doc)
    run2_mastery = copy.deepcopy(baseline_mastery_doc)
    _apply_contracts(
        entries_by_skill=_build_contract_entry_index(run1_doc, run1_mastery),
        compact_by_skill=compact_by_skill,
        verbose=False,
    )
    _apply_contracts(
        entries_by_skill=_build_contract_entry_index(run2_doc, run2_mastery),
        compact_by_skill=compact_by_skill,
        verbose=False,
    )
    if _serialize_json(run1_doc) != _serialize_json(run2_doc) or _serialize_json(
        run1_mastery
    ) != _serialize_json(run2_mastery):
        raise ValueError(
            "determinism check failed: same input produced different output"
        )


def _verify_idempotency(
    *,
    normalized_skills_doc: dict[str, Any],
    normalized_mastery_doc: dict[str, Any],
    compact_by_skill: dict[int, dict[str, Any]],
) -> None:
    second_pass_doc = copy.deepcopy(normalized_skills_doc)
    second_pass_mastery = copy.deepcopy(normalized_mastery_doc)
    changed, _ = _apply_contracts(
        entries_by_skill=_build_contract_entry_index(
            second_pass_doc, second_pass_mastery
        ),
        compact_by_skill=compact_by_skill,
        verbose=False,
    )
    if changed:
        raise ValueError(
            "idempotency check failed: second generation pass still modified output"
        )


def _build_contract_entry_index(
    skills_doc: dict[str, Any],
    mastery_doc: dict[str, Any],
) -> dict[int, dict[str, Any]]:
    index: dict[int, dict[str, Any]] = {}
    for skill in skills_doc.get("skills", []):
        skill_id = _read_int(skill.get("id"), "skills[].id")
        if skill.get("talent_tree") is not None:
            index[skill_id] = skill
    for skill in mastery_doc.get("skills", []):
        skill_id = _read_int(skill.get("skill_id"), "mastery.skills[].skill_id")
        index[skill_id] = skill
    return index


def generate(
    skills_path: Path,
    mastery_skills_path: Path,
    compact_path: Path,
    check_only: bool,
    verbose: bool,
    check_idempotency: bool,
    check_determinism: bool,
) -> int:
    skills_doc = _load_json(skills_path)
    mastery_skills_doc = _load_json(mastery_skills_path)
    compact_doc = _load_json(compact_path)
    _validate_skills_doc(skills_doc, skills_path)
    base_skill_ids = {
        _read_int(skill.get("id"), "skills[].id")
        for skill in _require_list(skills_doc.get("skills"), f"{skills_path}:skills")
    }
    _validate_mastery_skills_doc(
        mastery_skills_doc, mastery_skills_path, base_skill_ids
    )
    _validate_compact_doc(compact_doc, compact_path)

    compact_by_skill = {
        _read_int(s["skill_id"], "compact.skills[].skill_id"): _require_object(
            s, "compact.skills[]"
        )
        for s in compact_doc.get("skills", [])
    }
    if verbose:
        print(
            f"[INFO] Loaded {len(skills_doc.get('skills', []))} skills, "
            f"{len(compact_by_skill)} compact configs."
        )

    baseline_skills_doc = copy.deepcopy(_require_object(skills_doc, "skills_doc"))
    baseline_mastery_skills_doc = copy.deepcopy(
        _require_object(mastery_skills_doc, "mastery_skills_doc")
    )
    changed, changed_skill_ids = _apply_contracts(
        entries_by_skill=_build_contract_entry_index(skills_doc, mastery_skills_doc),
        compact_by_skill=compact_by_skill,
        verbose=verbose,
    )

    if check_determinism:
        _verify_determinism(
            baseline_skills_doc=baseline_skills_doc,
            baseline_mastery_doc=baseline_mastery_skills_doc,
            compact_by_skill=compact_by_skill,
        )
        if verbose:
            print("[OK] Determinism check passed.")

    if check_idempotency:
        _verify_idempotency(
            normalized_skills_doc=skills_doc,
            normalized_mastery_doc=mastery_skills_doc,
            compact_by_skill=compact_by_skill,
        )
        if verbose:
            print("[OK] Idempotency check passed.")

    if check_only:
        if changed:
            if verbose and changed_skill_ids:
                changed_list = ", ".join(str(v) for v in changed_skill_ids)
                print(f"[INFO] Out-of-date skill contracts: {changed_list}")
            print(
                "[FAIL] skill_contract blocks are out of date. Run generator without --check."
            )
            return 1
        print("[OK] skill_contract blocks are up to date.")
        return 0

    if changed:
        skills_path.write_text(
            _serialize_json(skills_doc),
            encoding="utf-8",
        )
        mastery_skills_path.write_text(
            _serialize_json(mastery_skills_doc),
            encoding="utf-8",
        )
        if verbose:
            changed_list = ", ".join(str(v) for v in changed_skill_ids)
            print(f"[INFO] Updated contracts for skill_id(s): {changed_list}")
        print(f"[OK] Updated {skills_path}")
        print(f"[OK] Updated {mastery_skills_path}")
    else:
        print("[OK] No changes required.")
    return 0


# ---------------------------------------------------------------------------
# A-01 Phase 3：SpecState 生成器
#
# 输入源为混合方案（D9 §9 R2 裁定）：
#   - 结构/拓扑：talent_tree + skill_contracts_compact.json + skill_mechanics.json
#   - 命名：assets/data/skill_specstate/<skill>.json（本文件只承载「节点 id -> 标识符」）
# 产物：behaviors/generated/<PascalSkill>SpecState.gen.hpp（POD + 绑定表，无 MECH）。
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class SpecStateNode:
    node_id: int
    identifier: str


@dataclass(frozen=True)
class SpecStateBinding:
    node_id: int
    member: str
    kind: str


@dataclass(frozen=True)
class SpecStateModel:
    skill_id: int
    pascal_skill: str
    state_name: str
    nodes: tuple[SpecStateNode, ...]
    point_bindings: tuple[SpecStateBinding, ...]
    flag_bindings: tuple[SpecStateBinding, ...]
    source_path: Path

    @property
    def output_name(self) -> str:
        return f"{self.pascal_skill}SpecState.gen.hpp"


def _load_specstate_model(path: Path) -> SpecStateModel:
    doc = _require_object(_load_json(path), str(path))
    version = _read_int(doc.get("version"), f"{path}:version")
    if version != SPECSTATE_DESCRIPTOR_VERSION:
        raise ValueError(
            f"{path}: version must be {SPECSTATE_DESCRIPTOR_VERSION}, got {version}"
        )
    skill_id = _read_int(doc.get("skill_id"), f"{path}:skill_id")
    pascal_skill = doc.get("pascal_skill")
    if not isinstance(pascal_skill, str) or not IDENTIFIER_RE.match(pascal_skill):
        raise ValueError(f"{path}: pascal_skill must be a valid C++ identifier")
    state_name = doc.get("state_name")
    if not isinstance(state_name, str) or not IDENTIFIER_RE.match(state_name):
        raise ValueError(f"{path}: state_name must be a valid C++ identifier")

    nodes: list[SpecStateNode] = []
    for raw in _require_list(doc.get("nodes"), f"{path}:nodes"):
        obj = _require_object(raw, f"{path}:nodes[]")
        node_id = _read_int(obj.get("node_id"), f"{path}:nodes[].node_id")
        identifier = obj.get("identifier")
        if not isinstance(identifier, str) or not IDENTIFIER_RE.match(identifier):
            raise ValueError(
                f"{path}: node {node_id} has invalid identifier {identifier!r}"
            )
        nodes.append(SpecStateNode(node_id, identifier))

    point_bindings: list[SpecStateBinding] = []
    flag_bindings: list[SpecStateBinding] = []
    for raw in _require_list(doc.get("bindings", []), f"{path}:bindings"):
        obj = _require_object(raw, f"{path}:bindings[]")
        node_id = _read_int(obj.get("node_id"), f"{path}:bindings[].node_id")
        member = obj.get("member")
        if not isinstance(member, str) or not IDENTIFIER_RE.match(member):
            raise ValueError(f"{path}: binding {node_id} has invalid member {member!r}")
        kind = obj.get("kind")
        if kind not in SPECSTATE_KINDS:
            raise ValueError(
                f"{path}: binding {node_id} kind must be point/flag, got {kind!r}"
            )
        binding = SpecStateBinding(node_id, member, kind)
        if kind == SPECSTATE_KIND_POINT:
            point_bindings.append(binding)
        else:
            flag_bindings.append(binding)

    return SpecStateModel(
        skill_id=skill_id,
        pascal_skill=pascal_skill,
        state_name=state_name,
        nodes=tuple(nodes),
        point_bindings=tuple(point_bindings),
        flag_bindings=tuple(flag_bindings),
        source_path=path,
    )


def _collect_talent_entries(
    skills_doc: Any, mastery_doc: Any
) -> list[tuple[int, dict[str, Any]]]:
    entries: list[tuple[int, dict[str, Any]]] = []
    skills = _require_object(skills_doc, "skills")
    for entry in _require_list(skills.get("skills"), "skills.skills"):
        if isinstance(entry, dict) and "talent_tree" in entry and "id" in entry:
            entries.append((_read_int(entry["id"], "skills.skills[].id"), entry))
    mastery = _require_object(mastery_doc, "mastery")
    for entry in _require_list(mastery.get("skills"), "mastery.skills"):
        if isinstance(entry, dict) and "talent_tree" in entry and "skill_id" in entry:
            entries.append(
                (_read_int(entry["skill_id"], "mastery.skills[].skill_id"), entry)
            )
    return entries


def _load_mechanics_keys(
    mechanics_path: Path, schema_path: Path
) -> tuple[dict[str, Any], set[tuple[int, int, str]]]:
    mechanics_doc = _require_object(_load_json(mechanics_path), str(mechanics_path))
    schema_doc = _require_object(_load_json(schema_path), str(schema_path))
    schema_keys: set[tuple[int, int, str]] = set()
    for raw in _require_list(schema_doc.get("entries"), f"{schema_path}:entries"):
        entry = _require_list(raw, f"{schema_path}:entries[]")
        if len(entry) != 3:
            raise ValueError(f"{schema_path}: entries[] must be [skill_id, node_id, key]")
        schema_keys.add(
            (
                _read_int(entry[0], f"{schema_path}:entries[][0]"),
                _read_int(entry[1], f"{schema_path}:entries[][1]"),
                str(entry[2]),
            )
        )
    return mechanics_doc, schema_keys


def _validate_specstate_model(
    model: SpecStateModel,
    entry: dict[str, Any],
    mech_keys: dict[int, tuple[str, ...]],
    schema_keys: set[tuple[int, int, str]],
) -> None:
    talent_nodes: dict[int, Any] = {}
    for raw in _require_list(entry.get("talent_tree"), f"skill {model.skill_id} talent_tree"):
        node = _require_object(raw, f"skill {model.skill_id} talent_tree[]")
        talent_nodes[_read_int(node.get("id"), "talent_tree[].id")] = node

    actual = [node.node_id for node in model.nodes]
    identifiers = [node.identifier for node in model.nodes]
    if len(set(actual)) != len(actual):
        raise ValueError(f"{model.source_path}: duplicate node_id in nodes")
    if len(set(identifiers)) != len(identifiers):
        raise ValueError(f"{model.source_path}: duplicate identifier in nodes")

    expected = set(talent_nodes)
    actual_set = set(actual)
    missing = sorted(expected - actual_set)
    extra = sorted(actual_set - expected)
    if missing or extra:
        raise ValueError(
            f"{model.source_path}: node set differs from talent_tree; "
            f"missing={missing} extra={extra}"
        )

    roles: dict[int, str] = {}
    transmuters: set[int] = set()
    contract = entry.get("skill_contract")
    if isinstance(contract, dict):
        for raw in _require_list(contract.get("nodes", []), "contract.nodes"):
            node = _require_object(raw, "contract.nodes[]")
            roles[_read_int(node.get("node_id"), "contract.nodes[].node_id")] = str(
                node.get("role", "")
            )
        for raw in _require_list(
            contract.get("transmuter_node_ids", []), "contract.transmuter_node_ids"
        ):
            transmuters.add(_read_int(raw, "contract.transmuter_node_ids[]"))

    bound_nodes: set[int] = set()
    members: list[str] = []
    for binding in model.point_bindings + model.flag_bindings:
        if binding.node_id not in actual_set:
            raise ValueError(
                f"{model.source_path}: binding targets unnamed node {binding.node_id}"
            )
        if (
            binding.node_id in transmuters or roles.get(binding.node_id) == "Transmuter"
        ) and binding.kind != SPECSTATE_KIND_FLAG:
            # 转质节点为 0/1 选择态，只能以 HasNode（flag）方式探测；
            # skill 10 在绑定循环外经 GetActiveTransmuterNode 处理，skill 12 走 flag 绑定，两种约定都合法。
            raise ValueError(
                f"{model.source_path}: transmuter node {binding.node_id} must be a flag "
                "binding (selected state), not a point binding"
            )
        if binding.node_id in bound_nodes:
            raise ValueError(f"{model.source_path}: node {binding.node_id} bound twice")
        bound_nodes.add(binding.node_id)
        members.append(binding.member)
    if len(set(members)) != len(members):
        raise ValueError(f"{model.source_path}: duplicate member in bindings")

    # 机制键闭合：源码读取的每个 (skill, node, key) 必须在 skill_mechanics.json 中有定义。
    # 数据中的未引用键由 gen_skill_mechanics_schema.py 单独报告，不在本校验范围。
    for schema_skill, node_id, key in sorted(schema_keys):
        if schema_skill != model.skill_id or node_id not in talent_nodes:
            continue
        if key not in mech_keys.get(node_id, ()):
            raise ValueError(
                f"{model.source_path}: skill_mechanics.json missing key "
                f"(skill={model.skill_id}, node={node_id}, key={key})"
            )


def _render_specstate_header(model: SpecStateModel) -> str:
    struct = f"{model.state_name}Gen"
    namespace = f"{model.pascal_skill}NodesGen"
    point_array = f"k{model.pascal_skill}PointBindingsGen"
    flag_array = f"k{model.pascal_skill}FlagBindingsGen"
    table = f"k{model.pascal_skill}TableGen"
    identifier_by_node = {node.node_id: node.identifier for node in model.nodes}

    lines: list[str] = []
    lines.append("// " + SPECSTATE_BANNER)
    lines.append(
        f"// 技能 {model.skill_id} 效果层 SpecState（POD + 绑定表），由命名 descriptor 驱动。"
    )
    lines.append(
        "// 输入：assets/data/skill_specstate/"
        f"skill_{model.skill_id:02d}.json + talent_tree 结构。"
    )
    lines.append("#pragma once")
    lines.append("")
    lines.append('#include "game/systems/skill/SpecStateTable.hpp"')
    lines.append("")
    lines.append("#include <array>")
    lines.append("#include <cstdint>")
    lines.append("")
    lines.append("namespace NoMoreDay::skills {")
    lines.append("")
    lines.append(f"// 技能 {model.skill_id} 节点常量（talent_tree 全集，节点 id 升序）。")
    lines.append("namespace " + namespace + " {")
    for node in model.nodes:
        lines.append(f"constexpr uint32_t {node.identifier} = {node.node_id};")
    lines.append("}  // namespace " + namespace)
    lines.append("")
    lines.append(f"// 技能 {model.skill_id} 效果层 SpecState（POD，A-01 D-A1）。")
    lines.append("struct " + struct + " {")
    lines.append("  using PointBinding = SpecPointBinding<" + struct + ">;")
    lines.append("  using FlagBinding = SpecFlagBinding<" + struct + ">;")
    lines.append("")
    for binding in model.point_bindings:
        lines.append(f"  int {binding.member} = 0;")
    for binding in model.flag_bindings:
        lines.append(f"  bool {binding.member} = false;")
    lines.append("};")
    lines.append("")

    lines.append(
        "inline constexpr std::array<"
        + struct
        + "::PointBinding, "
        + str(len(model.point_bindings))
        + "> "
        + point_array
        + "{{"
    )
    for binding in model.point_bindings:
        lines.append(
            "    {"
            + namespace
            + "::"
            + identifier_by_node[binding.node_id]
            + ", &"
            + struct
            + "::"
            + binding.member
            + "},"
        )
    lines.append("}};")
    lines.append("")

    lines.append(
        "inline constexpr std::array<"
        + struct
        + "::FlagBinding, "
        + str(len(model.flag_bindings))
        + "> "
        + flag_array
        + "{{"
    )
    for binding in model.flag_bindings:
        lines.append(
            "    {"
            + namespace
            + "::"
            + identifier_by_node[binding.node_id]
            + ", &"
            + struct
            + "::"
            + binding.member
            + "},"
        )
    lines.append("}};")
    lines.append("")

    lines.append("inline constexpr SpecStateTable<" + struct + "> " + table + "{")
    lines.append("    " + point_array + ", " + flag_array + "};")
    lines.append("")
    lines.append("}  // namespace NoMoreDay::skills")
    lines.append("")
    return "\n".join(lines)


def _reload_and_render_specstate(model: SpecStateModel) -> str:
    """跨调用确定性基线：从磁盘重新加载 descriptor 后独立渲染。

    与内存中的 model 不是同一实例，且重走 JSON 解析/排序路径；原地对同一对象
    二次渲染无法发现加载或渲染中隐藏的顺序依赖，故以重载渲染做真比较。
    """
    return _render_specstate_header(_load_specstate_model(model.source_path))


def _verify_specstate_determinism(model: SpecStateModel, baseline: str) -> None:
    """断言「重新加载 descriptor 后独立渲染」与基线逐字一致。"""
    if _reload_and_render_specstate(model) != baseline:
        raise ValueError(
            f"{model.source_path}: determinism check failed "
            "(reload from disk rendered differently)"
        )


def _self_test_specstate_determinism(model: SpecStateModel, baseline: str) -> None:
    """负向自检：证明确定性校验不是空转（H-1）。

    以一个被篡改的基线调用校验器，期望其报错；若校验器接受篡改基线，则说明
    比较恒真（等价于空转），此时主动失败。
    """
    tampered = baseline + "\n// determinism self-test sentinel\n"
    try:
        _verify_specstate_determinism(model, tampered)
    except ValueError:
        return
    raise ValueError(
        f"{model.source_path}: determinism self-test failed — the checker accepted "
        "a tampered baseline (vacuous check)"
    )


def generate_specstate(
    skills_path: Path,
    mastery_path: Path,
    descriptor_dir: Path,
    output_dir: Path,
    mechanics_path: Path,
    schema_path: Path,
    check_only: bool,
    verbose: bool,
    check_idempotency: bool,
    check_determinism: bool,
) -> int:
    skills_doc = _load_json(skills_path)
    mastery_doc = _load_json(mastery_path)
    entry_by_skill = {
        skill_id: entry
        for skill_id, entry in _collect_talent_entries(skills_doc, mastery_doc)
    }
    mechanics_doc, schema_keys = _load_mechanics_keys(mechanics_path, schema_path)

    descriptor_paths = sorted(descriptor_dir.glob("skill_*.json"))
    if not descriptor_paths:
        raise ValueError(f"No SpecState descriptor found: {descriptor_dir}/skill_*.json")

    models: list[SpecStateModel] = []
    for path in descriptor_paths:
        model = _load_specstate_model(path)
        if model.skill_id not in entry_by_skill:
            raise ValueError(
                f"{path}: skill_id {model.skill_id} is absent from talent_tree data"
            )
        mech_keys: dict[int, tuple[str, ...]] = {}
        raw_mech = mechanics_doc.get(str(model.skill_id), {})
        if isinstance(raw_mech, dict):
            for node_key, keys in raw_mech.items():
                if isinstance(keys, dict):
                    mech_keys[int(node_key)] = tuple(sorted(keys))
        _validate_specstate_model(
            model, entry_by_skill[model.skill_id], mech_keys, schema_keys
        )
        models.append(model)

    # 技能全覆盖断言（M-2）：descriptor 的 skill_id 集合必须与 talent_tree 全集一致，
    # 且各 descriptor 的 skill_id / pascal_skill 唯一；缺失或多余均报错，覆盖
    # 「descriptor 与生成物同时被删」等静默漏覆盖场景。
    covered_skill_ids = [model.skill_id for model in models]
    if len(set(covered_skill_ids)) != len(covered_skill_ids):
        duplicates = sorted(
            skill_id
            for skill_id in set(covered_skill_ids)
            if covered_skill_ids.count(skill_id) > 1
        )
        raise ValueError(
            f"{descriptor_dir}: duplicate skill_id across descriptors: {duplicates}"
        )
    pascal_names = [model.pascal_skill for model in models]
    if len(set(pascal_names)) != len(pascal_names):
        raise ValueError(
            f"{descriptor_dir}: duplicate pascal_skill across descriptors: "
            f"{sorted(pascal_names)}"
        )
    expected_skill_ids = set(entry_by_skill)
    covered = set(covered_skill_ids)
    missing_descriptors = sorted(expected_skill_ids - covered)
    unknown_descriptors = sorted(covered - expected_skill_ids)
    if missing_descriptors or unknown_descriptors:
        raise ValueError(
            f"{descriptor_dir}: descriptor coverage mismatch; "
            f"missing={missing_descriptors} unknown={unknown_descriptors}"
        )

    if output_dir.exists():
        expected_names = {model.output_name for model in models}
        stale = sorted(
            path.name
            for path in output_dir.glob("*.gen.hpp")
            if path.name not in expected_names
        )
        if stale:
            raise ValueError(
                f"{output_dir}: generated files without a descriptor: {stale}"
            )

    changed: list[str] = []
    for index, model in enumerate(models):
        rendered = _render_specstate_header(model)
        if check_determinism:
            # 跨调用比较：从磁盘重新加载后独立渲染再与基线比对（非原地二次渲染）。
            _verify_specstate_determinism(model, rendered)
            if index == 0:
                # 负向自检只跑一次，证明校验链路确实能捕获差异（H-1 防空转）。
                _self_test_specstate_determinism(model, rendered)
        if check_idempotency:
            reloaded = _load_specstate_model(model.source_path)
            if _render_specstate_header(reloaded) != rendered:
                raise ValueError(f"{model.source_path}: idempotency check failed")

        out_path = output_dir / model.output_name
        current = out_path.read_text(encoding="utf-8") if out_path.exists() else None
        if current == rendered:
            if verbose:
                print(f"[OK] {out_path.relative_to(ROOT)} is up to date.")
            continue
        changed.append(model.output_name)
        if check_only:
            print(f"[FAIL] {out_path.relative_to(ROOT)} differs from descriptor.")
        else:
            output_dir.mkdir(parents=True, exist_ok=True)
            out_path.write_text(rendered, encoding="utf-8")
            print(f"[OK] Generated {out_path.relative_to(ROOT)}")

    if check_only and changed:
        raise ValueError(
            f"SpecState artifacts out of date: {changed}; run --gen-specstate"
        )
    if not changed:
        print("[OK] SpecState artifacts unchanged.")
    return 0


def main() -> int:
    if sys.version_info < MIN_PYTHON:
        required = ".".join(str(v) for v in MIN_PYTHON)
        actual = f"{sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}"
        raise SystemExit(
            f"[FAIL] Python {required}+ is required, current version is {actual}."
        )

    parser = argparse.ArgumentParser(
        description="Generate specialization contracts for skill data tables"
    )
    parser.add_argument("--skills", type=Path, default=DEFAULT_SKILLS_JSON)
    parser.add_argument(
        "--mastery-skills", type=Path, default=DEFAULT_MASTERY_SKILLS_JSON
    )
    parser.add_argument("--compact", type=Path, default=DEFAULT_COMPACT_JSON)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument("--check-idempotency", action="store_true")
    parser.add_argument("--check-determinism", action="store_true")
    parser.add_argument("--gen-specstate", action="store_true")
    parser.add_argument(
        "--specstate-descriptors",
        type=Path,
        default=DEFAULT_SPECSTATE_DESCRIPTOR_DIR,
    )
    parser.add_argument(
        "--specstate-out",
        type=Path,
        default=DEFAULT_SPECSTATE_OUTPUT_DIR,
    )
    args = parser.parse_args()

    try:
        rc = generate(
            args.skills.resolve(),
            args.mastery_skills.resolve(),
            args.compact.resolve(),
            args.check,
            args.verbose,
            args.check_idempotency,
            args.check_determinism,
        )
        if args.gen_specstate:
            rc = rc or generate_specstate(
                args.skills.resolve(),
                args.mastery_skills.resolve(),
                args.specstate_descriptors.resolve(),
                args.specstate_out.resolve(),
                DEFAULT_MECHANICS_JSON,
                DEFAULT_MECHANICS_SCHEMA_JSON,
                args.check,
                args.verbose,
                args.check_idempotency,
                args.check_determinism,
            )
        return rc
    except ValueError as exc:
        print(f"[FAIL] {exc}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
