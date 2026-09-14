// ============================================================================
// SkillBakerFlagConsumerTests.cpp
//
// 用途：SkillSpecializationBaker 的 feature_flags「登记表守护」。
// 守护以下四条不变量：
//   (a) 任一技能树节点 bake 后产生的位，都必须在下方 kFlagConsumers 登记表内
//       —— 新增位不登记则本测试失败；
//   (b) 登记表覆盖全部实际产出的位，且不登记任何不会产出的位
//       （登记表位并集 == 全树遍历实际产出位并集）；
//   (c) 每个登记位都声明其消费点（consumer 字符串）；没有消费点的位必须
//       显式列入 kKnownDeadFlagBits 并附理由；
//   (d) kKnownDeadFlagBits 与登记表中 consumer 标注为「无」的位集合完全一致。
//
// 局限（重要）：
//   1. 本测试是「静态登记表 + 全树 bake 观测」的守护，运行期无法反射某个位
//      是否真的存在消费点。消费点的存在性由
//      `docs/designs/2026-09-13-skill-baker-consumer-map.md`（表 C）+
//      人工 rg 复核
//      （`rg "feature_flags &" src/game --glob "!SkillSpecializationBaker.cpp"`）
//      共同保证。本测试只守住「新增位必须登记 + 死位必须显式声明」这一条。
//   2. feature_flags 位是「按技能复用」的：同一个位值在不同技能下语义不同
//      （例如 bit0 在技能2=折返、技能3=无尽剑匣、技能4=金钟罩……）。因此表中
//      一行按位值登记，semantic 列以 `s<技能> <语义>` 列出所有已知复用，
//      consumer 列只保证「至少有一个技能消费」，不区分具体技能。
//   3. 死位（kKnownDeadFlagBits）只代表「全仓没有 feature_flags 读取方」，
//      不代表该位在设计中无意义：部分死位的语义由 del.* 专用字段/行为层承载，
//      由映射表 §5 逐条登记，待后续任务迁移或删除。本测试对 consumer 内容做
//      一致性断言（死位标「无」、非死位非空且非「无」），但不验证消费点真实存在。
//
// 覆盖依据（rg 证据）：
//   Baker 侧：src/game/systems/skill/SkillSpecializationBaker.cpp 的
//             ApplyNodeModifiersToProfile 中所有 `del.feature_flags |=`。
//             全部技能节点仅写入 bit0..bit28，并集恰为 0x1FFFFFFF。
//   消费侧：behaviors/ 与交付系统（RendingWave/BladeFormation/SwordArray/
//             BladeWard/FlowingThrust/InfiniteBlades/BladeBoomerang/
//             BeamChannelDeliverySystem/SkillSystem/DamageMitigationService）。
//
// 用例命名遵循 doctest 约定：前缀 `[Unit] `；值断言用 CHECK，前置条件用 REQUIRE。
// 注意避免 `CHECK(a && b)`（会触发 MSVC doctest C2338）。
// ============================================================================

#include "TestCommon.hpp"

