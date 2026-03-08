import json
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SKILLS_JSON = REPO_ROOT / "assets" / "data" / "skills.json"
SKILL_ICON_DIR = REPO_ROOT / "assets" / "textures" / "icons" / "skills"


def _fnv1a_32(text: str) -> int:
    hash_value = 2166136261
    for byte in text.encode("utf-8"):
        hash_value = ((hash_value ^ byte) * 16777619) & 0xFFFFFFFF
    return hash_value


class MasterySkillIconAssetsTest(unittest.TestCase):
    def test_mastery_skills_have_generated_icons_and_hashed_ids(self) -> None:
        expected = {
            10: ("七星斩", "skill_qixingzhan.png", _fnv1a_32("ui_skill_qixingzhan")),
            11: (
                "天剑降临",
                "skill_tianjianjianglin.png",
                _fnv1a_32("ui_skill_tianjianjianglin"),
            ),
            12: ("血海", "skill_xuehai.png", _fnv1a_32("ui_skill_xuehai")),
        }

        payload = json.loads(SKILLS_JSON.read_text(encoding="utf-8"))
        skills_by_id = {entry["id"]: entry for entry in payload["skills"]}

        for skill_id, (name_key, icon_file, icon_hash) in expected.items():
            with self.subTest(skill_id=skill_id, name_key=name_key):
                self.assertIn(skill_id, skills_by_id)
                self.assertEqual(skills_by_id[skill_id]["name_key"], name_key)
                self.assertEqual(skills_by_id[skill_id]["icon_id"], icon_hash)
                self.assertTrue((SKILL_ICON_DIR / icon_file).exists())


if __name__ == "__main__":
    unittest.main()
