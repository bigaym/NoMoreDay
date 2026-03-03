import sys
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SCRIPTS_DIR = REPO_ROOT / "scripts"
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))

import migrate_vfx_sequence_schema  # noqa: E402


class MigrateVfxSequenceSchemaTest(unittest.TestCase):
    def test_migrates_v1_document_to_v3(self) -> None:
        source = {
            "vfx_schema_version": 1,
            "name": "LegacySlash",
            "duration": 0.5,
            "events": [
                {
                    "time": 0.0,
                    "type": "shadow_pulse",
                    "params": {
                        "softnessScale": 1.2,
                        "intensityScale": 1.1,
                        "duration": 0.3,
                    },
                },
                {
                    "time": 0.2,
                    "type": "Particle",
                    "tierPolicy": "strict",
                    "params": {"materialId": "FireGlow", "count": 4},
                },
            ],
        }

        migrated = migrate_vfx_sequence_schema.migrate_document(source)

        self.assertEqual(migrated["vfx_schema_version"], 3)
        self.assertEqual(migrated["events"][0]["type"], "ShadowPulse")
        self.assertEqual(migrated["events"][0]["tierPolicy"], "skip")
        self.assertEqual(migrated["events"][1]["tierPolicy"], "strict")

    def test_rejects_unsupported_legacy_schema_header(self) -> None:
        source = {
            "schema_version": "legacy-v0",
            "name": "TooOld",
            "events": [],
        }

        with self.assertRaisesRegex(ValueError, "unsupported legacy format"):
            migrate_vfx_sequence_schema.migrate_document(source)


if __name__ == "__main__":
    unittest.main()