#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace NoMoreDay {
namespace {

// 生成单个位（1u << index）。用于表驱动，避免散落的魔法数字。
constexpr uint32_t FlagBit(uint32_t index) { return 1u << index; }

// ---------------------------------------------------------------------------
// feature_flags 位登记表。
//
// bit      : 位值（必为 2 的幂且全局唯一）。
// semantic : 该位在各技能下的语义（feature_flags 按技能复用）。
// consumer : 已知消费点（`feature_flags &` 读取处）。没有消费点的位必须把
//            consumer 标记为 kNoConsumer（"无"），并同时列入 kKnownDeadFlagBits
//            且附理由；此类位由映射表文档 §5 逐条复核。
// ---------------------------------------------------------------------------
constexpr const char *kNoConsumer = "无";

struct FlagSpec {
  uint32_t bit;
  const char *semantic;
  const char *consumer;
};

const FlagSpec kFlagConsumers[] = {
    {FlagBit(0),
     "s2 折返IsBoomerang；s3 无尽剑匣；s4 金钟罩；s5 剑意共鸣；s6 迟缓剑压；"
     "s7 心流叠加（运行时节点判定，不读 flag）；s8 轻巧；s9 延命",
     "RendingWave.cpp:184；BladeFormation.cpp:176；SwordArray.cpp:181；"
     "BeamChannelDeliverySystem.cpp:997"},
    {FlagBit(1),
     "s1 残影；s3 巨剑GiantSword；s4 拨云见日；s5 陨铁；s6 破甲剑意；"
     "s7 碎空爆；s8 疾速；s9 气旋爆发",
     "FlowingThrust.cpp:129；BladeFormation.cpp:111；SwordArray.cpp:186"},
    {FlagBit(2),
     "s1 疾风；s2 分裂标记；s3 旋刃BladeOrbit；s4 五行御守；s5 灵动引导；"
     "s6 绝命法场；s7 虚无牵引；s8 锋锐；s9 身轻如燕",
     "FlowingThrust.cpp:128；RendingWave.cpp:185；BladeFormation.cpp:112；"
     "SwordArray.cpp:195"},
    {FlagBit(3),
     "s1 换位Swap；s2 散射；s3 不朽Immortality；s4 不动如山；s5 神识锁定；"
     "s6 意念合一；s7 精神透支；s8 回力感应；s9 空明心境",
     "FlowingThrust.cpp:131；RendingWave.cpp:186；BladeWard.cpp:125；"
     "SwordArray.cpp:212"},
    {FlagBit(4),
     "s1 位移强化(115)；s2 顶点牵引(232/233)；s3 神速Godspeed；s4 剑意格挡；"
     "s5 无处遁形；s6 双生剑阵；s7 破绽洞察；s8 滞空切割；s9 向死而生",
     "RendingWave.cpp:389；BladeFormation.cpp:117；SwordArray.cpp:97；"
     "BeamChannelDeliverySystem.cpp:1125"},
    {FlagBit(5),
     "s1 影之突袭ShadowStrike；s2 时间停顿TimeLock；s3 专注；s4 借力打力；"
     "s5 天降命印；s6 三才阵；s7 神识多开；s8 放血；s9 浴血重生",
     "RendingWave.cpp:183；FlowingThrust.cpp:130；BladeFormation.cpp:118；"
     "SwordArray.cpp:100"},
    {FlagBit(6),
     "s1 要害感知；s2 环绕Orbit；s3 剑气强化(315)；s4 瞬身反打；s5 天诛；"
     "s6 剑气共鸣；s7 千面阵；s8 撕裂伤口；s9 绝影护甲",
     "RendingWave.cpp:182；BladeFormation.cpp:145"},
    {FlagBit(7),
     "s1 距离加成(155)；s2 灵追SpiritPursuit；s4 剑气反震；s5 剑刃风暴；"
     "s6 千丝万缕；s7 步影随行；s8 剑鸣；s9 虚灵之躯",
     "RendingWave.cpp:187；BladeWard.cpp:126；SwordArray.cpp:250"},
    {FlagBit(8),
     "s1 势不可挡Momentum；s3 剑匣强化(303)；s4 以眼还眼；s5 万剑归阵；"
     "s6 流云穿阵；s7 御剑神游；s8 拔血流云；s9 灵流穿透",
     "FlowingThrust.cpp:183；BeamChannelDeliverySystem.cpp:1091；"
     "SwordArray.cpp:251；BladeFormation.cpp:238"},
    {FlagBit(9),
     "s2 顶点牵引(233)；s3 剑匣强化(312)；s4 雷霆法环；s5 气定神闲；"
     "s6 御剑阵威；s7 神游脱战；s8 风眼；s9 虚境馈赠",
     "RendingWave.cpp:391；BladeFormation.cpp:140；BladeWard.cpp:130"},
    {FlagBit(10),
     "s3 剑匣强化(331)；s4 霜铠；s5 不坏剑身；s6 虚弱领域；s7 精准切割；"
     "s8 幻影回旋；s9 逆脉",
     "BladeWard.cpp:131；BeamChannelDeliverySystem.cpp:1010"},
    {FlagBit(11),
     "s3 剑匣强化(333)；s4 雷贯长虹；s5 剑气充盈；s6 剑阵牢笼；s7 引力坍缩；"
     "s8 无尽刃舞；s9 孤注一掷",
     "BladeWard.cpp:132；SwordArray.cpp:203；BeamChannelDeliverySystem.cpp:1052"},
    {FlagBit(12),
     "s2 剑意爆发IntentBurst；s3 剑匣强化(334)；s5 巨剑术；s6 阵斩回响；"
     "s7 空间粉碎；s8 接剑；s9 死亡螺旋",
     "RendingWave.cpp:109；BeamChannelDeliverySystem.cpp:1155"},
    {FlagBit(13),
     "s3 剑匣强化(350)；s5 天剑降世；s6 阵眼；s7 深渊侵蚀；s8 连环劲；"
     "s9 嗜血本能",
     "BladeFormation.cpp:153；BeamChannelDeliverySystem.cpp:926"},
    {FlagBit(14),
     "s3 剑匣强化(352)；s5 余波；s6 灵力泉涌；s7 意念风暴；s8 御剑接踵；"
     "s9 剑随心动",
     "BladeFormation.cpp:158"},
    {FlagBit(15),
     "s2 湮灭波ObliterationWave；s3 法术回响SpellEcho；s5 御剑行；"
     "s6 随身剑垒；s7 剑意化无；s8 回旋游步；s9 破空一闪",
     "RendingWave.cpp:141；BladeFormation.cpp:119；SwordArray.cpp:152"},
    {FlagBit(16),
     "s2 回响斩EchoedSlash；s3 法阵共鸣ArrayResonance；s5 御剑风雷；"
     "s6 剑神领域；s7 心念反哺；s8 磁力场Magnet；s9 缩地成寸",
     "RendingWave.cpp:410；BladeFormation.cpp:124；InfiniteBlades.cpp:157；"
     "BladeBoomerang.cpp:186"},
    {FlagBit(17),
     "s3 剑匣强化(371)；s5 随影；s6 法阵回护；s7 天外冰晶；s8 重力网；"
     "s9 意随神行",
     "BeamChannelDeliverySystem.cpp:1113"},
    {FlagBit(18),
     "s3 剑匣强化(373)；s5 剑意回流；s6 焚天烈焰阵；s7 极寒碎骨；"
     "s8 意随剑舞；s9 御剑化影",
     "SwordArray.cpp:221"},
    {FlagBit(19),
     "s3 剑匣强化(375)；s5 意气爆发；s6 炼狱余火；s7 神雷天罡；s8 心剑合一；"
     "s9 时光逆流",
     "InfiniteBlades.cpp:203"},
    {FlagBit(20),
     "s5 意念合一；s6 九幽雷池；s7 天劫落雷；s8 巨阙Giant；s9 全神贯注",
     "SwordArray.cpp:229；InfiniteBlades.cpp:212；BladeBoomerang.cpp:187"},
    {FlagBit(21), "s5 天火流星；s6 连珠落雷；s7 异常切割；s8 劫灰路径；s9 天山雪隐",
     "SwordArray.cpp:234"},
    {FlagBit(22),
     "s2 灵根感应(274)；s5 末日余烬；s6 法阵侵蚀；s7 心念灭抗；s8 燎原之势；"
     "s9 凛冬附魔",
     "DamageMitigationService.cpp:141"},
    {FlagBit(23), "s5 凛冬暴雪；s6 移形换阵；s8 电磁回旋；s9 疾空惊雷",
     "SkillSystem.cpp:1943；SwordArray.cpp:113"},
    {FlagBit(24), "s5 绝对零度；s8 高压电弧；s9 过载护盾", kNoConsumer},
    {FlagBit(25), "s5 灵根感应；s8 元素尾迹；s9 意念穿透",
     "DamageMitigationService.cpp:169"},
    {FlagBit(26), "s5 天灾；s8 灵根破壁；s9 灵气反哺", kNoConsumer},
    {FlagBit(27), "s8 元素护体；s9 影剑回响", kNoConsumer},
    {FlagBit(28), "s9 湮灭剑域(935)", kNoConsumer},
};

constexpr std::size_t kFlagConsumerCount =
    sizeof(kFlagConsumers) / sizeof(kFlagConsumers[0]);

// ---------------------------------------------------------------------------
// 已知死位清单：全仓（除 Baker 自身）不存在任何 `feature_flags &` 读取方的位。
//
// 必须与 kFlagConsumers 中 consumer == kNoConsumer 的位集合完全一致（由用例1
// 断言）；每项附理由，理由取自映射表表 C / §5。若后续为某位新增消费点，需
// 同时从本清单移除该位并把登记表的 consumer 改为真实读取点。
// ---------------------------------------------------------------------------
struct DeadFlagSpec {
  uint32_t bit;
  const char *reason;
};

const DeadFlagSpec kKnownDeadFlagBits[] = {
    {FlagBit(24),
     "映射表表 C 16777216：s5:573 / s8:873 写入，全仓无任何 feature_flags 读取方"},
    {FlagBit(26),
     "映射表表 C 67108864：s5:575 / s8:875 写入，全仓无任何 feature_flags 读取方"},
    {FlagBit(27),
     "映射表表 C 134217728：s8:876 写入，全仓无任何 feature_flags 读取方"},
    {FlagBit(28),
     "映射表表 C 268435456：s9:935 写入，全仓无任何 feature_flags 读取方"},
};

constexpr std::size_t kKnownDeadFlagCount =
    sizeof(kKnownDeadFlagBits) / sizeof(kKnownDeadFlagBits[0]);

// Baker 实际会产出的位集合，显式列出（与上表相互印证）。
// rg 证据见文件头：ApplyNodeModifiersToProfile 的全部 `del.feature_flags |=`，
// 覆盖技能1..9 且并集恰为 bit0..bit28。
constexpr uint32_t kExpectedBakerFlagBits[] = {
    FlagBit(0),  FlagBit(1),  FlagBit(2),  FlagBit(3),  FlagBit(4),
    FlagBit(5),  FlagBit(6),  FlagBit(7),  FlagBit(8),  FlagBit(9),
    FlagBit(10), FlagBit(11), FlagBit(12), FlagBit(13), FlagBit(14),
    FlagBit(15), FlagBit(16), FlagBit(17), FlagBit(18), FlagBit(19),
    FlagBit(20), FlagBit(21), FlagBit(22), FlagBit(23), FlagBit(24),
    FlagBit(25), FlagBit(26), FlagBit(27), FlagBit(28),
};

constexpr std::size_t kExpectedBakerFlagCount =
    sizeof(kExpectedBakerFlagBits) / sizeof(kExpectedBakerFlagBits[0]);

// bit0..bit28 全置位。
constexpr uint32_t kExpectedBakerFlagMask = 0x1FFFFFFFu;

// 在登记表中查找位值对应的登记项；未登记返回 nullptr。
const FlagSpec *FindFlagSpec(uint32_t bit) {
  for (std::size_t i = 0; i < kFlagConsumerCount; ++i) {
    if (kFlagConsumers[i].bit == bit) {
      return &kFlagConsumers[i];
    }
  }
  return nullptr;
}

// consumer 是否标记为「无消费点」（死位）。
bool IsNoConsumer(const char *consumer) {
  return consumer != nullptr && std::strcmp(consumer, kNoConsumer) == 0;
}

// 位是否在已知死位清单中。
bool IsKnownDeadBit(uint32_t bit) {
  for (std::size_t i = 0; i < kKnownDeadFlagCount; ++i) {
    if (kKnownDeadFlagBits[i].bit == bit) {
      return true;
    }
  }
  return false;
}

// 检查 flags 中每一位都已在登记表中登记。
bool AllProducedBitsRegistered(uint32_t flags) {
  for (uint32_t index = 0; index < 32u; ++index) {
    const uint32_t bit = FlagBit(index);
    if ((flags & bit) != 0u && FindFlagSpec(bit) == nullptr) {
      return false;
    }
  }
  return true;
}

// 代表性关键节点：每种交付原型/技能至少取一个代表节点，逐位 bake 并断言。
// 期望位与 Baker 写入保持一致（多位的 233 用掩码表达）。
struct RepresentativeBake {
  uint32_t skill_id;
  uint32_t node_id;
  uint32_t expected_mask;
};

const RepresentativeBake kRepresentativeBakes[] = {
    // 技能1：位移/残影/换位类
    {1u, 112u, FlagBit(8)},
    {1u, 113u, FlagBit(2)},
    {1u, 115u, FlagBit(4)},
    {1u, 130u, FlagBit(1)},
    {1u, 132u, FlagBit(5)},
    {1u, 133u, FlagBit(3)},
    {1u, 150u, FlagBit(6)},
    {1u, 155u, FlagBit(7)},
    // 技能2：折返/顶点牵引/分裂/湮灭波
    {2u, 230u, FlagBit(0)},
    {2u, 232u, FlagBit(0) | FlagBit(4)},
    {2u, 233u, FlagBit(0) | FlagBit(4) | FlagBit(9)},
    {2u, 251u, FlagBit(12)},
    {2u, 253u, FlagBit(15)},
    {2u, 254u, FlagBit(16)},
    {2u, 274u, FlagBit(22)},
    // 技能3：剑匣/法术回响
    {3u, 311u, FlagBit(0)},
    {3u, 312u, FlagBit(9)},
    {3u, 331u, FlagBit(10)},
    {3u, 333u, FlagBit(11)},
    {3u, 350u, FlagBit(13)},
    {3u, 352u, FlagBit(14)},
    {3u, 371u, FlagBit(17)},
    {3u, 373u, FlagBit(18)},
    {3u, 375u, FlagBit(19)},
    // 技能4：护盾/反震/雷霆
    {4u, 400u, FlagBit(0)},
    {4u, 470u, FlagBit(7)},
    {4u, 472u, FlagBit(9)},
    {4u, 474u, FlagBit(10)},
    {4u, 473u, FlagBit(11)},
    // 技能5：引导/领域
    {5u, 515u, FlagBit(8)},
    {5u, 533u, FlagBit(12)},
    {5u, 534u, FlagBit(13)},
    {5u, 555u, FlagBit(20)},
    {5u, 571u, FlagBit(22)},
    {5u, 575u, FlagBit(26)},
    // 技能6：剑阵
    {6u, 610u, FlagBit(4)},
    {6u, 611u, FlagBit(5)},
    {6u, 672u, FlagBit(20)},
    {6u, 673u, FlagBit(21)},
    {6u, 675u, FlagBit(23)},
    // 技能7：神游/元素
    {7u, 710u, FlagBit(0)},
    {7u, 735u, FlagBit(10)},
    {7u, 750u, FlagBit(11)},
    {7u, 770u, FlagBit(17)},
    {7u, 772u, FlagBit(19)},
    {7u, 773u, FlagBit(20)},
    {7u, 774u, FlagBit(21)},
    {7u, 775u, FlagBit(22)},
    // 技能8：折返投射（磁力/巨阙）
    {8u, 800u, FlagBit(0)},
    {8u, 803u, FlagBit(3)},
    {8u, 810u, FlagBit(4)},
    {8u, 815u, FlagBit(9)},
    {8u, 830u, FlagBit(10)},
    {8u, 850u, FlagBit(16)},
    {8u, 851u, FlagBit(17)},
    {8u, 852u, FlagBit(18)},
    {8u, 853u, FlagBit(19)},
    {8u, 854u, FlagBit(20)},
    {8u, 870u, FlagBit(21)},
    {8u, 874u, FlagBit(25)},
    {8u, 875u, FlagBit(26)},
    {8u, 876u, FlagBit(27)},
    // 技能9：幻影/护盾
    {9u, 902u, FlagBit(2)},
    {9u, 913u, FlagBit(8)},
    {9u, 934u, FlagBit(14)},
    {9u, 935u, FlagBit(28)},
    {9u, 972u, FlagBit(23)},
    {9u, 973u, FlagBit(24)},
    {9u, 974u, FlagBit(3)},
    {9u, 975u, FlagBit(0)},
    {9u, 993u, FlagBit(27)},
};

} // namespace

