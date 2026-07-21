# Varjo XR-4 MR 기능 검증 레퍼런스 (CXMR 템플릿 기준)

> 최종 검증: 2026-07-15 / 대상: **VarjoOpenXR 플러그인 v1.2.7** (`.uplugin` EngineVersion 5.7.0, Win64 전용) / 프로젝트 UE 5.7.4 / 하드웨어 **XR-4 (2023, HS-10: inside-out only, fixed focus)**

## 0. 이 문서의 사용법 — 신뢰 위계 (중요)

이 문서를 만들면서 **Varjo의 공식 자료들이 서로 모순된다는 걸 반복적으로 확인**했다. 따라서 근거의 우선순위를 고정한다:

| 순위 | 근거 | 신뢰도 |
|---|---|---|
| **1** | **플러그인 소스 코드** (`Plugins/VarjoPlugin/Source/**`) | **절대** — 실제로 컴파일되어 도는 것 |
| **1.5** | **예제 프로젝트 BP 그래프**(스크린샷 실측, §19) | **매우 높음** — Varjo가 배포한 실제 배선. 문서보다 신뢰 |
| 2 | Varjo 기능별 문서 페이지 (`developer.varjo.com/docs/unreal/ue5/*`) | 높음 — 단, 일부 낡음 |
| 3 | Varjo `supported-features` 매트릭스 | **낮음** — 명백한 오류 확인됨 (§13) |
| 4 | LLM이 요약한 핸드오프 문서 | **사용 금지** — 경고 삭제·트랙 혼합·시그니처 오류 다수 (§13) |

**규칙: 1과 2~4가 충돌하면 무조건 1이 이긴다.** 새 주장은 소스에서 확인하고 나서 쓴다.

---

## 1. 플러그인 BP API 전체 표면 (ground truth)

**Blueprint에서 호출 가능한 것은 이 4개 파일이 전부다.** 여기 없으면 BP로 못 한다.

| 파일 | 내용 |
|---|---|
| `VarjoOpenXR/Public/VarjoOpenXR.h` | `UVarjoOpenXRFunctionLibrary` — MR·Depth·Foveated·Markers 함수 전부 |
| `VarjoOpenXR/Public/VarjoMarkersEvent.h` | `UVarjoMarkersEvent`(ActorComponent) + 3개 이벤트 |
| `VarjoHandInteraction/Public/VarjoHandInteractionFunctionLibrary.h` | `GetHandInteractionAimPose` / `GetHandInteractionGripPose` |
| `VarjoOpenXRRuntimeSettings/Public/VarjoOpenXRRuntimeSettings.h` | RenderingMode / FoveatedRendering 프로젝트 세팅 |

**플러그인의 정체**: 네이티브 Varjo SDK(`varjo.h`) 미사용. 순수 OpenXR extension 플러그인(`IOpenXRExtensionPlugin` + `XR_VARJO_*`). → 네이티브 SDK 전용 기능은 이 경로에 **없다**(§12).

### 함수 목록 (`UVarjoOpenXRFunctionLibrary`)
- **MR**: `IsMixedRealitySupported` / `IsMixedRealityEnabled` / `SetViewOffset(float 0~1)` / `GetViewOffset`
- **Depth**: `SetDepthTestEnabled` / `IsDepthTestSupported` / `IsDepthTestEnabled` / `SetDepthTestRange(bool, NearZ, FarZ)` / `GetDepthTestRange` / `SetEnvironmentDepthEstimationEnabled` / `IsEnvironmentDepthEstimationSupported` / `IsEnvironmentDepthEstimationEnabled`
- **Foveated**: `IsFoveatedRenderingSupported` / `IsFoveatedRenderingEnabled` (setter 없음 — 프로젝트 세팅)
- **Markers**: `IsVarjoMarkersSupported` / `SetVarjoMarkersEnabled` / `IsVarjoMarkersEnabled` / `SetMarkerTimeout(id, sec)` / `SetMarkerTrackingMode(id, mode)` / `GetMarkerTrackingMode`

---

## 2. Mixed Reality (VST / 알파 / blend mode)

**메커니즘**: 플러그인이 패스스루를 직접 합성하지 **않는다**. 엔진 OpenXR 컴포지터의 **alpha-blend 환경 blend mode**에 얹힌다. `IsMixedRealitySupported`는 런타임이 `XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND`를 지원하는지 검사할 뿐(`MixedRealityPlugin.cpp:66`).

**핵심**: *"Pixels with RGBA(0,0,0,0) display only the VST image"* — 알파만 0이 아니라 **RGBA 전부 0**인 픽셀에 패스스루가 보인다.

### 런타임 토글
- ⚠️ **예제 BP는 콘솔 명령이 아니라 `Set Environment Blend Mode` 노드를 쓴다**(New Blend Mode enum: Opaque / Alpha Blend). 문서는 `Execute Console Command xr.OpenXREnvironmentBlendMode 3/1`을 안내하지만, 실제 BP_MRControls는 이 노드를 씀(§19). **이게 더 깔끔한 정답.** (이 노드는 Varjo 플러그인 함수 라이브러리엔 없음 → 엔진 OpenXR 노드로 [추정], in-editor 확인 권장)
- 참고(콘솔 방식): `xr.OpenXREnvironmentBlendMode 3` = Alpha Blend(ON) / `1` = Opaque(OFF)
- MR 가능 기기는 **기본 켜짐**
- ⚠️ *"Unreal Engine 5 doesn't currently do any validation for xr.OpenXREnvironmentBlendMode. Use Varjo-provided functions"* → 반드시 `IsMixedRealitySupported/Enabled`로 확인 (예제도 토글 전에 두 검사를 모두 함)
- 소스 확인: `IsMixedRealityEnabled`는 CVar override(==3) 또는 `EXRSystemFlags::IsAR`를 읽음(`MixedRealityPlugin.cpp:84-97`)

### 알파 채널 설정 (필수)
1. `Project Settings > Rendering > Postprocessing > Enable alpha channel support` = **"Allow through tonemapper"**
2. ⚠️ **품질 Epic 이상이면 색 포맷(PF_FloatRGBA 64bit)의 추가 알파 때문에 패스스루만 보일 수 있음.**
   해결: Engine Scalability의 **Effects를 High**로 낮추거나, **`r.SceneColorFormat 3`** (PF_FloatRGB 32bit) — Level BP의 BeginPlay에서 Execute Console Command
3. **Post Process Volume** 추가 → **Infinite Extent (Unbound)** 켜기
4. 머티리얼 **PP_MR** 생성 → **Material Domain = Post Process**, **Output Alpha 켜기** → PPV의 `Rendering Features > Post Process Materials > Array`에 Asset reference로 추가

### Camera Render Position (`SetViewOffset`) — ⚠️ CXMR 설계 충돌
물리 카메라가 눈보다 **앞에** 있어 가까운 물체가 실제보다 크게 보인다. VST 켜지면 기본적으로 VR 카메라를 물리 카메라 위치에 놓는다.

| 값 | 의미 | 권장 용도 |
|---|---|---|
| **1.0** (MR 시 기본) | 카메라 위치 렌더 — 가상/실물 **크기 일치**, 환경에 안정적 | 나란히 비교 / **물리적 상호작용** |
| **0.0** | 실제 눈 위치 렌더 — 가상이 정확한 크기, 실세계와 스케일 어긋날 수 있음 | **가까이 들여다보기** (머리 움직일 때 자연 시차) |

**차량 실내 평가는 둘 다 요구한다** (실물 휠 만지기 → 1.0, 인테리어 디테일 관찰 → 0.0). 0~1 실시간 보간 가능하므로 **모드 토글로 노출**하는 것이 답일 수 있음. 미해결.

### Background Toggle 패턴
`VROnlyObjects`(Actor Array, Public)에 SkySphere·벽 등을 담고 MR 시 순회하며 visibility off.

---

## 3. Depth Occlusion

**실물이 가상을 가리게 하는 유일한 SW 수단.**

### 🔴 range 밖에서는 실세계가 사라진다 — "구멍 현상"의 정체
> `openxr/mixed-reality` 원문: *"Beyond the specified range in depth testing, **the compositor will ignore depth completely and VR content will overwrite pixels from the video stream**, meaning real-world content becomes invisible in distant areas."*

**버그가 아니라 설계다.** range를 0–0.75m로 두면 **0.75m 너머는 실세계가 통째로 사라지고 VR이 덮는다.** → "구멍 현상 대응으로 range를 좁힌다"는 **방향이 반대** — 좁힐수록 구멍이 커진다.

