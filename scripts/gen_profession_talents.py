import json
import os

# --- Enum Definitions ---
class ProfessionID:
    BladeAscendant = 0

class TalentNodeType:
    Minor = "Minor"
    Major = "Major"
    Core = "Core"

class StatType:
    Strength = 0
    Dexterity = 1
    Intelligence = 2
    Vitality = 3
    MaxHealth = 4
    MaxMana = 5
    MaxBarrier = 6
    MoveSpeed = 7
    Armor = 8
    PhysicalDamage = 9
    FireDamage = 10
    ColdDamage = 11
    LightningDamage = 12
    PoisonDamage = 13
    ShadowDamage = 14
    CritChance = 15
    CritDamage = 16
    AttackSpeed = 17
    CastSpeed = 18
    Accuracy = 19
    ManaOnHit = 20
    ArmorPenetration = 21
    ResistPhysical = 22
    ResistFire = 23
    ResistCold = 24
    ResistLightning = 25
    ResistPoison = 26
    ResistShadow = 27
    ResistAll = 28
    CooldownReduction = 29
    ResourceCostReduction = 30
    ProjectileCount = 31
    AreaScale = 32
    ProjectileSpeed = 33
    DurationScale = 34
    DodgeChance = 35
    BlockChance = 36
    LifeSteal = 37
    LifeOnHit = 38
    HealthRegen = 39
    ManaRegen = 40
    BarrierRegen = 41
    BarrierDecay = 42
    BarrierDelay = 43
    BarrierRetention = 44
    Thorns = 45
    MagicFind = 46
    DodgeRating = 47
    BlockRating = 48
    GlobalDamageReduction = 49

class ModifierMode:
    Flat = 0
    PercentAdd = 1
    PercentMult = 2

class NodeBuilder:
    def __init__(self, profession):
        self.nodes = []
        self.profession = profession
        self.current_id = 1000 + (profession * 1000)
        self.sector_counters = { (profession, 1): 0, (profession, 2): 0, (profession, 3): 0 }

    def add_node(self, name, desc, tier, node_type, max_points, icon, modifiers=[], effects=[]):
        node = {
            "id": self.current_id,
            "profession": self.profession,
            "type": node_type,
            "tier": tier,
            "sector_index": self.sector_counters[(self.profession, tier)],
            "max_points": max_points,
            "name_key": name,
            "desc_key": desc,
            "modifiers": modifiers,
            "effects": effects,
            "icon_id": icon
        }
        self.nodes.append(node)
        self.sector_counters[(self.profession, tier)] += 1
        self.current_id += 1
        return node

    def create_mod(self, stat_type, mode, value):
        return { "type": stat_type, "mode": mode, "value": float(value), "required_tags": 0, "source": 2, "source_id": 0 }

