import json
import sys
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SCRIPTS_DIR = REPO_ROOT / "scripts"
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))

import migrate_skill_spec_modifier_slice  # noqa: E402


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


class SkillSpecModifierMigrationTest(unittest.TestCase):
    def test_migrates_supported_runtime_record_to_canonical(self) -> None:
        schema = _load_json(SCHEMA_PATH)
        runtime_doc = _load_json(FIXTURE_DIR / "skill_spec_runtime.supported.json")

        canonical_doc, dropped = (
            migrate_skill_spec_modifier_slice.migrate_runtime_document(
                schema=schema,
                runtime_doc=runtime_doc,
            )
        )

        self.assertEqual(len(dropped), 0)
        self.assertEqual(canonical_doc["schema_version"], 1)
        self.assertEqual(len(canonical_doc["records"]), 1)

        record = canonical_doc["records"][0]["record"]
        self.assertEqual(record["modifier_id"], 2002103)
        self.assertEqual(record["domain"], "skill_spec")
        self.assertEqual(record["operation"], "mul")
        self.assertEqual(record["stat_path"], "damage")
        self.assertEqual(record["stacks"], 9)
        self.assertEqual(record["conditions"]["all_skill_ids"], [2])

    def test_drops_unsupported_runtime_record_with_reason(self) -> None:
        schema = _load_json(SCHEMA_PATH)
        runtime_doc = _load_json(FIXTURE_DIR / "skill_spec_runtime.unsupported.json")

        canonical_doc, dropped = (
            migrate_skill_spec_modifier_slice.migrate_runtime_document(
                schema=schema,
                runtime_doc=runtime_doc,
            )
        )

        self.assertEqual(len(canonical_doc["records"]), 0)
        self.assertEqual(len(dropped), 1)
        self.assertTrue(
            any("unsupported opcode" in reason for reason in dropped[0]["reasons"])
        )


if __name__ == "__main__":
    unittest.main()
