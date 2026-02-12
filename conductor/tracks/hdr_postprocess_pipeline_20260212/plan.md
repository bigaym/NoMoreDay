# HDR + 鍚庡鐞嗙绾?瀹炴柦璁″垝 (Plan)

> **Track ID**: `hdr_postprocess_pipeline_20260212`  
> **渚濊禆 Spec**: `spec.md` (V1.0)  
> **棰勮宸ユ椂**: 5~6 澶? 
> **鐘舵€?*: IN_PROGRESS

---

## 闃舵鎬昏

| 闃舵 | 鍚嶇О | 鏍稿績浜у嚭 | 棰勮宸ユ椂 | 鐘舵€?|
|------|------|----------|----------|------|
| **1A** | GPU 鍩虹璁炬柦鎵╁睍 | FBO 鎿嶄綔灏佽, FramebufferManager, FullscreenQuad | 4h | 鉁?宸插畬鎴?|
| **1B** | RenderConfig & QualityTier 鎵╁睍 | Phase 1 閰嶇疆瀛楁, 鍥涙。棰勮 | 1h | 鉁?宸插畬鎴?|
| **1C** | HDR SceneBuffer 闆嗘垚 | Scene/VFX 娓叉煋鍒?RGBA16F FBO | 3h | 鉁?宸插畬鎴?|
| **1D** | Bloom 绠＄嚎 | BrightExtract + Kawase Down/Up + Mip Chain | 6h | 鉁?宸插畬鎴?|
| **1E** | Tonemapping | ACES Filmic + Exposure + Gamma | 2h | 鉁?宸插畬鎴?|
| **1F** | FXAA & Vignette | FXAA 3.11 Quality + 寰勫悜鏆楄 | 3h | 鉁?宸插畬鎴?|
| **1G** | 绠＄嚎缂栨帓 & CompositePass 鍗囩骇 | PostProcessPass 缂栨帓, Low Tier 鍥為€€ | 3h | 鉁?宸插畬鎴?|
| **1H** | 娴嬭瘯 & 楠屾敹 | 鍗曟祴, 鎬ц兘鍩哄噯, 瑙嗚鍥炲綊 | 3h | 鈴?杩涜涓?|

---

## Phase 1A: GPU 鍩虹璁炬柦鎵╁睍 (4h)

### Task 1A.1: GPUUtils FBO 鎿嶄綔鎵╁睍 (~1.5h)
- [x] 鍦?`GPUUtils.hpp` 涓０鏄?FBO/RBO/TexStorage2D/Viewport/DrawArrays/Enable/Disable/BlendFunc 闈欐€佹柟娉?- [x] 鍦?`GPUUtils.cpp` 涓鍔犲搴?`glfwGetProcAddress` 鍔犺浇涓庡嚱鏁版寚閽堣皟鐢?- [x] 鍦?`GPUUtils::Initialize()` 涓姞杞芥柊澧炲嚱鏁版寚閽?- [x] 缂栬瘧楠岃瘉

### Task 1A.2: FramebufferHandle + FramebufferManager (~1.5h)
- [x] 鍒涘缓 `FramebufferHandle.hpp`
- [x] 鍒涘缓 `FramebufferManager.hpp/cpp`锛屽疄鐜?Create/Destroy/Resize
- [x] `Create` 涓牎楠?`CheckFramebufferStatus == GL_FRAMEBUFFER_COMPLETE`
- [x] 缂栬瘧楠岃瘉

### Task 1A.3: FullscreenQuad (~1h)
- [x] 鍒涘缓 `FullscreenQuad.hpp/cpp`
- [x] 瀹炵幇绌?VAO + `DrawArrays(GL_TRIANGLES, 0, 3)` 鍏ㄥ睆涓夎褰?- [x] 瀹炵幇 `Shutdown()` 閲婃斁璧勬簮
- [x] 缂栬瘧楠岃瘉

---

## Phase 1B: RenderConfig & QualityTier 鎵╁睍 (1h)

### Task 1B.1: RenderConfig 鎵╁睍 (~30min)
- [x] 鍦?`RenderConstants.hpp` 澧炲姞 Bloom/FXAA/Vignette 閰嶇疆瀛楁
- [x] 缂栬瘧楠岃瘉

### Task 1B.2: QualityTierManager 鍥涙。棰勮 (~30min)
- [x] 鎸?Spec 3.3 瀹屾垚 Low/Medium/High/Ultra 閰嶇疆鐭╅樀
- [x] 缂栬瘧楠岃瘉