// ---------------------------------------------------------------------------
// 1) 登记表自身一致性：位合法、无重复、字符串非空；每个登记位声明消费点，
//    且 kKnownDeadFlagBits 与表中 consumer 标为「无」的位集合完全一致。
// ---------------------------------------------------------------------------
TEST_CASE(
    "[Unit] SkillBakerFlagRegistry - table and dead-bit declarations are "
    "consistent") {
  uint32_t table_mask = 0u;
  uint32_t table_dead_mask = 0u;

  for (std::size_t i = 0; i < kFlagConsumerCount; ++i) {
    const FlagSpec &spec = kFlagConsumers[i];
    CAPTURE(i); CAPTURE(spec.semantic);

    const uint32_t bit = spec.bit;
    CHECK(bit != 0u);
    // 位必须是 2 的幂（single bit）。
    const bool is_single_bit = (bit & (bit - 1u)) == 0u;
    CHECK(is_single_bit);
    // 位值不得重复登记。
    const bool already_seen = (table_mask & bit) != 0u;
    CHECK(!already_seen);
    CHECK(spec.semantic != nullptr);

    // 消费点声明：死位必须标为「无」，非死位必须有非空且非「无」的消费点。
    REQUIRE(spec.consumer != nullptr);
    const bool no_consumer = IsNoConsumer(spec.consumer);
    const bool known_dead = IsKnownDeadBit(bit);
    CHECK(no_consumer == known_dead);
    if (!known_dead) {
      const bool non_empty = spec.consumer[0] != '\0';
      CHECK(non_empty);
    }
    if (no_consumer) {
      table_dead_mask |= bit;
    }

    table_mask |= bit;
  }

  // 已知死位清单自身：位合法、无重复、理由非空。
  uint32_t declared_dead_mask = 0u;
  for (std::size_t i = 0; i < kKnownDeadFlagCount; ++i) {
    const DeadFlagSpec &dead = kKnownDeadFlagBits[i];
    CAPTURE(i); CAPTURE(dead.bit); CAPTURE(dead.reason);

    CHECK(dead.bit != 0u);
    const bool is_single_bit = (dead.bit & (dead.bit - 1u)) == 0u;
    CHECK(is_single_bit);
    const bool already_seen = (declared_dead_mask & dead.bit) != 0u;
    CHECK(!already_seen);
    CHECK(dead.reason != nullptr);
    const bool reason_non_empty = dead.reason[0] != '\0';
    CHECK(reason_non_empty);
    declared_dead_mask |= dead.bit;
  }

  // (d) 死位清单与表中 consumer 标为「无」的位集合完全一致；全仓死位恰为 4 个。
  CHECK(table_dead_mask == declared_dead_mask);
  CHECK(kKnownDeadFlagCount == 4u);

  uint32_t expected_mask = 0u;
  for (std::size_t i = 0; i < kExpectedBakerFlagCount; ++i) {
    expected_mask |= kExpectedBakerFlagBits[i];
  }

  // 显式清单自身为 bit0..bit28 全置位。
  CHECK(expected_mask == kExpectedBakerFlagMask);
  // 登记表恰好覆盖 Baker 产出的全部位。
  CHECK(table_mask == kExpectedBakerFlagMask);
  CHECK(kFlagConsumerCount == kExpectedBakerFlagCount);

  // 每个已知死位都必须真实登记在表中。
  for (std::size_t i = 0; i < kKnownDeadFlagCount; ++i) {
    const uint32_t bit = kKnownDeadFlagBits[i].bit;
    CAPTURE(bit);
    CHECK(FindFlagSpec(bit) != nullptr);
  }
}