**CXMR 함의**: 차량 실내에서 "0.5m 스티어링 휠"과 "3m 실내 벽"을 동시에 보이게 하는 것은 **depth test로 불가능**. → 휠은 **masking(§4-1)으로 가야 한다.**

### 🔴 depth buffer 제출이 전제
> `XR_VARJO_environment_depth_estimation` — *"The application must **submit a depth buffer**"* when using this extension.

매트릭스상 "Depth buffer submission | Unreal OpenXR | Yes"지만 **프로젝트에서 켜져 있는지 확인 필요**.

### 🔴 차량 실내는 depth occlusion에 구조적으로 불리하다
`support: using-occlusion`의 "Improving Ranged Occlusion" — 센서는 통제된 조명 + **비반사 표면**에서 최적:
> - Prefer **matte white** surfaces
> - Avoid **reflective materials such as mirrors, glass, and glossy surfaces**
> - **Avoid black and dark materials**
> - Avoid other sources of **infrared illumination**

**차량 인테리어는 대개 검은색 + 윈드실드 유리 + 광택 트림** → 세 조건에 전부 걸린다. **→ 실물 노출은 depth가 아니라 masking(§4-1)으로 간다.** 근거 3중:
1. range 밖은 실세계가 통째로 사라짐(위)
2. 반사면에 취약
3. **검고 어두운 재질에 취약**

### ⚠️ depth occlusion과 chroma key는 동시 사용 불가 — **API가 막지 않는다**
> Developer FAQ 원문: *"You need to select either one. **Nothing in the API stops you enabling both, but it may lead to unexpected behavior.**"*

에러 없이 조용히 이상 동작한다. 둘 중 하나만 켤 것.

### 사용법
- `SetDepthTestEnabled` → `SetEnvironmentDepthEstimationEnabled` 순서
- **기본 범위 0.0–0.75m** — *"typically good for hand occlusion"* (소스 확인: `DepthPlugin.h:45` `DepthTestRangeFarZ = 0.75f`)
- `SetDepthTestRange(Enabled, NearZ, FarZ)` — **단위 미터**. BP 파라미터 기본값은 FarZ=1.0(`VarjoOpenXR.h:77`)로 구조체 기본(0.75)과 다름
- range **비활성** 시 near=0, far=`HUGE_VALF`(`DepthPlugin.cpp:58-59`)
- ⚠️ `IsDepthTestSupported`는 **항상 true 하드코딩**(`DepthPlugin.cpp:32`) — 지원 여부 판정에 쓰지 말 것
- ⚠️ `IsEnvironmentDepthEstimationSupported`는 **런타임 proc-addr resolve 결과**(`DepthPlugin.cpp:104-107`) — 이게 진짜 판정
- **raw depth 데이터 read API 없음** — enable/disable + range만
- Varjo 예제 입력: T(depth test) / E(estimation) / R(range), `AdjustmentSpeed` float 기본 **0.01**

---

## 4. Masking (2종)

### 4-1. Basic alpha mask — **UE만으로 가능. 플러그인 API 불필요**
> ⚠️ 과거 "플러그인에 masking API가 없으므로 masking 불가"로 잘못 결론낸 적 있음. **API가 없는 이유는 필요 없어서다.**

**depth estimation을 쓰지 않으므로 반사면 노이즈와 무관** = 반사 환경 대응의 핵심 카드.

**전체 절차** (요약본이 대부분 누락했던 부분):
1. MR 먼저 켜기(§2)
2. Static Mesh Actor를 마스크 지오메트리로 배치 (예: `MRMask`)
3. `Project Settings > Rendering > Postprocessing > Custom Depth-Stencil Pass` **켜기**
4. **마스크 메시 설정** — `Details > Rendering`에서 **Render CustomDepth Pass 켜기**, 그리고 아래를 **전부 끄기**:
   - Cast Shadow (Lightning)
   - Visible in Reflection Captures
   - Visible in RealTimeSky Captures
   - Visible in Ray tracing
   - **Render in Main Pass** ← 이거 빠지면 마스크가 그냥 보임
   - **Render in Depth Pass**
   - Receives Decal
5. `PP_MR`에서 **SceneTextureSceneDepth를 복제**하고 SceneTextureID를 **CustomDepth**로 변경
6. **Material Parameter Collection** `PP_MRParameters` 생성 + Scalar Parameter `MRMask`
7. 로직: **`CustomDepth < SceneDepth`(마스크가 다른 오브젝트 뒤에 있지 않음) → Opacity 0. 아니면 1.0**
   - 영구 마스킹이면 0.0 고정, 토글하려면 MPC 스칼라 참조
8. BP: `PPMaterialParameters`(MPC-Single, 기본값 PP_MRParameters) + `MRMaskEnabled`(bool) + `UpdatePostProcessing` 함수
9. 입력 `MRMaskToggle`(키 N) — Enhanced Input 권장

**마스크 지오메트리 워크플로**: 실물 오브젝트의 3D 모델을 만들고 위치·회전·스케일을 실물과 정확히 맞춘다. **"the origin of the 3D model needs to match the anchor location in the real world"**. 정렬 수단 = **Varjo Markers / 외부 트래커 / 코드 수동 정렬**.

**품질 향상**: 실시간 환경 반사 + **shadow catcher**로 가상↔실물 블렌딩 개선.

### 4-2. Blend Control Mask — **Unreal은 Varjo Base/multi-app 경유만**
*"submit a masking layer that is used to control the blending between VST and application layer images"*. 알파 마스크와 달리 **다른 애플리케이션을 뚫는다**. 이전의 experimental Chroma Key Masking API를 **deprecate**함.

5개 모드:
| 모드 | 동작 | 요구 |
|---|---|---|
| Restricted | 마스크 영역에 VST 표시 | chroma key 호환 |
| Extended | 마스크 영역에 VR 표시(반대) | chroma key 호환 |
| Reduced | 마스크에 VST, 나머지에 chroma | chroma key 필요 |
| **Depth Test or VST** | 마스크 영역에서 video depth test, 나머지 VST | video depth 필요 |
| **Depth Test or VR** | 마스크 영역에서 video depth test, 나머지 VR | video depth 필요 |

→ **depth test를 영역 한정으로 적용 가능**. 단 Unreal 플러그인 API로는 제출 불가(매트릭스 `Yes 2)` = Varjo Base 또는 multi-app).

### 4-3. Chroma Key — Unreal은 Varjo Base 경유만
*"a predefined color is replaced with virtual content"* — 보통 밝은 초록/파랑(피부색과 대비).

- **공식 use case에 cockpit이 명시**: *"simulator environments with physical controls against green/blue screens"*, *"**masking virtual touch displays in cockpit applications**"*
- Native API 기능: 활성/비활성, 파라미터 설정, **layer flags로 어느 앱 레이어에 영향 줄지 제어**
- **eye tracking 켜면 정확도 향상**: *"user would get more accurate results of chroma key calculations and mask filtering"*
- ⚠️ **성능 비용 있음**: *"has an impact on overall system performance, as video pass-through processing is using extra resources"*
- ⚠️ **환경 민감**: *"requires controlled environment and lighting. **All shadows and reflections are bad**, constant lighting is good"*
- 지원: Native SDK / Unity XR Plugin / **Varjo Base(간이 UI)**. → **Unreal은 Base 경유만**
- Blend Control Mask API가 이전의 experimental Chroma Key Masking API를 deprecate함

### 4-4. Masking으로 하는 occlusion (depth와 별개)
`get-started/mixed-reality`: 실물이 가상을 가리게 하려면 **실물의 1:1 스케일 모델**을 만들어 투명하게 정렬한다. → depth estimation 없이도 occlusion이 되는 경로. 정렬은 마커/트래커/수동.

---

## 5. Varjo Markers

### 5-1. API (소스 확정)
**이벤트** — `UVarjoMarkersEvent` ActorComponent를 액터에 붙이고 `Details > Events`에서 바인딩:

| 이벤트 | 파라미터 | 비고 |
|---|---|---|
| `NewVarjoMarkerDetected` | MarkerId, Position, Rotation, Size | **세션당 ID마다 단 한 번** |
| `VarjoMarkerMoved` | MarkerId, Position, Rotation, Size | pose 갱신마다 |
| `VarjoMarkerLost` | **MarkerId만** | Position/Rotation/Size **없음** |

**좌표계 (검증 완료)**: `xrLocateSpace(space, trackingSpace)` → tracking space pose → `ToFTransform(pose, worldToMetersScale)` → `trackerTransform * trackingToWoldTransform`(UE의 A*B = A적용후B) → **world-space**. BP가 받는 Position/Rotation은 **추가 변환 없이 `SetActorLocation`에 바로 사용 가능**. `GetTrackingToWorldTransform` 이미 적용됨. (`VarjoMarkersPlugin.cpp:86-98`)

