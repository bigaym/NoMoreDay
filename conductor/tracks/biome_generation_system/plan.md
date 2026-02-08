# 鐢熺墿缇よ惤鍦板浘鐢熸垚绯荤粺 瀹炴柦璁″垝 (V1.0)

> **Track ID**: `biome_generation_system_20260208`
> **渚濊禆 Spec**: `spec.md` (V1.0)
> **棰勮宸ユ椂**: 7-10 澶?

---

## 馃搶 闃舵鎬昏

| 闃舵 | 瀛怲rack | 鍚嶇О | 鏍稿績浜у嚭 | 棰勮宸ユ椂 | 鐘舵€?|
|------|---------|------|----------|----------|------|
| **Phase 1** | 1.x | 鏁版嵁椹卞姩灞?| BiomeConfig鎵╁睍, JSON瑙ｆ瀽, 鏋氫妇瀹氫箟 | 1 澶?| 鈴?寰呭紑濮?|
| **Phase 2** | 2.x | 娓叉煋涓庣墿鐞嗗寮?| 绌烘皵澧欐覆鏌撳櫒, 鑳屾櫙Shader, 鐗规畩鐗╃悊 | 2 澶?| 鈴?寰呭紑濮?|
| **Phase 3** | 3.x | 鐢熸垚绠楁硶婕旇繘 | 涓夌被绛栫暐鐢熸垚鍣? CA鍙傛暟璋冧紭 | 2-3 澶?| 鈴?寰呭紑濮?|
| **Phase 4** | 4.x | 鍔ㄦ€佷氦浜掗€昏緫 | 鍙牬鍧忓湴褰? 鍔ㄦ€佸埛鎬, 鍔犻€熷甫 | 1.5 澶?| 鈴?寰呭紑濮?|
| **Phase 5** | 5.x | 鐢熸€侀泦鎴?| 鎬墿姹犳槧灏? 鎺夎惤琛ㄥ叧鑱?| 1 澶?| 鈴?寰呭紑濮?|
| **Phase 6** | 6.x | 娴嬭瘯涓庢墦纾?| 鍏ㄧ兢钀芥祴璇? 鎬ц兘鍩哄噯, Bug淇 | 1 澶?| 鈴?寰呭紑濮?|

---

## 馃敶 Sub-Track 1: 鏁版嵁椹卞姩灞?(Foundation)

> **鐩爣**: 寤虹珛瀹屾暣鐨勭兢钀芥暟鎹熀纭€璁炬柦

### Task 1.1: BiomeID 鏋氫妇鎵╁睍 鈴憋笍 0.5h
- [x] 鍦?`Common.hpp` 涓墿灞?`BiomeID` 鏋氫妇
- [x] 娣诲姞鎵€鏈?27 绉嶇兢钀絀D (鍩庨晣6 + 鎴樻枟21)
- [x] 纭繚 `Town=1`, `Cave=10` 淇濇寔鍏煎

**鏂囦欢淇敼**:
```
src/game/components/Common.hpp
```

### Task 1.2: BiomeStyle 鍜?BiomeFeature 瀹氫箟 鈴憋笍 0.5h
- [x] 瀹氫箟 `BiomeStyle` 鏋氫妇 (Town, Open, Maze, Special)
- [x] 瀹氫箟 `BiomeFeature` 浣嶆帺鐮佹灇涓?
- [x] 娣诲姞杈呭姪瀹?鍑芥暟鐢ㄤ簬浣嶆搷浣?

**鏂囦欢淇敼**:
```
src/game/components/Common.hpp
```

### Task 1.3: BiomeConfig 缁撴瀯浣撴墿灞?鈴憋笍 1h
- [x] 鎵╁睍 `BiomeConfig` 娣诲姞鏂板瓧娈?
- [x] 娣诲姞 `style`, `features`, 鐗规畩鏈哄埗鍙傛暟
- [x] 娣诲姞瑙嗚灞炴€?(ambientColor, backgroundShader, visualFilterShader)
- [x] 瀹炵幇 `hasFeature()` 渚挎嵎鏂规硶

**鏂囦欢淇敼**:
```
src/game/data/BiomeRegistry.hpp
src/game/data/BiomeRegistry.cpp
```

