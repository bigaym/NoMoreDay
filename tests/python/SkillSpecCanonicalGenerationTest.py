import json
import sys
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SCRIPTS_DIR = REPO_ROOT / "scripts"
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))

import gen_skill_spec_modifier_contract  # noqa: E402


SCHEMA_PATH = (
    REPO_ROOT
    / "assets"
    / "data"
    / "modifier_v2"
    / "canonical"
    / "skill_spec_modifier_record.schema.json"
)
RUNTIME_OUTPUT_PATH = (
    REPO_ROOT / "assets" / "data" / "modifier_v2" / "skill_spec_modifiers.json"
)
FIXTURE_DIR = REPO_ROOT / "tests" / "fixtures" / "schema" / "modifier"


def _load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


class SkillSpecCanonicalGenerationTest(unittest.TestCase):
    def test_generated_runtime_matches_committed_asset(self) -> None:
        schema = _load_json(SCHEMA_PATH)
        canonical_doc = _load_json(
            FIXTURE_DIR / "skill_spec_modifiers.canonical.runtime.valid.json"
        )

        generated = gen_skill_spec_modifier_contract.generate_runtime_document(
            schema=schema,
            canonical_doc=canonical_doc,
        )
        committed = _load_json(RUNTIME_OUTPUT_PATH)

        self.assertEqual(generated, committed)

    def test_rejects_invalid_canonical_fixture(self) -> None:
        schema = _load_json(SCHEMA_PATH)
        canonical_doc = _load_json(
            FIXTURE_DIR / "skill_spec_modifiers.canonical.runtime.invalid.json"
        )

        with self.assertRaisesRegex(ValueError, r"modifier_id"):
            gen_skill_spec_modifier_contract.generate_runtime_document(
                schema=schema,
                canonical_doc=canonical_doc,
            )


if __name__ == "__main__":
    unittest.main()