**Size**: width/height만. **Unreal units**, World-to-Meters 반영 → 항상 실물 크기와 일치. 크기는 변하지 않으므로 스폰 후 재스케일 불필요. Varjo 예제는 `width / 100`으로 기본 큐브 스케일 보정.

### 5-2. ⚠️ 함정 (소스에서만 발견됨 — 문서에 없음)
1. **`NewVarjoMarkerDetected`는 세션당 ID마다 한 번만.** `markers` 맵은 `PostCreateSession`과 `SetVarjoMarkersEnabled(false)`에서만 Reset되고, Lost는 맵에서 **제거하지 않음**. → 잃었다 다시 잡으면 **Moved가 나옴, Detected 아님**.
   - **재정렬 구현**: `SetVarjoMarkersEnabled(false→true)`로 맵을 리셋해 Detected를 강제 유도 (의도가 명확). Moved를 받는 방식보다 권장.
   - Varjo 문서도 이걸 간접 확인: *"On Varjo Marker Lost, **hide** the Actor rather than destroying it to avoid respawning overhead"*
2. **이벤트 침묵 ≠ 마커 없음.** `POSITION_TRACKED_BIT`가 없으면 Moved도 Lost도 **안 나감**(무음 드롭, `VarjoMarkersPlugin.cpp:87`). 침묵으로 timeout 판정 금지.
3. **`SetMarkerTimeout`/`SetMarkerTrackingMode`는 감지 후에만 성공** (markerId≠0 + 맵 등록됨). → `NewVarjoMarkerDetected` 핸들러 안에서 호출.
4. `displayTime`/`trackingSpace`는 **초기화 안 된 멤버**(`VarjoMarkersPlugin.h:56-57`). `UpdateDeviceLocations`에서만 채워짐 → **세션 시작 직후 첫 마커 이벤트는 신뢰하지 말 것.**
5. `GetTrackingToWorldTransform`은 **이벤트 시점 샘플링**(VR pawn/origin 포함) → pawn origin이 움직이면 고정된 차량과 실물이 조용히 어긋남. **마커 감지~차량 배치 사이 pawn 이동 금지.**
6. **Size는 최초 감지 때 한 번만 캐시**되고 갱신 안 됨(`xrGetMarkerSizeVARJO`가 newMarker 분기 안에만 있음).
7. `VarjoMarkerLost`는 **등록된 적 없는 id에도 발생 가능**(맵 확인 없이 broadcast).

### 5-3. Tracking mode
- **Stationary (기본)**: heavily filtered, less responsive. *"optimal for fixed physical elements like **cockpit components**"*
- **Dynamic**: prediction 사용. ⚠️ **"Enabling prediction will lessen the tracking accuracy of the markers."** → 정확도를 깎는다
- 소스: 이벤트 수신 시 `markerUpdate.isPredicted`로 모드가 자동 반영됨(`VarjoMarkersPlugin.cpp:83`)

### 5-4. Timeout
시야에서 사라진 뒤 Lost로 간주되기까지의 초. **기본 0** (BP DisplayName이 "Set Marker Timeout (Default is ZERO)"). *"recommended to set a longer timeout"*. **Varjo 레퍼런스 BP의 `MarkerTimeout` public float 기본값 = 3초** ← CXMR의 "timeout 3초"는 여기서 온 값.

### 5-5. 물리 스펙 — **정렬 정확도의 상한** ⚠️
| 크기 | 최대 추적거리 | 실용(=최대의 절반 권장) |
|---|---|---|
| Small 25mm | 0.5 m | 0.25 m |
| Medium 50mm | 1 m | 0.5 m |
| **Large 150mm** | **3 m** | **1.5 m** |

→ **대시보드 마커는 150mm.** 50mm는 실용 0.5m라 착석 시 눈–대시 거리에 못 미친다.

**인쇄 = 정확도**:
- ⚠️ **"even a 1 mm difference in size may cause tracking to shift by about 1 cm"** (small 기준)
- **100% scale("actual size"), 사이징 최적화 없이** 인쇄
- **무광 용지 + 무광 잉크.** 잉크젯/피그먼트 OK, **레이저 토너 비권장**(반사)
- **Rich black(CMYK) > 그레이스케일**, 가장 진하고 꽉 찬 검정
- 단면 인쇄만

**배치**:
- 휘지 않게, **평면에만**. 곡면이면 **판지에 먼저 부착**
- **마커 위 반사 회피**
- **같은 ID를 한 환경에 두 번 쓰지 말 것**
- 큰 오브젝트/여러 각도에선 **마커 여러 개가 정확도 향상**

### 5-6. 미해결
- **마커 ID 전체 범위 / object vs environment 대역 규칙**: Varjo 문서에 없음. 플러그인도 markerId만 다루고 구분 로직 없음. → **CXMR이 직접 정의해야 함**
- **동시 추적 가능 개수**: 문서에 없음. 플러그인은 TMap이라 무제한. → **실측 필요**

---

## 6. Tracking / Anchors / Origin

- **재앵커링/recenter API가 플러그인에 없음.** `SetViewOffset`은 눈↔카메라 렌더 오프셋이지 월드 정렬이 아님. → **재정렬은 앱(CXMR) 책임**
- **Inside-out 원점은 "environment setup" 때 정의됨**
- **Origin Override (Varjo Base)**: *"setting the origin to wherever the headset is at the moment"* — API는 아니지만 존재하는 재앵커링 수단
- **Spatial Anchors: Unreal OpenXR = No** (매트릭스). 각주 8) *"Spatial persistance of anchors coming in Varjo Base 4.16"*
- **앵커 종류와 권장** (`anchors-and-masking`):
  - **Varjo Markers** — *"our primary suggestion for locking in masks that do not need to move around and **the user is seated**"* ← CXMR 그 자체
  - **SteamVR Trackers** — 빠른 움직임/서서 걸어다닐 때 권장. ⚠️ **XR-4 HS-10은 base station이 없어 사용 불가** → 서서 하는 시나리오는 대안 없음. **착석 평가로 못 박는 하드웨어적 이유**
  - **Controllers** — *"Using controllers as anchors is not recommended"*
  - **World Origin** — 명시적 앵커 없을 때 기본

---

## 7. Hand Tracking / Interaction — **이중 구조**

⚠️ Varjo의 `hand-tracking-with-unreal5` 문서는 **VarjoHandInteraction 모듈을 모른다**(소스 Copyright 2025 = 문서보다 최신).

| 층 | 출처 | 제공 |
|---|---|---|
| **관절 스켈레톤** | 엔진 **OpenXRHandTracking** (`XR_EXT_hand_tracking`) | Hand Key **Positions / Rotations / Radii** 배열 |
| **상호작용 포즈** | **VarjoHandInteraction 모듈** (`XR_EXT_hand_interaction`) | `GetHandInteractionAimPose` / `GetHandInteractionGripPose` (BP) |

- ⚠️ **실측 정정**: 문서는 `Get Motion Controller Data`(UE 5.7+ deprecated)를 쓰라 하지만, **예제 BP_TrackedHands는 `Get Hand Tracking State` 노드를 쓴다**(§19). 반환 struct = `Handtracking State`(Valid / Device Name / XRSpace Type / Hand / Tracking Status / **Hand Key Locations / Rotations / Radii**). 입력: World Context + **XRSpace Type(Unreal World Space)** + **Hand(Left/Right)**. → 템플릿은 이 노드를 쓴다. (`Get Motion Controller State`도 아니었음)
- VarjoHandInteraction의 **pinch/poke/palm은 C++ motion source로만 존재**하고 BP 함수는 aim/grip뿐(`VarjoHandInteraction.h:78`). poke 인터랙션 하려면 motion source를 직접 써야 함.
- **시각화**(BP_TrackedHands): Instanced Static Mesh(Movable, SM_ChamferCube) + Tick → Clear Instances → Branch(IsEnabled) → 좌/우 각각 `Get Hand Tracking State` → Key Locations LENGTH>0 → **For Loop(첫 인덱스 1부터)** → GET loc/rot/radii → Make Transform(scale=radii×0.01) → `Add Instance World Space`
- ⚠️ **Motion Controller 컴포넌트에 visualization이 켜져 있고 실물 컨트롤러가 없으면 손 위치에 컨트롤러 모델이 렌더된다.** `Tracking Status == Not Tracked`일 때 숨길 것.