---

## Phase 1C: HDR SceneBuffer 闆嗘垚 (3h)

### Task 1C.1: RenderContext 鎵╁睍 (~15min)
- [x] 鍦?`graph/RenderContext.hpp` 澧炲姞 `hdrSceneBuffer`
- [x] 缂栬瘧楠岃瘉

### Task 1C.2: HDR FBO 鐢熷懡鍛ㄦ湡绠＄悊 (~1h)
- [x] `RenderSystem` 澧炲姞鎸佷箙 `s_hdrSceneBuffer`
- [x] `Initialize()` 鍒涘缓 RGBA16F FBO
- [x] `Shutdown()` 閲婃斁
- [x] `render()` 涓鐞嗙獥鍙ｅ昂瀵稿彉鍖栦笌 Resize

### Task 1C.3: Scene/VFX/UIWorld 娓叉煋閲嶅畾鍚?(~1.5h)
- [x] `bloomEnabled == true` 鏃讹紝Scene/VFX/UIWorld 娓叉煋鍒?HDR FBO
- [x] `bloomEnabled == false` 鏃讹紝鍥為€€鍒?Phase 0 鐩村嚭琛屼负
- [x] 閫氳繃 `RenderContext` 浼犻€?`hdrSceneBuffer`
- [x] 缂栬瘧涓庤繍琛岄獙璇?
---

## Phase 1D: Bloom 绠＄嚎 (6h)

### Task 1D.1: 鍚庡鐞?Shader 缂栧啓 (~2h)
- [x] `assets/shaders/postprocess/fullscreen.vert`
- [x] `assets/shaders/postprocess/bright_extract.frag`
- [x] `assets/shaders/postprocess/kawase_down.frag`
- [x] `assets/shaders/postprocess/kawase_up.frag`

### Task 1D.2: PostProcessPass 楠ㄦ灦 + Bloom Mip Chain (~2h)
- [x] 鍒涘缓 `PostProcessPass.hpp/cpp`
- [x] `Initialize()` 鍔犺浇 shader 骞剁紦瀛?uniform location
- [x] `RebuildBloomMips()` 鍒涘缓閫掑噺鍒嗚鲸鐜?FBO 閾?- [x] `DestroyBloomMips()` + `Shutdown()`
- [x] 缂栬瘧楠岃瘉

### Task 1D.3: Bloom 鎵ц閫昏緫 (~2h)
- [x] 瀹炵幇 `ExecuteBloom()`锛圔rightExtract 鈫?Downsample 鈫?Upsample锛?- [x] 姣忔姝ｇ‘璁剧疆 FBO / Viewport / 杈撳叆绾圭悊
- [x] 杩愯楠岃瘉

---

## Phase 1E: Tonemapping (2h)

### Task 1E.1: Tonemap Shader (~1h)
- [x] 鍒涘缓 `assets/shaders/postprocess/tonemap.frag`
- [x] 瀹炵幇 ACES Filmic + Gamma 2.2
- [x] 鎺ュ叆 `uHDRScene/uBloomTexture/uBloomIntensity/uExposure`

### Task 1E.2: Tonemap 鎵ц閫昏緫 (~1h)
- [x] 瀹炵幇 `ExecuteTonemap()` 杈撳嚭鍒?LDR FBO
- [x] 缁戝畾 HDR + Bloom 杈撳叆绾圭悊
- [x] 榛樿 `uExposure = 1.0`
- [x] 杩愯楠岃瘉

---

## Phase 1F: FXAA & Vignette (3h)

### Task 1F.1: FXAA Shader (~1.5h)
- [x] 鍒涘缓 `assets/shaders/postprocess/fxaa.frag`
- [x] 鍗囩骇涓烘洿璐磋繎 FXAA 3.11 Quality 鐨?3x3 閭诲煙瀹炵幇
- [x] 鎺ュ叆 `uSource` + `uTexelSize`

### Task 1F.2: Vignette Shader (~30min)
- [x] 鍒涘缓 `assets/shaders/postprocess/vignette.frag`
- [x] 瀹炵幇 `smoothstep(radius, radius - 0.45, dist)`
- [x] 鎺ュ叆 `uSource/uIntensity/uRadius`

