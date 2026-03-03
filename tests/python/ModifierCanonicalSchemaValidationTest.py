import json
import sys
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SCRIPTS_DIR = REPO_ROOT / "scripts"
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))

import validate_canonical_schema  # noqa: E402


SCHEMA_PATH = (
    REPO_ROOT
    / "assets"
    / "data"
    / "modifier_v2"
    / "canonical"
    / "skill_spec_modifier_record.schema.json"
)
FIXTURE_DIR = REPO_ROOT / "tests" / "fixtures" / "schema" / "modifier"


def _load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


class ModifierCanonicalSchemaValidationTest(unittest.TestCase):
    def test_accepts_valid_fixture(self) -> None:
        schema = _load_json(SCHEMA_PATH)
        payload = _load_json(FIXTURE_DIR / "skill_spec_modifier_record.valid.json")

        errors = validate_canonical_schema.validate_instance(
            schema=schema, instance=payload
        )

        self.assertEqual(errors, [])

    def test_rejects_additional_properties(self) -> None:
        schema = _load_json(SCHEMA_PATH)
        payload = _load_json(
            FIXTURE_DIR / "skill_spec_modifier_record.invalid.extra_property.json"
        )

        errors = validate_canonical_schema.validate_instance(
            schema=schema, instance=payload
        )

        self.assertTrue(
            any("unexpected property 'debug_note'" in error for error in errors)
        )

    def test_rejects_constraint_violations(self) -> None:
        schema = _load_json(SCHEMA_PATH)
        payload = _load_json(
            FIXTURE_DIR / "skill_spec_modifier_record.invalid.constraints.json"
        )

        errors = validate_canonical_schema.validate_instance(
            schema=schema, instance=payload
        )

        self.assertTrue(any("modifier_id" in error for error in errors))
        self.assertTrue(any("operation" in error for error in errors))
        self.assertTrue(any("stat_path" in error for error in errors))
        self.assertTrue(any("stacks" in error for error in errors))
        self.assertTrue(any("tags" in error for error in errors))
        self.assertTrue(any("all_skill_ids" in error for error in errors))
        self.assertTrue(any("min_player_level" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