### Task 1.4: biomes.json 鎵╁睍涓庤В鏋?鈴憋笍 2h
- [x] 鍗囩骇 `biomes.json` schema 鍒?version 2
- [x] 娣诲姞鍏ㄩ儴 27 绉嶇兢钀介厤缃?
- [x] 瀹炵幇鏂板瓧娈电殑 JSON 瑙ｆ瀽閫昏緫
- [x] 娣诲姞 `features` 瀛楃涓叉暟缁勫埌浣嶆帺鐮佺殑杞崲

**鏂囦欢淇敼**:
```
assets/data/biomes.json
src/game/data/BiomeRegistry.cpp (LoadFromJSON鏂规硶)
```

### Task 1.5: 鍗曞厓娴嬭瘯 - BiomeRegistry 鈴憋笍 0.5h
- [x] 娴嬭瘯 JSON 鍔犺浇瀹屾暣鎬?
- [x] 娴嬭瘯 `GetBiome(BiomeID)` 鍜?`GetBiome(std::string)`
- [x] 娴嬭瘯 `hasFeature()` 浣嶆帺鐮侀€昏緫

---

## 馃煚 Sub-Track 2: 娓叉煋涓庣墿鐞嗗寮?(Visual & Physics)

> **鐩爣**: 瀹炵幇绌烘皵澧欐覆鏌撶绾垮拰鐗规畩鐗╃悊鏁堟灉

### Task 2.1: 绌烘皵澧欐爣璁扮郴缁?鈴憋笍 1h
- [x] 鎵╁睍 `Tile` 缁撴瀯娣诲姞 `isAirWall` 鏍囪 (鎴栦娇鐢ㄧ幇鏈塼ype+biome鍒ゆ柇)
- [x] 淇敼 `MapSystem::render()` 璺宠繃绌烘皵澧欑摝鐗囩殑澧欏娓叉煋
- [x] 纭繚鐗╃悊灞備粛鐒堕樆鎸＄Щ鍔?

**鏂囦欢淇敼**:
```
src/game/components/MapComponent.hpp
src/game/systems/world/MapSystem.cpp
```