### Ultraleap
- **Ultraleap Tracking Plugin** 사용 + Leap Options에서 **"Use OpenXR As Source" 비활성**(풀 기능)
- OpenXR를 소스로 쓸 땐 HMD offset 필요 — **XR-4는 X:0, Y:0, Z:0**
- `IEPawnHands`의 **`Get HMDOffsets`** 함수의 **Switch on Name** 노드에 XR-4 case 추가
- **Varjo 자체 손추적은 offset 불필요**

---

## 8. Eye Tracking / Gaze / Foveated

### Gaze
- `XR_EXT_eye_gaze_interaction` → 엔진 **OpenXREyeTracker** 플러그인 (Varjo OpenXR 켜면 자동 활성)
- **플러그인 자체는 gaze를 BP로 노출하지 않는다.** 내부적으로 `XR_REFERENCE_SPACE_TYPE_COMBINED_EYE_VARJO`를 **foveation 전용**으로만 사용(`FoveatedRenderingPlugin.cpp:46`)
- `GetGazeData` 노드 → gaze origin(위치) / direction(회전) / valid(bool). `Break EyeTrackerGazeData`로 분해
- ⚠️ **제공되는 건 combined gaze pose뿐.** *"Fixation point and confidence values are not currently supported with OpenXREyeTracker plugin."* stereo gaze·pupil·calibration 모드 미지원
- 매트릭스: **Eye tracking analytics = Unreal OpenXR "No"** (Native SDK 전용)
- 시각화: 물리 끄기, translucent + depth test 끈 머티리얼, `LineTraceByChannel`(Visibility), 약 **10m**
- (UE 4.27 버그 — gaze가 한 번 valid면 계속 true 반환. 5.7엔 무관)

### Foveated Rendering
- 지원 판정: `XrSystemFoveatedRenderingPropertiesVARJO`(`FoveatedRenderingPlugin.cpp:26-33`)
- 활성: 프로젝트 세팅 `FoveatedRendering`(기본 **true**) + RenderingMode **QuadView**
- ⚠️ **Stereo 모드면 foveation 완전 상실** — `InViewConfigurationType != XR_VIEW_CONFIGURATION_TYPE_PRIMARY_QUAD_VARJO`면 즉시 early-return(`FoveatedRenderingPlugin.cpp:58`)
- **제어 파라미터는 on/off + QuadView/Stereo뿐.** 런타임 setter·gaze 포인트 파라미터 없음. BP엔 `Is...Supported/Enabled`만
- fixed-focus 하드웨어와 무관(foveation은 초점이 아니라 해상도)
- 소스 TODO 주석: gaze 없을 때 fallback 전략 미흡, 동적 토글하려면 swapchain 재할당 필요("lack of Unreal Engine API")

---

## 9. Controllers

⚠️ **문서(`unreal5-controller-inputs`)가 낡았다.** 문서는 *"Vive and Index controller input mappings"*만 언급하지만, **소스엔 XR-4 전용 지원이 완전히 구현돼 있다**:
- 확장 `XR_VARJO_xr4_controller_interaction`, 프로파일 **`/interaction_profiles/varjo/xr-4_controller`** (`VarjoController.cpp:153,159`)
- 키 등록: L/R 각각 A/B Click·Touch, System Click, Grip Click·Touch, Trigger Touch·Axis, Thumbstick X/Y/2D/Click/Touch
- **햅틱 지원**(`GetInteractionProfile`의 `OutHasHaptics = true`)
- 컨트롤러 모델 에셋: `/VarjoOpenXR/Devices/Models/Varjo/varjo_controller_1_0_{left,right}`
- 매트릭스도 "Varjo controllers = Yes"로 소스와 일치 → **문서만 뒤처짐**

컨트롤러 입력·트래킹 자체는 엔진 내장 OpenXR 담당(Varjo 플러그인 없이도 동작). 바인딩은 `Project Settings > Input > Bindings`. UE 5.0+ VR Template이 출발점.

---

## 10. 렌더링 트랙 — **MR은 Forward** (확정)

Varjo `unreal5-recommended-settings` 원문 구조:

| 층 | 내용 |
|---|---|
| **일반 권장 (전부)** | Instanced Stereo **on** / Foveated **on** / Hardware Occlusion Queries **off** |
| **"UE 5.1 → DEFERRED RENDERING"** (별도 섹션) | Forward Shading off, Nanite on, Lumen, HW RT, TSR… |

**그 Deferred 섹션에 Varjo가 직접 박은 경고 (verbatim)**:
> - *"This approach should be considered as an **Experimental and not production ready** yet."*
> - *"**Nanite is incompatible with QuadView** and may render see-through on the headset"*
> - *"**Reflections and QuadView have issues** in Deferred rendering, and **Lighting will be very unpredictable in QuadView**."*
> - *"As Epic Games does not officially recommend to use this method for VR, even though it is possible"*

### CXMR 확정
**Forward Shading 유지 / Nanite off / Foveated(QuadView) on / Instanced Stereo·Manual Exposure·TSR 유지**

- **정확한 인과: Forward 선택 → Nanite 자동 불가**(Nanite는 Deferred 전용). QuadView 호환성은 **무관한 변수** — "QuadView 포기하면 Nanite 되나?"는 잘못된 질문. 재논의 금지.
- **QuadView가 비용의 원인이지 Forward가 아님**: 위 3번째 경고대로 Deferred로 가도 QuadView에선 반사·라이팅이 깨진다 → **Forward 선택은 사실상 공짜**
- ⚠️ 위 페이지는 **UE 5.1 기준**. 5.7에서 Nanite×QuadView 상태는 미확정(Forward면 무관)

### Hardware Occlusion Queries
*"can cause them to 'pop in'... can further cause rendering artefacts with Varjo's **four-viewport setup**, especially when using foveated rendering"* → `Project Settings > Rendering > Culling`에서 **Occlusion Culling 해제**
(`achieving-performance`도 확인: *"Quad view may cause edge artifacts if the engine uses culling"*)

### 10-1. QuadView 아티팩트 — Unreal 전용 (원문 근거)
`achieving-performace-with-high-resolution-rendering`의 **Unreal-Specific Topics**:

- **"Lumen doesn't work with quad view, falling back to ISR"** ← Developer FAQ *"Why is Lumen not working?"*의 답. Forward 결정의 추가 근거
- **Auto Exposure**: *"camera auto-exposure enabled produces **white square artifacts**; disabling auto exposure and manual adjustment fixes this"* ← FAQ *"Dark square is displayed in focus area"*의 정체. **Manual Exposure는 취향이 아니라 아티팩트 수정**
- **Screen Space Reflections**: reflection blend artifacts
- **Transparent Objects**: atmosphere, clouds, glass materials가 아티팩트 유발
- **Different LODs**: 오브젝트에 접근할 때 그림자/외형 아티팩트
- Unreal 디버그 텍스트가 focus 디스플레이(중앙)에 뜨면 → CVar **`DISABLEALLSCREENMESSAGES`**

**blend artifact 원리**: foveal/peripheral 뷰가 겹치는 영역에서 내용이 정확히 일치하지 않으면 **경계에 사각형 아티팩트**. foveation이면 시선을 따라 움직이고, fixed foveation이면 고정. 원인 = 뷰포트별 다른 LOD / screen-space·temporal 효과 / TAA(경미).

**Fixed foveation 옵션**: foveal 뷰를 시선 대신 **뷰포트 중앙에 고정**하는 것도 가능 — *"sometimes preferable if foveation creates artifacts"*.

**Stereo는 권장 안 됨**: *"Generally not recommended due to inferior image quality and performance compared to quad view"* → Forward+QuadView 유지가 맞음.

### 10-2. 포스트프로세스 화이트리스트 / 블랙리스트
`get-started/Post-processing` 원문. 원리: *"avoid **Shaders and effects that use screen space or per-pixel information**"* — 뷰포트별 픽셀 밀도가 달라 **foveal 영역에 시선을 따라다니는 변색**이 생김.

| ✅ 안전 | ❌ 금지 |
|---|---|
| White balance | Automatic/dynamic exposure |
| Channel mixer | Bloom |
| Colour adjustments | Vignette |
| Color curves | Chromatic aberration |
| Lift, Gamma, Gain | Film grain |
| Shadows, Midtones, Highlights | Depth of Field |
| Split toning | Lens distortion |
| **Fixed exposure** | Motion blur |
| | Panini projection |
| | Screen Space Ambient Occlusion |
| | Screen Space reflection |

