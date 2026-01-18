
import json
import os

RUNE_FILE = "assets/data/runes.json"
RUNEWORDS_FILE = "assets/data/runewords.json"

# ID mapping: JSON ID (3001-3033) -> Chinese
NAME_MAPPING = {
    3001: "曾", 3002: "完", 3003: "童", 3004: "育", 3005: "举",
    3006: "夷", 3007: "衡", 3008: "翳", 3009: "阳", 3010: "华", 3011: "飘",
    3012: "竺", 3013: "禁", 3014: "崖", 3015: "答", 3016: "霄",
    3017: "翁", 3018: "极", 3019: "晖", 3020: "密", 3021: "耀", 3022: "慧",
    3023: "迦", 3024: "昙", 3025: "虚", 3026: "演", 3027: "梵",
    3028: "腾", 3029: "显", 3030: "秀", 3031: "清", 3032: "灵", 3033: "宝"
}

# English -> Chinese Mapping (Standard D2 Names -> Design Doc Names)
ENGLISH_TO_CHINESE = {
    "El": "曾", "Eld": "完", "Tir": "童", "Nef": "育", "Eth": "举",
    "Ith": "夷", "Tal": "衡", "Ral": "翳", "Ort": "阳", "Thul": "华", "Amn": "飘",
    "Sol": "竺", "Shael": "禁", "Dol": "崖", "Hel": "答", "Io": "霄",
    "Lum": "翁", "Ko": "极", "Fal": "晖", "Lem": "密", "Pul": "耀", "Um": "慧",
    "Mal": "迦", "Ist": "昙", "Gul": "虚", "Vex": "演", "Ohm": "梵",
    "Lo": "腾", "Sur": "显", "Ber": "秀", "Jah": "清", "Cham": "灵", "Zod": "宝"
}

def update_runes():
    # 1. Update Runes.json (if not already done, or ensures consistency)
    if os.path.exists(RUNE_FILE):
        try:
            with open(RUNE_FILE, 'r', encoding='utf-8') as f:
                data = json.load(f)
            
            runes = data.get("runes", [])
            for rune in runes:
                rune_id = rune.get("id")
                if rune_id in NAME_MAPPING:
                    rune["name"] = NAME_MAPPING[rune_id]

            with open(RUNE_FILE, 'w', encoding='utf-8') as f:
                json.dump(data, f, indent=4, ensure_ascii=False)
            print(f"Verified/Updated {RUNE_FILE}")
        except Exception as e:
            print(f"Error updating runes: {e}")

    # 2. Update Runewords.json
    if os.path.exists(RUNEWORDS_FILE):
        try:
            with open(RUNEWORDS_FILE, 'r', encoding='utf-8') as f:
                data = json.load(f)
            
            runewords = data.get("runewords", [])
            updated_count = 0
            
            for rw in runewords:
                runes_list = rw.get("runes", [])
                new_list = []
                changed = False
                for rName in runes_list:
                    if rName in ENGLISH_TO_CHINESE:
                        new_list.append(ENGLISH_TO_CHINESE[rName])
                        changed = True
                    else:
                        # Already Chinese or unknown?
                        if rName in ENGLISH_TO_CHINESE.values():
                             new_list.append(rName) # Already Chinese
                        else:
                             print(f"Warning: Unknown rune name '{rName}' in runeword '{rw.get('name')}'")
                             new_list.append(rName)
                
                if changed:
                    rw["runes"] = new_list
                    updated_count += 1

            with open(RUNEWORDS_FILE, 'w', encoding='utf-8') as f:
                json.dump(data, f, indent=4, ensure_ascii=False)
            print(f"Updated {updated_count} runewords in {RUNEWORDS_FILE}")
            
        except Exception as e:
            print(f"Error updating runewords: {e}")
    else:
        print(f"Error: {RUNEWORDS_FILE} not found.")

if __name__ == "__main__":
    update_runes()
