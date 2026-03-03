import json
import sys
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SCRIPTS_DIR = REPO_ROOT / "scripts"
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))

import validate_json  # noqa: E402


class ValidateJsonModifierCanonicalArtifactsTest(unittest.TestCase):
    def test_ignores_canonical_schema_artifact_in_modifier_v2(self) -> None:
        schema_path = (
            REPO_ROOT
            / "assets"
            / "data"
            / "modifier_v2"
            / "canonical"
            / "skill_spec_modifier_record.schema.json"
        )
        payload = json.loads(schema_path.read_text(encoding="utf-8"))

        errors = validate_json._validate_modifier_v2(schema_path, payload)

        self.assertEqual(errors, [])


if __name__ == "__main__":
    unittest.main()