### 10-3. 성능 — Varjo Base 사용자 설정이 앱 성능을 좌우한다
- **해상도**: Varjo Base의 **"Resolution Quality"** 설정 × 렌더 모드로 결정. 기본값 권장.
- ⚠️ **"XR functions consume additional GPU resources and synchronize framerates between VR and camera feeds"** → **MR을 켜는 것 자체가 비용**이고 VR 프레임레이트가 카메라 피드에 동기화됨. *"XR functions should only be enabled when actively used."*
- ⚠️ **Motion Smoothing의 숨은 비용 (Unreal에 직격)**: velocity vector를 쓰는데 *"If not provided, they're **generated from previous frame RGB images, consuming computation time and reducing base performance**"*. 그런데 매트릭스상 **Velocity buffer submission | Unreal OpenXR = No** → **Unreal 앱은 velocity를 못 내므로 Base에서 Motion Smoothing이 켜져 있으면 자동으로 성능 페널티.**
  - 90fps 달성 시 영향 없음. 45–89fps → **45fps로 락**(실제/합성 교대). 45 미만 → 3프레임 중 2개 합성. 30 미만 → 매우 나쁨. **최소 30fps 목표.**
  - 옵션: "Enabled if app supports it" / "Always enabled" / "Disabled". **VSync 필수.**
- **VSync**: 45–89 → 45fps 락, 30–44 → 30fps 락. 끄면 90fps 초과 방지. **커스텀 fps 락보다 Base의 락 사용 권장.**
- **VRS**(RTX): 플러그인이 해주지 않음. 개발자가 gaze projection 계산 + VRS 맵 구성 + 아티팩트 완화를 직접 해야 함. 현 단계 범위 밖.

---

## 11. 물리 환경 요구사항 (반사 환경 대응)

### 조도 — **최적 1000 lux 이상**
| lux | 결과 |
|---|---|
| 1100 | Perfect |
| 400 | Good |
| 260 | Acceptable |
| 160 | 노이즈 보이나 헤드셋에선 수용 가능 |
| 110 | 노이즈 + 성능 저하 |
| < 50 | 매우 noisy, 성능 나쁨 |

### 그 외
- **색온도 3000–6500K**, **일관성이 핵심**. 창문 자연광이 setup을 망침(특히 blue를 chroma color로 쓸 때). 색온도 다른 조명 혼용 금지
- **LED 또는 인버터 형광등**(플리커 방지). **>1kHz 최적**, 50/60Hz는 flicker compensation과 함께면 허용
- **spotlight보다 diffuse**, **간접 조명** 배치(사용자가 광원을 직접 보지 않게)
- 마스킹/chroma 면: **가능한 한 빛을 적게 반사하는 무광 fabric**, 고채도 순색
- 카메라 세팅을 렌더링 엔진 파라미터와 맞출 것. auto white balance가 문제되면 **잠글 것**

→ 기존 결론 **"무광 처리가 최고 가성비 투자"**가 원문으로 뒷받침됨. 이제 **측정 가능한 기준(1000 lux)** 확보.

### 🟢 반사 환경을 **측정**하는 도구가 있다
**Varjo Base > More > Analytics window > Performance > tracking environment evaluation > Show**
헤드셋을 실제 사용 시 볼 방향들(스크린·벽·천장)로 **천천히** 돌린다. 판정:

| 결과 | 의미 |
|---|---|
| **Green** | *"Your current view is optimal for inside-out tracking"* |
| **Yellow** | *"Tracking is degraded but not yet lost"* |
| **Red** | *"Tracking is lost"* |

**Yellow/Red 원인**: 텍스처 있는 표면 부족 / 조명 부적절 / **헤드셋 전면판이 벽에 너무 가까움** / 움직이는 것이 너무 많이 보임 / 헤드셋 급속 이동.
**개선**: *"Add items like furniture, posters, or decorations"*, 조명 개선 → **개선 후 Room Setup 재실행**.

→ **buck 환경을 주관이 아니라 수치로 평가할 수 있다.** M1/M2 전에 반드시 실행.

### depth occlusion(ranged)용 환경 — §3 참조
matte white 선호 / 거울·유리·광택 회피 / **검고 어두운 재질 회피** / IR 광원 회피.

### hand occlusion 마스크 품질 개선
조명 개선 / 손추적을 'Automatic' 또는 'Varjo hand tracking'으로 / **손 뒤 배경에 손과 색이 뚜렷이 다른 물체 배치** / 손 뒤 디스플레이·조명 낮추기 / 장갑·장신구·밝은 매니큐어·시계 제거. (문신이 품질에 영향 줄 수 있음)

---

## 12. Unreal에서 **안 되는** 것 (Native SDK / Varjo Base 경계)

`supported-features` 매트릭스 **Unreal OpenXR** 열 기준 (§13의 오류 주의):

| 기능 | Unreal | 비고 |
|---|---|---|
| MR environment reflections | **No** | Native SDK + Unity XR SDK 전용 → **차체에 실세계 반사 못 얹음** |
| MR post-process shaders | No | Native SDK만 (각주 1: Experimental) |
| MR datastream (raw camera) | No | Native SDK / Unity XR SDK |
| 3D reconstruction | No | Native SDK만 (Experimental) |
| Eye tracking analytics (pupil/fixation) | No | Native SDK |
| Eye camera datastream | No | Native SDK만 |
| Velocity buffer submission | No | Native SDK만 |
| Multi-app support | No | Native SDK / Unity XR SDK |
| Spatial Anchors | No | Varjo Base 4.16에서 persistence 예정 |
| MR camera settings (노출/화벨) | **Yes ²⁾** | ²⁾ = **Varjo Base 또는 multi-app 경유**. 플러그인 API 아님 |
| Chroma keying | **Yes ²⁾** | 동일 — Varjo Base 경유 |
| Blend control mask layer | **Yes ²⁾** | 동일 |

**AI 평가용 raw 픽셀·gaze analytics는 이 템플릿으로 못 얻는다** → Native SDK 확장 지점으로 남긴다.

### 12-1. 사실적 MR 라이팅 — 4기법 중 1개만 엔진에서 가능
`native/creating-realistic-mr` 기준:

| 기법 | 요구 | Unreal 가능? |
|---|---|---|
| **Virtual shadow casting** | **엔진 렌더 기법만** | ✅ **가능** |
| Environment cubemap 실시간 반사 | Native SDK (`varjo_Lock` + `varjo_MRSetEnviromentCubemapConfig`) | ❌ |
| VR 출력↔비디오 밝기/화벨 매칭 | Native SDK (DistortedColor 스트림 구독) | ❌ |
| 화벨 정규화 post-process | Native SDK (스트림 메타데이터) | ❌ |

**✅ Shadow catcher (Unreal에서 구현 가능)**: 실제 바닥과 같은 위치에 바닥 모델을 만들고, 그림자를 **"black VR color and write the shadow intensity into the alpha channel"**로 렌더. → **PP_MR 알파 구조에 그대로 얹힌다.** 차량 접지감 향상에 유효.

**❌ EnvironmentCubemap (Native 전용, 참고)**: 실시간 조명 데이터. v2.4.0부터 HDR, *"accurate luminance for both direct and indirect sources of light"*, RGB(1,1,1) = 100 cd/m² @ 6500K. 모드 2종 — Fixed 6500K(기본, 수동 후처리 필요) / **AutoAdapt**(VST에 자동 매칭, 후처리 불필요). → **차체 실세계 반사가 Unreal에서 불가한 이유의 원문 근거.**

---

## 13. ⚠️ Varjo 자체 자료의 확인된 오류 (근거로 쓰지 말 것)

1. **`supported-features` 매트릭스: "Varjo Markers | Unreal OpenXR | **No**" → 명백한 오류.**
   플러그인에 `XR_VARJO_MARKER_TRACKING` 풀 구현 + `markers-with-unreal5` 문서 전체가 존재. **소스가 이긴다.**
