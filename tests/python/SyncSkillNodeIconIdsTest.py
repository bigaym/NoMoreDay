import json
import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SCRIPTS_DIR = REPO_ROOT / "scripts"
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))

import sync_skill_node_icon_ids  # noqa: E402


def _write_json(path: Path, payload: dict) -> None:
    path.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )


def _read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


class SyncSkillNodeIconIdsTest(unittest.TestCase):
    def test_default_json_files_include_mastery_tree_document(self) -> None:
        self.assertEqual(
            sync_skill_node_icon_ids.DEFAULT_JSON_FILES,
            (
                REPO_ROOT / "assets" / "data" / "skills.json",
                REPO_ROOT / "assets" / "data" / "mastery_skill_trees.json",
            ),
        )

    def test_sync_json_files_updates_regular_and_mastery_documents(self) -> None:
        icon_hashes = {100: 1111, 1000: 2222}

        with tempfile.TemporaryDirectory() as tmp:
            tmp_dir = Path(tmp)
            regular_path = tmp_dir / "skills.json"
            mastery_path = tmp_dir / "mastery_skill_trees.json"

            _write_json(
                regular_path,
                {
                    "skills": [
                        {
                            "id": 1,
                            "talent_tree": [
                                {"id": 100, "icon_id": 100},
                            ],
                        }
                    ]
                },
            )
            _write_json(
                mastery_path,
                {
                    "skills": [
                        {
                            "skill_id": 10,
                            "mastery_id": "sword_saint",
                            "talent_tree": [
                                {"id": 1000, "icon_id": 1000},
                            ],
                        }
                    ]
                },
            )

            exit_code = sync_skill_node_icon_ids.sync_json_files(
                (regular_path, mastery_path),
                icon_hashes,
                check_only=False,
            )

            self.assertEqual(exit_code, 0)
            self.assertEqual(
                _read_json(regular_path)["skills"][0]["talent_tree"][0]["icon_id"],
                1111,
            )
            self.assertEqual(
                _read_json(mastery_path)["skills"][0]["talent_tree"][0]["icon_id"],
                2222,
            )


if __name__ == "__main__":
    unittest.main()