// ---------------------------------------------------------------------------
// 2) 穷举烘焙：遍历技能1..9 的全部技能树节点，逐节点单独 bake，断言：
//    a. 任意产出的位都必须在登记表内（新增位未登记则失败）；
//    b. 所有节点产出位的并集 == 登记表位集合（无多、无漏）；
//    c. 登记表每一位都至少有一个产出节点（覆盖记录）。
//
// 说明：必须逐节点单独 bake。技能6 存在跨节点后处理
// （262144 与 1048576 同时存在时会剔除 1048576），一次性烘焙会掩盖部分位。
// ---------------------------------------------------------------------------
TEST_CASE(
    "[Unit] SkillBakerFlagRegistry - every tree node emits only registered "
    "flags") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  entt::registry registry;
  const auto player = registry.create();

  uint32_t observed_mask = 0u;
  uint32_t table_mask = 0u;
  for (std::size_t i = 0; i < kFlagConsumerCount; ++i) {
    table_mask |= kFlagConsumers[i].bit;
  }

  for (uint32_t skill_id = 1u; skill_id <= 9u; ++skill_id) {
    const SkillTreeDefinition *tree =
        SkillRegistry::Get().GetSkillTree(skill_id);
    REQUIRE(tree != nullptr);

    for (const auto &entry : tree->nodes) {
      const uint32_t node_id = entry.first;

      SpecializedSkill spec;
      spec.skill_id = skill_id;
      spec.allocated_points[node_id] = 4;

      BakedSkillProfile profile{};
      SkillSpecializationBaker::Bake(registry, player, skill_id, &spec, profile,
                                     nullptr);

      const uint32_t flags = profile.delivery.feature_flags;
      CAPTURE(skill_id); CAPTURE(node_id);
      const bool registered = AllProducedBitsRegistered(flags);
      CHECK(registered);
      observed_mask |= flags;
    }
  }

  // (b) 实际产出的位集合与登记表位并集完全一致（无多、无漏）。
  CHECK(observed_mask == table_mask);
  CHECK(observed_mask == kExpectedBakerFlagMask);

  // 覆盖记录：登记表内每一位都必须被至少一个技能树节点实际产出。
  for (std::size_t i = 0; i < kFlagConsumerCount; ++i) {
    const uint32_t bit = kFlagConsumers[i].bit;
    const bool covered = (observed_mask & bit) != 0u;
    CAPTURE(bit); CAPTURE(kFlagConsumers[i].semantic);
    CHECK(covered);
  }
}