### Task 1F.3: FXAA & Vignette 鎵ц閫昏緫 (~1h)
- [x] 瀹炵幇 `ExecuteFXAA()`锛圠DR 鈫?ping-pong锛?- [x] 瀹炵幇 `ExecuteVignette()`锛團XAA 杈撳嚭 鈫?鏈€缁堣緭鍑猴級
- [x] 鏍规嵁閰嶇疆椤瑰惎鍋?
---

## Phase 1G: 绠＄嚎缂栨帓 & CompositePass 鍗囩骇 (3h)

### Task 1G.1: PostProcessPass 闆嗘垚 RenderGraph (~1.5h)
- [x] `RenderSystem::render()` 鍦?UIWorldPass 鍚庢彃鍏?PostProcessPass
- [x] `bloomEnabled == false` 鏃惰烦杩?PostProcessPass
- [x] `PostProcessPass::Execute()` 涓茶仈 Bloom 鈫?Tonemap 鈫?FXAA 鈫?Vignette

### Task 1G.2: CompositePass 鍗囩骇 (~1h)
- [x] HDR 寮€鍚椂浠庡悗澶勭悊杈撳嚭 FBO 璇诲彇 LDR 缁撴灉骞舵嫹鍥為粯璁ゅ抚缂撳啿
- [x] HDR 鍏抽棴鏃朵繚鎸?Phase 0 pass-through

### Task 1G.3: 鍒濆鍖?閿€姣佺敓鍛藉懆鏈?(~30min)
- [x] `RenderSystem::Initialize()` 璋冪敤 `PostProcessPass::Initialize()`
- [x] `RenderSystem::Shutdown()` 璋冪敤 `PostProcessPass::Shutdown()` 涓?`FullscreenQuad::Shutdown()`

---

## Phase 1H: 娴嬭瘯 & 楠屾敹 (3h)

### Task 1H.1: 鍗曞厓娴嬭瘯 (~1h)
- [x] 鏂板 `tests/unit/PostProcessTest.cpp`
- [x] 瑕嗙洊 `FramebufferManager_CreateDestroy`
- [x] 瑕嗙洊 `FramebufferManager_Resize`
- [ ] 瑕嗙洊 `BloomMipChain_Levels`
- [x] 瑕嗙洊 `QualityTier_Phase1Config`
- [x] 瑕嗙洊 `PostProcess_LowTierBypass`

### Task 1H.2: 鎬ц兘鍩哄噯鎵╁睍 (~1h)
- [x] 鏂板 `tests/performance/PostProcessBenchmark.cpp`
- [x] 浣跨敤 GPU Timer Query (`GL_TIME_ELAPSED`) 璁℃椂
- [x] 杈撳嚭 Low / Ultra / Delta(Ultra-Low) 鎸囨爣
- [ ] 缁嗗垎 Bloom 涓?Tonemap+FXAA+Vignette 鍒嗘璁℃椂

### Task 1H.3: 瑙嗚鍥炲綊楠屾敹 (~1h)
- [ ] Low Tier 鎴浘涓?Phase 0 鍍忕礌绾у姣?- [ ] Ultra Tier 鏁堟灉鎴浘锛圔loom / Tonemap / FXAA / Vignette锛?- [ ] Resize 20 娆＄ǔ瀹氭€ч獙璇侊紙鏃犲穿婧冦€佹棤娉勬紡锛?- [ ] 30 鍒嗛挓鍘嬪姏鎴樻枟绋冲畾鎬ч獙璇?
---

## Definition of Done

- [x] 鏂板 shader 鍙紪璇?- [x] 鏂板 FBO 鍒涘缓鍧囩粡杩?`CheckFramebufferStatus`
- [ ] Low Tier 涓?Phase 0 瀹屽叏涓€鑷达紙寰呮埅鍥炬牳楠岋級
- [ ] Ultra 瑙嗚鏁堟灉楠屾敹瀹屾垚
- [ ] Bloom 鈮?0.5ms锛孴onemap+FXAA+Vignette 鈮?0.4ms锛堢洰鏍囨満瀹炴祴锛?- [ ] 闀跨ǔ鍘嬫祴涓?Resize 绋冲畾鎬ч€氳繃
- [x] 鏂板鍗曟祴鍙繍琛?- [x] `build.bat` 鏋勫缓閫氳繃

---

*璁″垝鐗堟湰: 1.1*  
*鏈€鍚庢洿鏂? 2026-02-12*
