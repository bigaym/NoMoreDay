"""
Generate skill_contract blocks in assets/data/skills.json from a compact config.

Usage:
  python scripts/gen_skill_contracts.py
  python scripts/gen_skill_contracts.py --check
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SKILLS_JSON = ROOT / "assets" / "data" / "skills.json"
DEFAULT_COMPACT_JSON = ROOT / "assets" / "data" / "skill_contracts_compact.json"

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


def _load_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def _as_int_set(values: list[Any]) -> set[int]:
    return {int(v) for v in values}


def _validate_enum_set(name: str, values: dict[str, str], allowed: set[str]) -> None:
    for node_id, value in values.items():
        if value not in allowed:
            raise ValueError(f"{name}[{node_id}] has invalid value: {value}")


def _build_contract_for_skill(skill: dict[str, Any], cfg: dict[str, Any]) -> dict[str, Any]:
    skill_id = int(skill["id"])
    if skill_id != int(cfg["skill_id"]):
        raise ValueError("skill_id mismatch between skill and compact config")

    talent_tree = skill.get("talent_tree", [])
    nodes_by_id = {int(n["id"]): n for n in talent_tree}
    if not nodes_by_id:
        raise ValueError(f"skill {skill_id} has no talent_tree nodes")

    min_nodes = int(cfg.get("min_nodes", len(nodes_by_id)))
    max_nodes = int(cfg.get("max_nodes", len(nodes_by_id)))
    max_transmuters = int(cfg.get("max_transmuters", 2))
    max_triggers = int(cfg.get("max_triggers", 1))
    has_sword_intent_node = bool(cfg.get("has_sword_intent_node", True))
    has_synergy_node = bool(cfg.get("has_synergy_node", True))

    transmuter_ids = [int(v) for v in cfg.get("transmuter_node_ids", [])][:2]
    while len(transmuter_ids) < 2:
        transmuter_ids.append(0)

    synergy_ids = _as_int_set(cfg.get("synergy_node_ids", []))
    sword_intent_ids = _as_int_set(cfg.get("sword_intent_node_ids", []))
    sword_step_ids = _as_int_set(cfg.get("sword_step_node_ids", []))
    explicit_keystone_ids = _as_int_set(cfg.get("keystone_node_ids", []))
    explicit_passive_ids = _as_int_set(cfg.get("passive_node_ids", []))

    trigger_nodes_cfg = cfg.get("trigger_nodes", [])
    trigger_nodes: dict[int, dict[str, Any]] = {}
    for t in trigger_nodes_cfg:
        node_id = int(t["node_id"])
        trigger_nodes[node_id] = {
            "trigger_skill_id": int(t.get("trigger_skill_id", 0)),
            "effectiveness": float(t.get("effectiveness", 0.0)),
            "internal_cooldown": float(t.get("internal_cooldown", 0.0)),
            "consumes_mana": bool(t.get("consumes_mana", False)),
        }

    resist_models = {str(k): str(v) for k, v in cfg.get("resist_models", {}).items()}
    scope_policies = {str(k): str(v) for k, v in cfg.get("scope_policies", {}).items()}
    _validate_enum_set("resist_models", resist_models, VALID_RESIST)
    _validate_enum_set("scope_policies", scope_policies, VALID_SCOPE)

    node_contracts: list[dict[str, Any]] = []
    for node_id in sorted(nodes_by_id.keys()):
        node = nodes_by_id[node_id]
        max_points = int(node.get("max_points", 1))

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
        trigger = trigger_nodes.get(
            node_id,
            {
                "trigger_skill_id": 0,
                "effectiveness": 0.0,
                "internal_cooldown": 0.0,
                "consumes_mana": False,
            },
        )

        is_default_trigger = (
            trigger["trigger_skill_id"] == 0
            and trigger["effectiveness"] == 0.0
            and trigger["internal_cooldown"] == 0.0
            and trigger["consumes_mana"] is False
        )

        emits_non_default = (
            role != ROLE_PASSIVE
            or resist_model != RESIST_NONE
            or scope_policy != SCOPE_SKILL_ONLY
            or (node_id in sword_intent_ids)
            or (node_id in sword_step_ids)
            or (not is_default_trigger)
        )
        if not emits_non_default:
            continue

        node_contracts.append(
            {
                "node_id": node_id,
                "role": role,
                "resist_model": resist_model,
                "scope_policy": scope_policy,
                "affects_sword_intent": node_id in sword_intent_ids,
                "affects_sword_step": node_id in sword_step_ids,
                "trigger": trigger,
            }
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


def generate(skills_path: Path, compact_path: Path, check_only: bool) -> int:
    skills_doc = _load_json(skills_path)
    compact_doc = _load_json(compact_path)
    compact_by_skill = {int(s["skill_id"]): s for s in compact_doc.get("skills", [])}

    changed = False
    for skill in skills_doc.get("skills", []):
        skill_id = int(skill["id"])
        if skill_id not in compact_by_skill:
            continue

        contract = _build_contract_for_skill(skill, compact_by_skill[skill_id])
        if skill.get("skill_contract") != contract:
            skill["skill_contract"] = contract
            changed = True

    if check_only:
        if changed:
            print("[FAIL] skill_contract blocks are out of date. Run generator without --check.")
            return 1
        print("[OK] skill_contract blocks are up to date.")
        return 0

    if changed:
        skills_path.write_text(
            json.dumps(skills_doc, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )
        print(f"[OK] Updated {skills_path}")
    else:
        print("[OK] No changes required.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate skills.json skill contracts")
    parser.add_argument("--skills", type=Path, default=DEFAULT_SKILLS_JSON)
    parser.add_argument("--compact", type=Path, default=DEFAULT_COMPACT_JSON)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    return generate(args.skills.resolve(), args.compact.resolve(), args.check)


if __name__ == "__main__":
    raise SystemExit(main())