// ---------------------------------------------------------------------------
// 3) 代表性关键节点：按技能/交付原型逐个 bake，断言期望位出现，且产出的位
//    全部已登记。与用例2的穷举互为印证（用例2保证不漏，本用例保证不错）。
// ---------------------------------------------------------------------------
TEST_CASE(
    "[Unit] SkillBakerFlagRegistry - representative key nodes emit expected "
    "registered flags") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  entt::registry registry;
  const auto player = registry.create();

  const std::size_t representative_count =
      sizeof(kRepresentativeBakes) / sizeof(kRepresentativeBakes[0]);

  for (std::size_t i = 0; i < representative_count; ++i) {
    const RepresentativeBake &rb = kRepresentativeBakes[i];
    CAPTURE(rb.skill_id); CAPTURE(rb.node_id); CAPTURE(rb.expected_mask);

    SpecializedSkill spec;
    spec.skill_id = rb.skill_id;
    spec.allocated_points[rb.node_id] = 4;

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, rb.skill_id, &spec, profile,
                                   nullptr);

    const uint32_t flags = profile.delivery.feature_flags;

    const bool expected_present = (flags & rb.expected_mask) == rb.expected_mask;
    CHECK(expected_present);

    const bool registered = AllProducedBitsRegistered(flags);
    CHECK(registered);
  }
}

} // namespace NoMoreDay