def generate_balanced_talents():
    builder = NodeBuilder(ProfessionID.BladeAscendant)
    
    # --- Tier 1: 凡人境 (10 Nodes, 50 Points) ---
    builder.add_node("起源", "修行起点", 1, TalentNodeType.Major, 1, "icon_origin", modifiers=[builder.create_mod(i, 0, 1) for i in range(4)])
    
    basics = [
        ("炼体术", "最大生命值", StatType.MaxHealth, 10, "icon_hp"),
        ("聚气诀", "最大法力值", StatType.MaxMana, 10, "icon_mana"),
        ("基础剑招", "物理伤害", StatType.PhysicalDamage, 3, "icon_phys"),
        ("轻身术", "移动速度", StatType.MoveSpeed, 1, "icon_speed"),
        ("铁壁功", "护甲", StatType.Armor, 10, "icon_armor"),
        ("淬骨法", "力量", StatType.Strength, 3, "icon_str"),
        ("通感术", "敏捷", StatType.Dexterity, 3, "icon_dex"),
        ("明心诀", "智力", StatType.Intelligence, 3, "icon_int"),
        ("吐纳法", "生命回复", StatType.HealthRegen, 5, "icon_hp_regen")
    ]
    for name, attr, stat, val, icon in basics:
        builder.add_node(name, f"+{val} {attr}", 1, TalentNodeType.Minor, 5, icon, [builder.create_mod(stat, 1 if "伤害" in attr or "速度" in attr else 0, val)])

    # --- Tier 2: 筑基境 (20 Nodes, 100 Points) ---
    builder.add_node("剑意觉醒", "解锁剑意机制", 2, TalentNodeType.Core, 1, "icon_intent", effects=[{"type":0, "value":"SwordIntentUnlock", "trait_id":101}])
    builder.add_node("剑心通明", "解锁剑心通明特质", 2, TalentNodeType.Core, 1, "icon_sword_heart", effects=[{"type":0, "value":"SwordHeart", "trait_id":100}])
    builder.add_node("筑基：灵泉", "+15% 法力回复速度", 2, TalentNodeType.Major, 1, "icon_mana_regen", [builder.create_mod(StatType.ManaRegen, 1, 15)])
    builder.add_node("筑基：疾风", "+10% 攻击速度", 2, TalentNodeType.Major, 1, "icon_as", [builder.create_mod(StatType.AttackSpeed, 1, 10)])

    advanced = [
        ("流云剑法", "攻击速度", StatType.AttackSpeed, 2, "icon_as"),
        ("弱点洞察", "暴击率", StatType.CritChance, 1, "icon_crit"),
        ("元素剑气", "元素伤害", StatType.FireDamage, 5, "icon_ele"),
        ("幻影身法", "闪避率", StatType.DodgeChance, 1, "icon_dodge"),
        ("剑气护盾", "格挡率", StatType.BlockChance, 1, "icon_block"),
        ("神识锁定", "命中值", StatType.Accuracy, 20, "icon_acc"),
        ("灵力扩充", "法力上限", StatType.MaxMana, 5, "icon_mana_pct"),
        ("玄武之壳", "护甲", StatType.Armor, 5, "icon_armor_pct"),
        ("青龙之息", "生命回复", StatType.HealthRegen, 5, "icon_hp_regen"),
        ("御剑术", "投射物速度", StatType.ProjectileSpeed, 5, "icon_proj"),
        ("剑气回响", "范围效果", StatType.AreaScale, 5, "icon_area"),
        ("灵力反馈", "击中回蓝", StatType.ManaOnHit, 2, "icon_mana_on_hit"),
        ("剑意留影", "暴击率", StatType.CritChance, 2, "icon_crit_2"),
        ("御剑乘风", "移动速度", StatType.MoveSpeed, 2, "icon_speed_2"),
        ("剑意护体", "全局减伤", StatType.GlobalDamageReduction, 1, "icon_dr"),
        ("长生之魂", "最大生命", StatType.MaxHealth, 5, "icon_hp_2")
    ]
    for name, attr, stat, val, icon in advanced:
        builder.add_node(name, f"+{val}% {attr}" if stat != StatType.Accuracy and stat != StatType.ManaOnHit else f"+{val} {attr}", 2, TalentNodeType.Minor, 5, icon, [builder.create_mod(stat, 1 if stat != StatType.Accuracy and stat != StatType.ManaOnHit else 0, val)])

    # --- Tier 3: 元婴/飞升 (15 Nodes, 75 Points) ---
    builder.add_node("剑意浩荡", "最大剑意层数 +2", 3, TalentNodeType.Core, 1, "icon_intent_cap", effects=[{"type":1, "value":"MaxSwordIntent:2", "trait_id":200}])
    builder.add_node("心剑合一", "智力转暴伤", 3, TalentNodeType.Core, 1, "icon_mind_blade", effects=[{"type":3, "value":"IntToCritMult:0.3", "trait_id":0}])
    builder.add_node("御剑御风", "御剑闪避总增", 3, TalentNodeType.Core, 1, "icon_wind_rider", effects=[{"type":4, "value":"SwordRidingMoreEvasion:15", "trait_id":0}])
    
    mastery = [
        ("开山之剑", "物理伤害", StatType.PhysicalDamage, 8, "icon_phys_adv"),
        ("绝杀之意", "暴击伤害", StatType.CritDamage, 15, "icon_crit_adv"),
        ("破妄之气", "护甲穿透", StatType.ArmorPenetration, 5, "icon_pen"),
        ("金刚之躯", "全抗性", StatType.ResistAll, 3, "icon_res"),
        ("无痕步法", "移动速度", StatType.MoveSpeed, 2, "icon_speed_adv"),
        ("长生神魂", "最大生命值", StatType.MaxHealth, 5, "icon_hp_adv"),
        ("剑域扩张", "范围效果", StatType.AreaScale, 5, "icon_area_adv"),
        ("万剑天劫", "全局减伤", StatType.GlobalDamageReduction, 2, "icon_dr_adv"),
        ("灵力回响", "法力回复", StatType.ManaRegen, 5, "icon_mana_adv"),
        ("化神：无我", "全属性加成", StatType.Intelligence, 5, "icon_int_adv"),
        ("剑道感悟", "经验获取", StatType.PhysicalDamage, 2, "icon_exp"),
        ("不灭剑心", "格挡评级", StatType.BlockRating, 10, "icon_block_adv")
    ]
    for name, attr, stat, val, icon in mastery:
        builder.add_node(name, f"+{val}% {attr}", 3, TalentNodeType.Minor, 5, icon, [builder.create_mod(stat, 1, val)])

    return builder.nodes

def main():
    nodes = generate_balanced_talents()
    data = {
        "version": 1,
        "profession_stars": [
            {"profession": 0, "name_key": "剑修", "desc_key": "御剑而行，以气化形"},
            {"profession": 1, "name_key": "秘术师", "desc_key": "掌控元素，毁灭万物"},
            {"profession": 2, "name_key": "神官", "desc_key": "神圣治愈，净化心灵"},
            {"profession": 3, "name_key": "骑士", "desc_key": "重甲守护，坚不可摧"},
            {"profession": 4, "name_key": "游侠", "desc_key": "百步穿杨，陷阱伏击"},
            {"profession": 5, "name_key": "狂战士", "desc_key": "怒气爆发，愈战愈勇"}
        ],
        "nodes": nodes
    }
    out_path = "assets/data/profession_talents.json"
    with open(out_path, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=2, ensure_ascii=False)
    print(f"Generated {len(nodes)} balanced nodes. Total points: {sum(n['max_points'] for n in nodes)}")

if __name__ == "__main__":
    main()