2. **`unreal5-controller-inputs`: "Vive and Index만 지원" → 낡음.** 소스에 XR-4 전용 프로파일·키·햅틱 완비(§9).
3. **`hand-tracking-with-unreal5`: VarjoHandInteraction 모듈 누락.** 문서가 플러그인보다 낡음(모듈 Copyright 2025). `Get Motion Controller Data`도 UE 5.7에서 deprecated.
4. **`unreal5-known-issues` 페이지는 빈 껍데기** — [Developer FAQ](https://support.varjo.com/hc/en-us/developer-faq#unreal-sdk)로 리다이렉트.
5. **매트릭스 "MR alpha masks = No"는 맥락 주의**: *플러그인 API가* 없다는 뜻. 엔진 Custom Depth 기법으론 **가능**(§4-1).
6. **LLM 요약 핸드오프 문서의 오류** (사용 금지):
   - experimental Deferred 트랙의 스텝을 "Recommended Settings" 일반 권장으로 둔갑 + 경고 4개 삭제
   - masking 절차의 7개 필수 off 설정 중 대부분 누락 → 그대로 하면 **작동 안 함**
   - "Detected·Lost가 Position/Rotation/Size 제공" → **Lost는 ID만**
   - "VarjoMixedReality 클래스" → 존재하지 않음(v1.2.7은 `UVarjoOpenXRFunctionLibrary`)
   - 카메라 노출/화벨·reflection cubemap·BP_SessionPriority를 UE 플러그인 기능으로 배치 → 전부 소스에 없음
   - 존재하지 않는 "F0~F8 분석" 참조

---

## 14. 미해결 / 실측 필요

| # | 항목 | 방법 |
|---|---|---|
| 1 | **마커 ID 체계** (calibration/dynamic/예약 대역) | Varjo 표준 없음 → **CXMR이 직접 정의. 최우선** |
| 2 | 동시 추적 가능 마커 개수 | 실측 |
| 3 | `IsEnvironmentDepthEstimationSupported` 실제 반환값 | 헤드셋에서 BP 호출 |
| 4 | 반사 환경에서 depth occlusion 품질 | estimation ON + range 조절하며 A/B |
| 5 | `SetViewOffset` 0.0 vs 1.0 — 실내 평가에 뭐가 맞나 | 두 모드 토글로 비교 |
| 6 | Developer FAQ 개별 답변 | "Unreal 5.6 smearing", "Lumen not working", "black screen", "dark square in focus area" 등 |
| 7 | UE 5.7에서 Nanite×QuadView 현황 | Forward로 가면 무관 (참고용) |
| 8 | 마커 Stationary 실제 안정성 | `VarjoMarkerMoved` 빈도/떨림 로깅, 반사면 A/B |
| **9** | **멀티마커 차량 캘리브레이션** (단일 마커 불안정 해결) | 단일 마커는 각도 노이즈가 lever-arm으로 증폭 → 2+ 마커로 baseline 기반 yaw + floor 가정. **설계 진행 중**(§18 캘리브레이션 참조). 먼저 불안정의 종류(정렬 순간 노이즈 vs freeze 후 드리프트 vs 상시 오프셋) 진단 필요 |

## 15. Varjo Base 라이선스 게이팅 (Standard vs Pro)

> 출처: https://support.varjo.com/hc/en-us/varjo-base-pro-for-xr-4

| ✅ **Standard(무료)에 포함** | 🔒 **Pro 전용** |
|---|---|
| **Varjo Markers** | Hand occlusion / People occlusion(Beta) |
| **Depth occlusion** | **Masking in Varjo Base** |
| **Chroma key** | **Environment cubemap for HDR lighting** |
| Autofocus cameras | **Programmatic control for camera settings** |
| | **Multi-app support** |
| | Video pass-through data stream / Video post-process shader |
| | 3D reconstruction / Night mode |
| | **Eye tracking for analytics and research** |
| | Support for 3rd party tracking plugins |
| | Programmatic control for IPD adjustment |
| | Varjo CLI / Varjo Base configuration file |

### CXMR 영향 — **핵심 경로는 전부 Standard. 안전.**
- **마커 캘리브레이션 = Standard** ✅ (주 calibration 수단)
- **depth occlusion = Standard** ✅
- **스티어링 휠 masking = UE Custom Depth(앱 사이드)** → "Masking in Varjo Base"(Pro)와 **무관** ✅
- ⚠️ 매트릭스의 *"MR camera settings | Unreal | Yes ²⁾(Base 경유)"*는 실제로 **Pro 필요**(노출/화벨 프로그램 제어). 
- ⚠️ Base의 **"Occlusion" UI 기능**(Settings > Mixed reality > Mixed reality effects > Occlusion)은 **Pro + Base 4.16+**. 앱 사이드 depth occlusion(플러그인의 `SetEnvironmentDepthEstimationEnabled`)과 **다른 것** — 후자는 Standard. **실측으로 확인 권장.**

### Varjo Base의 Occlusion 기능 (Pro, 4.16+, 참고)
Settings > Mixed reality > Mixed reality effects > Occlusion. 기본 꺼짐.
*"When this setting is enabled, mixed reality will be turned on automatically, and **the camera render position may change slightly**"*
- **All content in range** (거리 슬라이더) / **Hands** / **People (Beta)**
- **Depth awareness**: 앱이 공간 레이아웃 데이터를 주면 실물이 가상과의 전후 관계로 표시됨. **주지 않으면 실물이 항상 보이게 됨**(위치 무관) → §3의 depth buffer 제출 요구와 연결

### Varjo Base 마스킹 (Pro, 4.15+) — **템플릿엔 부적합**
코드 없이 마스킹 가능한 경로. `.obj`(단일 메시, 머티리얼 불요, 삼각분할 권장, **1 unit = 1 meter**, **원점 = 앵커 위치**)를 Settings > Mixed reality > Mixed reality effects > Masking > Add mask로 로드. Lab Tools 마스크도 가능(4.11+, Pro). ⚠️ *"you need to close Varjo Lab Tools when using masks through Varjo Base"*

**콘텐츠 모드 5종**: Real world / Virtual content / Real world and chroma key(chroma 필요) / Virtual content and real world with depth occlusion(depth 또는 hand tracking 필요) / Real world with depth occlusion
**앵커 3종**: **Room origin**(room setup 시 정해진 좌표에 고정) / **Varjo Marker**(Preserved(static) 또는 Dynamic + timeout 조절) / SteamVR tracker
**조정**: Position(cm) / Rotation(도) / Scale(**음수면 미러링**)

**❌ 템플릿에 못 쓰는 이유**: Pro 필요 + **"the Varjo Base settings export does not include masking settings"** → 머신마다 수동 설정, **이식 불가**. 공용 템플릿 목적에 정면 배치. → **UE Custom Depth(§4-1)가 맞다.**

---

## 16. 트러블슈팅 (Developer FAQ 확정)

- **패키징 빌드가 헤드셋에서 검은 화면** — 3대 원인:
  1. **OpenXR Toolkit 설치됨** → OpenXR Toolkit Settings에서 **Disable**
  2. **커스텀 OpenXR API 레이어** → Varjo Base > Settings > System > Compatibility > OpenXR API layers 확인
  3. **OpenXR 런타임 오설정** → Varjo Base > Settings > System > Compatibility에서 **OpenXR 껐다 켜기**
- **UE 5.6 smearing** — *"Unreal Engine **5.6.0 and 5.6.1** both have issues in VR/XR rendering producing a smearing image when looking around... more pronounced with Quadview tracking the user's eyes."* → ✅ **"The smearing has been corrected in UE5.7."** **5.7.4 사용 중이므로 해당 없음.** (5.6에 묶이면 `SceneRendering.cpp` 소스 수정 필요)
- **Lumen이 안 됨** — quad view와 비호환, ISR로 폴백(§10-1)
- **focus 영역에 어두운/흰 사각형** — **Auto Exposure** 때문. Manual로(§10-1)
- **eye/hand tracking이 "Unsupported"** — UE **5.3 이상**이면 OpenXREyeTracker / OpenXRHandTracking으로 해결. 5.7이면 무관
- **hand tracking 구현체 전환** — Varjo Base > Settings > System > Experimental > Hand tracking에서 **Varjo ↔ Ultraleap** 전환. Varjo 권장은 "Varjo hand tracking" 또는 "Ultraleap Plugin for Unreal Engine"
- Unreal 디버그 텍스트가 focus 디스플레이에 → CVar `DISABLEALLSCREENMESSAGES`

---

## 17. 검증된 DefaultEngine.ini 설정 (예제 프로젝트 실측)

> 출처: `Downloads/VarjoOpenXRGame/Config/DefaultEngine.ini` (Varjo 공식 예제, VR Template 기반). **문서보다 신뢰도 높음** — Varjo가 실제로 검증해 배포한 config. 단 예제엔 VR Template/Android/Resonance Audio 잔재가 섞여 있으니 아래는 **MR 핵심만 추린 것**.

### MR 필수 (Forward 트랙) — 반드시 있어야
```ini
[/Script/Engine.RendererSettings]
r.ForwardShading=True                    ; MR 알파 합성 안정 (Deferred 아님)
r.PostProcessing.PropagateAlpha=True     ; 알파 채널 = MR 핵심. 없으면 패스스루 합성 안 됨
r.DefaultBackBufferPixelFormat=4         ; 알파 가능한 백버퍼 포맷
r.CustomDepth=1                          ; masking 전제조건 (§4-1)
r.CustomDepthTemporalAAJitter=True
r.DefaultFeature.AutoExposure=False      ; focus 영역 흰 사각형 버그 방지 (§10-1)
r.DefaultFeature.Bloom=False             ; 포스트프로세스 블랙리스트 (§10-2)
r.DefaultFeature.MotionBlur=False
r.DefaultFeature.LensFlare=False
r.AntiAliasingMethod=3                    ; TSR
r.SeparateTranslucency=False
r.AllowOcclusionQueries=False            ; 4-뷰포트 pop-in 아티팩트 방지 (§10)
vr.RoundRobinOcclusion=False
vr.InstancedStereo=True                  ; 성능 권장

[/Script/VarjoOpenXRRuntimeSettings.VarjoOpenXRRuntimeSettings]
FoveatedRendering=True
RenderingMode=VarjoRenderingMode_QuadView
```
(Varjo 런타임 섹션은 없어도 플러그인 C++ 기본값이 동일 — QuadView + Foveated=True. 명시가 안전.)

### 예제가 쓰는 부수 설정 (참고)
`r.AllowStaticLighting=True`, `r.GenerateMeshDistanceFields=True`, `r.RayTracing=False`, `r.SkinCache.CompileShaders=False`, `r.BasePassOutputsVelocity=False`(velocity off — Unreal이 velocity buffer 못 냄과 일치, §10-3), `r.MobileHDR=False`, `vr.MobileMultiView=True`.

### ⚠️ Deferred/블랭크 프로젝트에서 넘어올 때 제거·전환할 것
```ini
r.DynamicGlobalIlluminationMethod=1   ; Lumen GI — Forward에서 무력화. 제거 권장
r.ReflectionMethod=1                  ; Lumen 반사 — 동일
r.RayTracing=True                     ; 예제는 False
r.Substrate=True                      ; ⚠️ 새 재질 저작 체계. 예제 미사용.
                                      ;   공용 템플릿이면 Studio 차량 머티리얼 전제까지 결정하는 사안 — 의식적으로 선택
```

### ⚠️ VR Template 잔재 — 복사 금지
`TargetedHardwareClass=Mobile`/`Scalable`(VR Template 기본, 고충실도 desktop엔 재검토), Resonance Audio, Android/Oculus 섹션, VRTemplate 맵/게임모드, 대량의 CollisionProfile.

### 현 GMTCK_MR 상태 (2026-07-16 대조 결과)
프로젝트가 **`TP_BlankBP` 기반 Lumen/Substrate/RayTracing/Deferred 블랭크**로 설정돼 있음 — MR 트랙과 정반대. 위 "MR 필수" 세트가 **전부 빠져 있고**, Lumen/Substrate/RayTracing이 켜져 있음. Blank 시작이라 VR pawn/입력 스캐폴딩도 없음(예제에서 노드 복사로 충당).

---

## 18. 예제 BP 실측 — 실제 그래프 배선 (스크린샷, 2026-07-18)

> 출처: `Downloads/VarjoOpenXRGame` 예제 프로젝트 BP 스크린샷. **문서보다 신뢰도 높음**(신뢰 위계 1.5). CXMR은 이 배선을 거의 그대로 이관 가능.

### 공통 토글 패턴 (모든 BP_*Controls)
`Enhanced Input Action` → `Is X Enabled`(현재 상태) → **NOT** → `Set X`. 상태를 별도 bool로 안 들고 **런타임 상태를 되읽어 반전**. (일관되게 이 패턴. MR만 `Triggered`, 나머지는 `Completed` 트리거 사용 — 사소한 불일치)
각 Controls는 BeginPlay에서 `Get Player Controller` → `Enable Input`.

### BP_MRControls (변수: MRBackgroundEnabled, MRMaskEnabled, MRMaskObject, PPMaterialParameters(MPC), VROnlyObjects(Actor[]), VRPawn)
- **MR 렌더 토글**: `IA_Varjo_MRToggle` → `Is Mixed Reality Supported` → `Is Mixed Reality Enabled` → **`Set Environment Blend Mode`** 노드(Opaque ↔ Alpha Blend). **콘솔 명령 아님**(§2 정정).
- **ViewOffset 토글**: `IA_Varjo_ViewOffsetToggle` → `Is MR Supported` → `Get View Offset` < 0.5 판정 → **Timeline(0~1)** → `Set View Offset`. → **0↔1 부드러운 보간**. §2에서 예측한 "모드 토글" 방식이 예제에 이미 구현됨.
- **배경 토글**: `IA_Varjo_MRBackgroundToggle` → SET MRBackgroundEnabled(NOT) → For Each `VROnlyObjects` → `Set Visibility`(Propagate to Children).
- **마스크 토글**: `IA_Varjo_MRMaskToggle` → SET MRMaskEnabled(NOT) → `UpdatePostProcessing`.
- **UpdatePostProcessing**: `Set Scalar Parameter Value`(Collection=PPMaterialParameters, Name="MRMask", Value=bool→float). MPC 스칼라로 마스크 on/off.

### PP_MR 머티리얼 (실제 로직 — 문서가 "이미지 보고 재현"이라 넘겼던 것)
`SceneTexture:CustomDepth` Mask(R) vs `SceneTexture:SceneDepth` Mask(R) → **Subtract → Ceil → Min(1.0) → If** 비교 → MPC `MRMask`(1−x)로 게이트 → **Premultiplied alpha**(Multiply) → **Emissive Color + Opacity** 출력. Material Domain=Post Process, Blend=Opaque. **로직 = `CustomDepth < SceneDepth`면 마스크(구멍)**. §4-1과 일치.

### BP_MarkerControls — ⚠️ **CXMR 캘리브레이션 설계와 정확히 일치**
변수: **MarkerList**(int→BP_Marker map), **MarkerTimeout**(float, 기본 **3**), DynamicEvenMarkersEnabled(bool), **TargetVehicle**(Actor), **MarkerOffsets**(int→Transform map), **bCalibrated**(bool). 컴포넌트: `VarjoMarkersEvent`.
- **`New Varjo Marker Detected`** = **단일 진입점**(Sequence Then0~3):
  1. `Set Even Marker Dynamic`(id)
  2. `Set Marker Timeout`(id, MarkerTimeout) ← **Detected 안에서 호출**. 소스 분석(감지 후에만 먹음)과 정확히 일치.
  3. 마커 viz: `FIND` MarkerList[id] → 없으면 Make Transform(Scale=Size/100, Break Vector2D) → `SpawnActor BP_Marker` → ADD → `Update Marker Texts`. (주석: *"default static mesh cube size is 100uu so we divide by 100"*)
  4. **캘리브레이션 (필립님 임시 코드, 단일 마커)**: bCalibrated 게이트 → `FIND MarkerOffsets[id]` → **Invert Transform** → 마커 world transform과 곱(Compose) → **`Set Actor Transform`(TargetVehicle)** → SET Calibrated(true) → **`Set Varjo Marker Tracking Enabled`(Enabled=FALSE)**. ✅ **freeze 확정** — 정렬 직후 마커 추적을 끔 = one-shot freeze. 메모리의 캘리브레이션 결정과 일치.
    - `MarkerOffsets` = **int(마커ID) → Transform(차량 로컬 공간에서 마커의 위치)** 맵. 기본값 예: ID **202 → identity**. 수식: `VehicleWorld = Inverse(MarkerOffset) ⊗ MarkerWorld`. offset=identity면 차량 원점 = 마커 위치.
    - ⚠️ **필립님이 "임시/불안정" 명시** — 단일 마커라 각도 노이즈가 차량 전체로 증폭됨(§14-9). 멀티마커로 업그레이드 검토 중. Recalibrate 함수도 별도 존재(스샷 외).
- **`Varjo Marker Moved`**: FIND → `Set Hidden(false)` + `Set World Location And Rotation`(Position, Rotation). marker viz만 갱신.
- **`Varjo Marker Lost`**: FIND → `Set Hidden(true)`. **파괴 아닌 숨김**. 소스+문서와 일치.
- **SetEvenMarkerDynamic**(데모 기능): `id % 2` 판정 → DynamicEvenMarkersEnabled AND 짝수면 `Set Marker Tracking Mode` Dynamic. → **CXMR엔 불필요**하지만, "ID 기반으로 모드 분기"라는 패턴 자체는 마커 ID 체계(§14-1)에 참고.

→ **결론**: 기존 CXMR 설계(BP_MarkerControls / MarkerOffsets / bCalibrated / TargetVehicle)는 필립님이 예제 위에 얹은 것. 캘리브레이션 로직(단일 마커 one-shot freeze)은 확정 파악됨. **멀티마커 업그레이드가 다음 설계 과제**(§14-9).

### VRPawn (기본 pawn)
- 컴포넌트: Camera>HMD, MotionControllerLeft>HandLeft>XRDeviceVisualizationLeft, MotionControllerLeftAim>WidgetInteractionLeft, MotionControllerRight.
- 함수: StartTeleportTrace / TeleportTrace / IsValidTeleportLocation / EndTeleportTrace / TryTeleport / **SnapTurn** / **ToggleMenu** / GetGrabComponentNearMotionController. → **텔레포트 로코모션 + 스냅턴 + 위젯 인터랙션**이 기본 pawn에 내장.
- BeginPlay: `Is HMD Enabled` → `Set Tracking Origin(**Stage**)` → `Execute Console Command vr.PixelDensity 1.0` → **IMC 3개 등록**: `IMC_Default`(우선순위 0) → `IMC_Hands`(0) → `IMC_Varjo`(0).
  - ⚠️ BP_HandInteractionPawn은 origin=**Local Floor**, VRPawn은 **Stage** — 스왑 시 origin이 바뀔 수 있음. 주의.
  - ⚠️ **입력 우선순위 gotcha** (예제 주석): *"Setting ActionSet (OpenXR term) priority at Runtime is currently work-in-progress... setting priority in the Player Mappable Input config works as intended."* → **`Add Mapping Context`의 Priority 인자는 런타임에 안 먹을 수 있음.** 우선순위가 필요하면 Player Mappable Input config로 설정할 것.
- **함의**: 기능 토글(IMC_Varjo)은 VRPawn이 등록. Pawn을 BP_HandInteractionPawn으로 스왑해도, BP_*Controls 액터들은 **각자 Enable Input**으로 IA_Varjo를 받으므로 pawn과 무관하게 동작. → 이게 예제가 기능을 **레벨의 개별 액터**로 둔 이유. (CXMR A안 = 매니저 1개 + 컴포넌트. 접근은 다르나 "pawn 밖에서 입력 처리"라는 전제는 동일하게 성립.)

### BP_Marker (viz 액터)
컴포넌트: Cube + IDText + TrackingModeText + X/Y/Z axis. 변수 ID(int). `UpdateMarkerTexts`: ID→text, `Get Marker Tracking Mode` → Switch(Stationary/Dynamic) → TrackingModeText. 디버그 viz용.

### Pawn 스왑 아키텍처 (hand tracking과 결합)
- **VRMultiPawnGameMode**: `SpawnAndPossess`(현재 Pawn transform 저장 → Destroy → 새 Pawn Spawn → Possess). `SetHandPawn`(→BP_HandInteractionPawn) / `SetDefaultPawn`(→VRPawn).
- **BP_HandControls**: `IA_Varjo_HandVisualizationToggle` → SET IsEnabled(NOT) on BP_TrackedHands → Branch → `EnableHandInteractionPawn`(Get Game Mode→Cast VRMultiPawnGameMode→`Set Hand Pawn`) / `DisableHandInteractionPawn`(→`Set Default Pawn`).
- ⚠️ **즉 "손 시각화 토글"이 곧 Pawn 통째 교체**다. VRPawn ↔ BP_HandInteractionPawn. → CXMR §8의 "provider만 교체" 설계와 **접근이 다름**. Varjo 공식 방식은 **Pawn 스왑**. built-in↔Ultraleap 전환도 이 층에서 결정.

### BP_HandInteractionPawn (컨트롤러/손 인터랙션)
- 컴포넌트: Camera + 좌우 각각 MotionController **Aim/Grip/Palm/Pinch/Poke** + 하위 포인트(HitPoint/Grip/Palm/Pinch/Poke).
- BeginPlay: `Set Tracking Origin(Local Floor)` → Add Mapping Context **IMC_HandInteraction(Priority 5)** → `Get All Actors with Tag "HandInteraction"` → 각 오브젝트 collision/physics 활성.
- **Aim(레이저)**: Tick → `Line Trace By Channel`(Visibility, forward×1000) → HitPoint 갱신 + Hit Actor, `Actor Has Tag "Cube"` 체크.
- **Grip/Pinch(잡기)**: `IA_Keys_Grasp/Pinch_Interaction` → Delay 0.2 → value≥0.5 → `Sphere Trace By Channel`(radius 5, from Grip/Pinch loc) → `Actor Has Tag "Cube"` → **IGrabbable `Grab`(Attach To)** / `Release`.
- Ready/value 입력(`IA_Keys_AimReady/GripReady/PinchReady`, `_Aim/Grasp/Pinch_Interaction`)은 예제에선 대부분 **Print String(Development Only)** 디버그.
- Escape → Quit Game.

### IGrabbable 인터페이스 + 잡기 대상 2종
- **IGrabbable**(BP Interface): `Grab` / `Release` / `Attach Success`.
- **BP_FloatingObject**(kinematic): Grab→`Attach Component To Component`(Keep World) / Release→`Detach`(Keep World). 물리 없음.
- **BP_PhysicsObject**(물리): Grab→`Set Simulate Physics(false)`→Attach / Release→Detach→`Set Simulate Physics(true)`.
- → CXMR에서 **물리 스티어링 휠·핸들 잡기**를 구현한다면 이 인터페이스 패턴이 출발점. 단 §기술결정: 가상 휠 잡기는 구현 안 함(물리 휠 + masking)이므로 우선순위 낮음.

### 입력 자산 구조 (IMC 2개)
- **IMC_Varjo**: 기능 토글 14종(MR/Background/Mask/ViewOffset/DepthTest/Range/Gaze/Hand/Marker/Dynamic/Foveated viz 등).
- **IMC_HandInteractionKeys**(별도 Data Asset) + **IA_Keys/** 폴더: aim/grip/pinch **ready + value**(좌우). BP_HandInteractionPawn이 IMC_HandInteraction을 Priority 5로 추가.
- 인터랙션 포인트 viz용 Material Instance 5종: MI_Aim(빨강)/Grip(초록)/Palm(주황)/Pinch(파랑)/Poke(보라).

### 기타 컨트롤 BP
- **BP_EyeTrackingControls**: `IA_Varjo_GazeVisualizationToggle` → **`Is Eye Tracker Connected` AND** NOT(현재) → SET on 별도 viz 액터. gaze 시각화를 트래커 연결 여부로 게이트.
- **BP_FoveatedRenderingControls**: `IA_Varjo_FoveatedRenderingVisualizationToggle` → `Is Foveated Rendering Enabled` AND NOT → SET on `PP_FoveatedRenderingVisualization`(포스트프로세스, IsFocusView를 노랑으로 Lerp — foveal 영역 시각화).
- **BP_DepthControls**: DepthTest/EnvDepthEstimation/DepthTestRange 각각 read→NOT→set. 변수 AdjustmentSpeed(0.01).

### Substrate 재확인
PP_MR / PP_FoveatedRenderingVisualization 열 때 *"Substrate (Beta) is not enabled for this project"* 경고 → **예제는 Substrate 미사용**. 현 GMTCK_MR은 `r.Substrate=True`(§17) → **불일치 재확인**. Substrate 유지 시 이 포스트프로세스 머티리얼들이 그대로 동작하는지 검증 필요.

---

## 19. 참고 링크

- 플러그인 문서 루트: https://developer.varjo.com/docs/unreal/ue5/unreal5
- Recommended Settings (렌더링 트랙 근거): https://developer.varjo.com/docs/unreal/ue5/unreal5-recommended-settings
- Masking (전체 절차): https://developer.varjo.com/docs/unreal/ue5/masking-with-unreal5
- Markers (UE): https://developer.varjo.com/docs/unreal/ue5/markers-with-unreal5
- Markers (물리 스펙): https://developer.varjo.com/docs/get-started/varjo-markers
- Anchors and masking: https://developer.varjo.com/docs/get-started/anchors-and-masking
- 환경 요구사항: https://developer.varjo.com/docs/get-started/environment-setup-for-mixed-reality
- Camera render position: https://developer.varjo.com/docs/get-started/camera-render-position
- Supported features 매트릭스(주의): https://developer.varjo.com/docs/get-started/supported-features
- Blend control mask (Native): https://developer.varjo.com/docs/native/blend-control-mask
- Developer FAQ: https://support.varjo.com/hc/en-us/developer-faq#unreal-sdk
- 예제 저장소: VarjoOpenXRUnrealExamples (GitHub)
