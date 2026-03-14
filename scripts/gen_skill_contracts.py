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
import json
from pathlib import Path
import sys
from typing import Any

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SKILLS_JSON = ROOT / "assets" / "data" / "skills.json"
DEFAULT_MASTERY_SKILLS_JSON = ROOT / "assets" / "data" / "mastery_skill_trees.json"
DEFAULT_COMPACT_JSON = ROOT / "assets" / "data" / "skill_contracts_compact.json"
MIN_PYTHON = (3, 10)

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
        }
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
    exclusion_group_counts: dict[int, int] = {}
    for node_id in sorted(nodes_by_id.keys()):
        node = nodes_by_id[node_id]
        max_points = _read_int(
            node.get("max_points", 1), f"skill {skill_id}.node {node_id}.max_points"
        )

        role = ROLE_PASSIVE
        if node_id in explicit_passive_ids:
            role = ROLE_PASSIVE
        elif node_id in trigger_nodes:
            role = ROLE_TRIGGER
        elif node_id in transmuter_ids:
            role = ROLE_TRANSMUTER
        elif node_id in synergy_ids:
            role = ROLE_SYNERGY
        elif node_id in explicit_keystone_ids or max_points == 1:
            role = ROLE_KEYSTONE

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
            },
        )

        is_default_trigger = (
            trigger["trigger_skill_id"] == 0
            and trigger["effectiveness"] == 0.0
            and trigger["range_mult"] == 1.0
            and trigger["internal_cooldown"] == 0.0
            and trigger["consumes_mana"] is False
        )

        emits_non_default = (
            role != ROLE_PASSIVE
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

        if keystone_exclusion_group != 0:
            exclusion_group_counts[keystone_exclusion_group] = (
                exclusion_group_counts.get(keystone_exclusion_group, 0) + 1
            )

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

    for exclusion_group_id, exclusion_count in exclusion_group_counts.items():
        if exclusion_count < 2:
            raise ValueError(
                f"skill {skill_id}: keystone_exclusion_group {exclusion_group_id} has fewer than 2 nodes"
            )

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
    args = parser.parse_args()

    try:
        return generate(
            args.skills.resolve(),
            args.mastery_skills.resolve(),
            args.compact.resolve(),
            args.check,
            args.verbose,
            args.check_idempotency,
            args.check_determinism,
        )
    except ValueError as exc:
        print(f"[FAIL] {exc}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
