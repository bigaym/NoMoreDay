
def fnv1a_32(string):
    hash_val = 0x811c9dc5
    for char in string:
        hash_val ^= ord(char)
        hash_val = (hash_val * 0x01000193) & 0xFFFFFFFF
    return hash_val

print(f"ui_skill_icon_1: {fnv1a_32('ui_skill_icon_1')}")
print(f"ui_skill_icon_2: {fnv1a_32('ui_skill_icon_2')}")