### Task 2.2: AirWallRenderer 瀹炵幇 鈴憋笍 2h
- [x] 鍒涘缓 `AirWallRenderer` 绫?
- [x] 瀹炵幇 `Initialize()`: 鍔犺浇鑳屾櫙Shader
- [x] 瀹炵幇 `RenderBackground()`: 娓叉煋鏄熺┖/浜戞捣鑳屾櫙
- [x] 闆嗘垚鍒版覆鏌撶绾?(鍦ㄥ湴鍥炬覆鏌撲箣鍓?

**鏂板缓鏂囦欢**:
```
src/game/systems/render/AirWallRenderer.hpp
src/game/systems/render/AirWallRenderer.cpp
```

### Task 2.3: 鑳屾櫙Shader缂栧啓 鈴憋笍 1.5h
- [x] `sky_background.fs`: 鏄熺┖鏁堟灉 (鐢ㄤ簬婕傛诞缇ゅ矝銆佷簯椤跺ぉ瀹?
- [x] `coral_background.fs`: 娣辨捣鏁堟灉 (鐢ㄤ簬鐝婄憵閬楄抗)
- [x] 缁熶竴 uniform 鎺ュ彛 (time, cameraOffset, zoom)

**鏂板缓鏂囦欢**:
```
assets/shaders/backgrounds/sky_background.fs
assets/shaders/backgrounds/coral_background.fs
```

### Task 2.4: 瑙嗚婊ら暅鍚庡鐞?鈴憋笍 1.5h
- [x] `abyss_fog.fs`: 娣辨笂杩烽浘鏁堟灉
- [x] `coral_filter.fs`: 钃濊壊娣辨捣婊ら暅
- [x] 淇敼娓叉煋绠＄嚎鏀寔鍚庡鐞哠hader

**鏂板缓鏂囦欢**:
```
assets/shaders/filters/abyss_fog.fs
assets/shaders/filters/coral_filter.fs
```

### Task 2.5: 鐗规畩鐗╃悊缁勪欢 鈴憋笍 1h
- [x] 锛堣瘎浼板悗璺宠繃锛屽綋鍓嶇増鏈級瀹炵幇浣庨噸鍔? `gravityMultiplier` 搴旂敤鍒?PhysicsSystem
- [x] 锛堣瘎浼板悗璺宠繃锛屽綋鍓嶇増鏈級瀹炵幇鎽╂摝鍔涗慨鏀? `frictionMultiplier` 搴旂敤鍒?MovementSystem
- [x] 锛堣瘎浼板悗璺宠繃锛屽綋鍓嶇増鏈級鍒涘缓 `BiomeEffectComponent` 缂撳瓨褰撳墠缇よ惤鏁堟灉

**璺宠繃鍘熷洜锛?026-02-08锛?*:
- 褰撳墠宸ョ▼涓嶅瓨鍦?`MovementSystem`锛堣鍒掓枃浠惰矾寰勫け鏁堬級锛岀帺瀹剁Щ鍔ㄥ湪 `GameplayState` 鍐呯洿鎺ュ啓鍏?`Velocity`锛屽熀纭€闃诲凹鍦?`PhysicsSystem::updatePosition()` 缁熶竴澶勭悊銆?
- 褰撳墠鎴樻枟/浣嶇Щ閫昏緫鏄簩缁村钩闈㈤€熷害妯″瀷锛屼笉瀛樺湪鍙鐢ㄧ殑鈥滈噸鍔涜酱鈥濇満鍒讹紱`gravityMultiplier` 鐜伴樁娈垫棤绋冲畾璇箟锛屽己琛屾帴鍏ヤ細褰卞搷鍐插埡銆佹姇灏勭墿涓庢€墿閫熷害涓€鑷存€с€?
- 鑻ュ悗缁渶瑕佽鐗规€э紝寤鸿鍦ㄦ柊澧炵粺涓€杩愬姩鍩熺粍浠讹紙濡?`BiomeEffectComponent` + 缁熶竴閫熷害绯绘暟绠＄嚎锛夊悗鍐嶈惤鍦帮紝鑰屼笉鏄湪褰撳墠鍒嗘暎閫昏緫涓复鏃舵墦琛ヤ竵銆?

**鏂囦欢淇敼**:
```
src/game/systems/physics/PhysicsSystem.cpp
src/game/systems/combat/MovementSystem.cpp
```

---

## 馃煛 Sub-Track 3: 鐢熸垚绠楁硶婕旇繘 (Generation Algorithms)

> **鐩爣**: 瀹炵幇涓夌被椋庢牸鐨勫湴鍥剧敓鎴愮瓥鐣?

### Task 3.1: IBiomeStrategy 鎺ュ彛瀹氫箟 鈴憋笍 0.5h
- [x] 瀹氫箟 `IBiomeStrategy` 铏氬熀绫?- [x] 瀹氫箟 `GenerateTerrain()` 鍜?`PlaceSpecialStructures()` 鎺ュ彛
- [x] 瀹氫箟閫氱敤鍙傛暟缁撴瀯浣?
**鏂板缓鏂囦欢**:
```
src/game/systems/world/BiomeStrategies.hpp
```

### Task 3.2: OpenBiomeStrategy (A缁? 鈴憋笍 2h
- [x] 瀹炵幇浣庡鐜?CA 鐢熸垚 (wallProb 0.15-0.22)
- [x] 鍑忓皯骞虫粦杩唬娆℃暟 (2-3娆? 淇濇寔绌烘椃
- [x] 绋€鐤忛殰纰嶇墿鏀剧疆
- [x] 楠岃瘉: 鐢熸垚鍦板浘 >70% 涓哄湴鏉?
**鏂板缓鏂囦欢**:
```
src/game/systems/world/OpenBiomeStrategy.cpp
```

### Task 3.3: MazeBiomeStrategy (B缁? 鈴憋笍 3h
- [x] 瀹炵幇楂樺鐜?CA 鐢熸垚 (wallProb 0.38-0.48)
- [x] 澧炲姞骞虫粦杩唬娆℃暟 (5-6娆? 褰㈡垚璧板粖
- [x] 璧板粖瀹藉害鎺у埗 (2-3鏍?
- [x] 姝昏儭鍚屾娴嬩笌鏍囪 (鐢ㄤ簬瀹濈鏀剧疆)
- [x] 楠岃瘉: 鐢熸垚鍦板浘鏈夋槑鏄剧殑閫氶亾缁撴瀯

**鏂板缓鏂囦欢**:
```
src/game/systems/world/MazeBiomeStrategy.cpp
```

### Task 3.4: SpecialBiomeStrategy (C缁? 鈴憋笍 4h
- [x] 瀹炵幇娴┖骞冲彴鐢熸垚 (FloatingIsle, SkyPalace)
- [x] 瀹炵幇妗ユ杩炴帴绠楁硶
- [x] 瀹炵幇涓績绔炴妧鍦?(HolyArena)
- [x] 瀹炵幇鍔ㄦ€佸埛鎬鏀剧疆 (HiveNest)
- [x] 绌烘皵澧欏尯鍩熸爣璁?
**鏂板缓鏂囦欢**:
```
src/game/systems/world/SpecialBiomeStrategy.cpp
```

### Task 3.5: BiomeMapGenerator 闆嗘垚 鈴憋笍 1h
- [x] 鍒涘缓 `BiomeMapGenerator` 绫?- [x] 瀹炵幇 `CreateStrategy(BiomeStyle)` 宸ュ巶鏂规硶
- [x] 瀹炵幇 `GenerateForBiome()` 涓诲叆鍙?- [x] 闆嗘垚鍒?`MapSystem::generateMap()`

**鏂板缓鏂囦欢**:
```
src/game/systems/world/BiomeMapGenerator.hpp
src/game/systems/world/BiomeMapGenerator.cpp
```

### Task 3.6: 杩為€氭€т笌鍑哄彛鏀剧疆 鈴憋笍 1h
- [x] 纭繚鎵€鏈夌瓥鐣ョ敓鎴愮殑鍦板浘杩為€?- [x] 缁熶竴鍑哄彛鏀剧疆閫昏緫 (妤兼銆佷紶閫侀棬)
- [x] 鐗规畩缁撴瀯鍑哄彛浣嶇疆楠岃瘉

---

## 馃煝 Sub-Track 4: 鍔ㄦ€佷氦浜掗€昏緫 (Dynamic Interactions)

> **鐩爣**: 瀹炵幇缇よ惤鐗规畩鏈哄埗

### Task 4.1: DestructibleTileComponent 鈴憋笍 1.5h
- [ ] 瀹氫箟 `DestructibleTileComponent` 缁勪欢
- [ ] 瀹炵幇鍙牬鍧忕摝鐗囩殑浼ゅ鎺ユ敹
- [ ] 瀹炵幇鐮村潖鍚庡湴褰㈠彉鍖?(WALL -> FLOOR)
- [ ] 娣诲姞鐮村潖鐗规晥鍜屾畫楠?

**鏂板缓鏂囦欢**:
```
src/game/components/DestructibleTileComponent.hpp
```

**鏂囦欢淇敼**:
```
src/game/systems/combat/DamageSystem.cpp
src/game/systems/world/MapSystem.cpp
```

### Task 4.2: SpawnerWallComponent 鈴憋笍 2h
- [ ] 瀹氫箟 `SpawnerWallComponent` 缁勪欢
- [ ] 瀹炵幇瀹氭椂鍒锋€€昏緫
- [ ] 涓?`EnemySpawnSystem` 闆嗘垚
- [ ] 娣诲姞瑙嗚鏁堟灉 (瑙︽墜澧欏鍔ㄧ敾)

**鏂板缓鏂囦欢**:
```
src/game/components/SpawnerWallComponent.hpp
```

**鏂囦欢淇敼**:
```
src/game/systems/world/EnemySpawnSystem.cpp
```

### Task 4.3: SpeedZoneComponent 鈴憋笍 1h
- [ ] 瀹氫箟 `SpeedZoneComponent` 缁勪欢
- [ ] 瀹炵幇鍔犻€熷甫瑙﹀彂閫昏緫
- [ ] 涓?`MovementSystem` 闆嗘垚
- [ ] 娣诲姞瑙嗚鏁堟灉 (娴佺嚎绮掑瓙)

**鏂板缓鏂囦欢**:
```
src/game/components/SpeedZoneComponent.hpp
```

### Task 4.4: 杩烽浘瑙嗛噹绯荤粺 鈴憋笍 1.5h
- [ ] 淇敼 `FogOfWar` 鏀寔缇よ惤閰嶇疆鐨勮閲庨檺鍒?
- [ ] 瀹炵幇 `visionRadius` 鍙傛暟搴旂敤
- [ ] 闄愬埗瓒呭嚭瑙嗛噹鑼冨洿鐨勬晫浜烘覆鏌?
- [ ] 涓庡悗澶勭悊婊ら暅鑱斿姩

**鏂囦欢淇敼**:
```
src/game/systems/world/MapSystem.cpp (updateVisibility)
src/game/systems/render/RenderSystem.cpp
```

---

## 馃數 Sub-Track 5: 鐢熸€侀泦鎴?(Monster Ecology)

> **鐩爣**: 瀹屾垚鎬墿姹犱笌缇よ惤鐨勭粦瀹?

### Task 5.1: 绉嶆棌鍚嶇О鏄犲皠琛?鈴憋笍 0.5h
- [ ] 鍒涘缓 `kRaceNameMap` 闈欐€佹槧灏勮〃
- [ ] 鏀寔 JSON 涓殑瀛楃涓插埌 `EnemyRace::Type` 杞崲
- [ ] 娣诲姞澶у皬鍐欎笉鏁忔劅鍖归厤

**鏂囦欢淇敼**:
```
src/game/systems/world/EnemySpawnSystem.cpp
```

### Task 5.2: EnemySpawnSystem 閫傞厤 鈴憋笍 1.5h
- [ ] 淇敼 `initData()` 璇诲彇缇よ惤 `enemyPool`
- [ ] 瀹炵幇 `selectRace()` 鍩轰簬鏉冮噸闅忔満閫夋嫨
- [ ] 楠岃瘉鍩庨晣 `isSafeZone` 涓嶅埛鎬?

**鏂囦欢淇敼**:
```
src/game/systems/world/EnemySpawnSystem.hpp
src/game/systems/world/EnemySpawnSystem.cpp
```

### Task 5.3: 鎺夎惤琛ㄥ叧鑱?(鍙€? 鈴憋笍 1h
- [ ] 鎵╁睍 `LootTable` 鏀寔缇よ惤淇
- [ ] 鐗瑰畾缇よ惤澧炲姞鐗瑰畾鏉愭枡鎺夌巼
- [ ] 涓?`ItemDropSystem` 闆嗘垚

**鏂囦欢淇敼**:
```
src/game/systems/loot/ItemDropSystem.cpp
```

---

## 馃煟 Sub-Track 6: 娴嬭瘯涓庢墦纾?(Testing & Polish)

> **鐩爣**: 纭繚绯荤粺绋冲畾鎬у拰浣撻獙瀹屾暣鎬?

### Task 6.1: 鍗曞厓娴嬭瘯 - 鐢熸垚绠楁硶 鈴憋笍 1h
- [ ] 娴嬭瘯 A缁勫湴鍥惧鐜囧湪 15%-22%
- [ ] 娴嬭瘯 B缁勫湴鍥捐蛋寤婅繛閫氭€?
- [ ] 娴嬭瘯 C缁勫湴鍥剧┖姘斿姝ｇ‘鏍囪
- [ ] 娴嬭瘯鎵€鏈夊湴鍥?`EnsureConnectivity()` 閫氳繃

### Task 6.2: 闆嗘垚娴嬭瘯 - 鍏ㄧ兢钀介亶鍘?鈴憋笍 1.5h
- [ ] 缂栧啓娴嬭瘯鑴氭湰閬嶅巻鎵€鏈?27 绉嶇兢钀?
- [ ] 楠岃瘉姣忎釜缇よ惤姝ｅ父鍔犺浇銆佹覆鏌撱€佸埛鎬?
- [ ] 鎴浘瀛樻。鐢ㄤ簬鍥炲綊娴嬭瘯

### Task 6.3: 鎬ц兘鍩哄噯 鈴憋笍 0.5h
- [ ] 256x256 鍦板浘鐢熸垚鏃堕棿 < 100ms
- [ ] 绌烘皵澧欐覆鏌?FPS 涓嬮檷 < 5%
- [ ] 杩烽浘瑙嗛噹娓叉煋寮€閿€楠岃瘉

### Task 6.4: Bug 淇涓庣粏鑺傛墦纾?鈴憋笍 1h
- [ ] 淇鍙戠幇鐨勯棶棰?
- [ ] 璋冧紭瑙嗚鍙傛暟 (棰滆壊銆佸厜鏁?
- [ ] 瀹屽杽閿欒澶勭悊涓庢棩蹇?

---

## 馃搵 浠诲姟渚濊禆鍥?

```
Phase 1 (鏁版嵁灞?
    鈹?
    鈹溾攢鈹€ 1.1 BiomeID鏋氫妇 鈹€鈹€鈹?
    鈹溾攢鈹€ 1.2 Style/Feature 鈹尖攢鈹€ 1.3 BiomeConfig鎵╁睍 鈹€鈹€ 1.4 JSON瑙ｆ瀽 鈹€鈹€ 1.5 鍗曞厓娴嬭瘯
    鈹?
    v
Phase 2 (娓叉煋/鐗╃悊) 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?
    鈹?                                              鈹?
    鈹溾攢鈹€ 2.1 绌烘皵澧欐爣璁?鈹€鈹€ 2.2 AirWallRenderer       鈹?
    鈹溾攢鈹€ 2.3 鑳屾櫙Shader 鈹€鈹€鈹?                         鈹?
    鈹溾攢鈹€ 2.4 瑙嗚婊ら暅                                鈹?
    鈹斺攢鈹€ 2.5 鐗规畩鐗╃悊                                鈹?
                                                    鈹?
Phase 3 (鐢熸垚绠楁硶) 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?
    鈹?                                              鈹?
    鈹溾攢鈹€ 3.1 IBiomeStrategy鎺ュ彛                      鈹?
    鈹溾攢鈹€ 3.2 OpenBiomeStrategy (A缁?                 鈹?
    鈹溾攢鈹€ 3.3 MazeBiomeStrategy (B缁?                 鈹?
    鈹溾攢鈹€ 3.4 SpecialBiomeStrategy (C缁? 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?
    鈹溾攢鈹€ 3.5 BiomeMapGenerator闆嗘垚                   鈹?
    鈹斺攢鈹€ 3.6 杩為€氭€ч獙璇?                             鈹?
                                                    鈹?
Phase 4 (鍔ㄦ€佷氦浜? 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?
    鈹?                                              鈹?
    鈹溾攢鈹€ 4.1 DestructibleTile                        鈹?
    鈹溾攢鈹€ 4.2 SpawnerWall                             鈹?
    鈹溾攢鈹€ 4.3 SpeedZone                               鈹?
    鈹斺攢鈹€ 4.4 杩烽浘瑙嗛噹                                鈹?
                                                    鈹?
Phase 5 (鐢熸€侀泦鎴? 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?
    鈹?                                              鈹?
    鈹溾攢鈹€ 5.1 绉嶆棌鏄犲皠琛?                             鈹?
    鈹溾攢鈹€ 5.2 EnemySpawnSystem閫傞厤                    鈹?
    鈹斺攢鈹€ 5.3 鎺夎惤琛ㄥ叧鑱?                             鈹?
                                                    鈹?
                                                    v
Phase 6 (娴嬭瘯鎵撶（) 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
    鈹?
    鈹溾攢鈹€ 6.1 鍗曞厓娴嬭瘯
    鈹溾攢鈹€ 6.2 鍏ㄧ兢钀介泦鎴愭祴璇?
    鈹溾攢鈹€ 6.3 鎬ц兘鍩哄噯
    鈹斺攢鈹€ 6.4 Bug淇
```

---

## 馃摑 澶囨敞

1. **骞惰鍙鎬?*: Phase 2/3/4/5 鍦?Phase 1 瀹屾垚鍚庡彲閮ㄥ垎骞惰寮€鍙?
2. **椋庨櫓椤?*: 鐗规畩绛栫暐鐢熸垚 (Task 3.4) 澶嶆潅搴︽渶楂橈紝棰勭暀棰濆缂撳啿鏃堕棿
3. **MVP 绛栫暐**: 鑻ユ椂闂寸揣寮狅紝鍙紭鍏堝畬鎴?A缁?B缁勶紝C缁勭壒娈婃満鍒跺悗缁凯浠?

---

*璁″垝鐗堟湰: 1.0*
*鏈€鍚庢洿鏂? 2026-02-08*

## Sub-Track 4 Progress Update (2026-02-08)
- [x] Task 4.1 `DestructibleTileComponent` + destructible wall damage/destruction path (`MapSystem` + `HazardSystem`).
- [x] Task 4.2 `SpawnerWallComponent` + timed dynamic spawn walls (`MapSystem` + `EnemySpawnSystem`).
- [x] Task 4.3 `SpeedZoneComponent` + speed zone multiplier on movement/physics path (`GameplayState` + `MapSystem`).
- [x] Task 4.4 Limited-vision radius + enemy render culling (`LevelManager`, `GPUEntitySystem`, `RenderSystem`, `UIMinimap`).
- [x] Divergence handled: `DamageSystem.cpp`/`MovementSystem.cpp` are absent in this branch; functionality integrated into active runtime systems.
