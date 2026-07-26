# CXMR 헤드셋 실측 체크리스트 (Varjo XR-4)

첫 실기 검증용. **지금까지 MR 기능 중 실기에서 확인된 것은 하나도 없다** — 전부 PIE(헤드셋 없음)까지만
검증돼 있다. 이 문서는 무엇을 어떤 순서로 확인할지와, "안 되는 것처럼 보이지만 정상"인 경우를 적어둔다.

테스트 맵: `/Game/Map/L_Main`. 실행하면 `BP_CXMRGameMode`가 `BP_CXMRPawn`을 스폰한다.

---

## 0. 시작 전 30초

| 확인 | 기대 |
|---|---|
| `VehicleRoot_TestCar`가 앞쪽 2.5m에 보임 | 흰 큐브 2개(바디 + 캐빈)로 된 더미 차량 |
| 왼손을 들면 컨트롤 패널이 보임 | 안 보이면 §5 |
| 콘솔 `CXMR.DebugMarkers 1` | 마커 계측기 켜기 — §3에서 씀 |

---

## 1. MR 패스스루 — 가장 먼저

**`M` 키** (또는 패널의 Mixed Reality 버튼).

- ✅ 기대: 실제 방이 보이기 시작
- ⚠️ **가상 하늘·바닥이 남아 실제 세계를 가리면** `B`(VR Background)를 눌러 끈다.
  `SkyAtmosphere` / `ExponentialHeightFog` / `VolumetricCloud` / `SM_SkySphere` / `Floor`에
  `CXMRSceneObjectComponent(VROnly)`가 붙어 있어 한 번에 숨는다.
- ❌ **패널의 Mixed Reality가 OFF에서 안 움직이면** = CVar를 못 찾은 것. 로그에
  `LogCXMR: Warning: Mixed reality toggle ignored` 가 찍힌다. (상태를 거짓으로 바꾸지 않도록 만든 동작)

## 2. 뷰 오프셋 / Depth

| 키 | 기능 | 확인 |
|---|---|---|
| `K` | View Offset | 패널 표시가 `CAMERA` ↔ `EYE`로 바뀐다. 근거리 물체를 볼 때 정렬감 차이 |
| `T` | Depth Test | 손을 눈앞에 대면 가상 물체보다 앞에 보이는지 |
| `U` | Env Depth | depth estimation 활성 |

⚠️ **depth test range 밖은 실세계가 통째로 사라진다**(컴포지터 설계). 기본 0~0.75m = 손 범위.
방 전체를 보려던 게 아니라면 정상.

## 3. 마커 — 캘리브레이션의 토대

**`CXMR.DebugMarkers 1`을 켜고 실물 마커를 시야에 넣는다.**

1. **감지 자체**: 마커 pose가 그려지는가? 로그에 `LogCXMRDebug`가 찍힌다.
2. **정렬**: 프로파일(`DA_MarkerProfile_TestCar`)에 **ID 0 하나만, 오프셋 0**으로 등록돼 있다.
   → **ID 0 마커를 쓰면 더미 차량이 마커 위치에 정확히 얹혀야 한다.**
3. **드리프트**: 몇 분 두고 차량이 밀리는지 관찰. 반사면이 주 악화 요인.
4. **재정렬**: `R` 키. 마커 추적을 껐다 켜서 Detected를 다시 유도한다.

⚠️ **다른 ID의 마커는 프로파일에 없어서 무시된다**(설계상 정상). 멀티마커를 쓰려면
프로파일에 엔트리를 추가하고 **각 마커의 차량 기준 오프셋을 실측해 넣어야** 한다.
지금은 `MinMarkersToCalibrate = 1`로 두어 단일 마커로 완료되게 해뒀다.

## 4. Masking — 실물을 화면에 통과시키기

**`N` 키**. 테스트용 `MaskCube_Test`가 앞쪽 1.1m 눈높이에 있다(평소엔 **보이지 않는 게 정상** —
CustomDepth에만 그려진다).

- ✅ 기대: 그 큐브 모양대로 패스스루가 뚫려 실제 세계가 보인다
- ❌ 안 뚫리면 확인 순서: PPV의 Infinite Extent → PP_MR 머티리얼 → `PP_MRParameters`의
  `MRMask` 스칼라가 1로 가는지

실물 스티어링 휠 테스트는 이 큐브를 휠 위치·크기로 옮기면 된다.

## 5. 컨트롤 패널 — 위치 조정이 필요할 것

패널은 **왼손 컨트롤러**에 붙어 있고, **오른손 트리거**로 클릭한다.

- 클릭 키: `VarjoController_Right_Trigger_Axis` (아날로그 트리거, 액추에이션 0.5)
- ⚠️ **`PanelOffset(8,0,4)` / `PanelRotation(pitch -25, yaw 180)` / `PanelScale 0.03`은 추정값이다.**
  손에 대해 어디에 걸리는지 보고 `BP_CXMRPawn`에서 조정 — **리빌드 불필요**.
- 안 보이면: 너무 가까워 근접 클리핑에 잘렸을 수 있다(`PanelOffset` X를 늘린다).

## 6. Viewer — 차량/트림/CMF

| 조작 | 기대 |
|---|---|
| 오른쪽 스틱 좌우 | 트림 순환 (Base ↔ Sport — 위 큐브 모양이 바뀜) |
| 오른쪽 스틱 상하 | 차량 순환 (Test Car A ↔ B) |
| 패널 `Next CMF` | 재질 변경 (Plain ↔ Grid) |
| 왼쪽 스틱 좌우 / `A`·`B` | 턴테이블 회전 |

⚠️ **턴테이블은 `Placement.Mode = PawnRelative`일 때만 동작한다.** 지금은 마커 테스트를 위해
`MarkerAnchor`로 두었으므로 **회전이 막혀 있는 게 정상.** 회전을 보려면 에디터에서
`VehicleRoot_TestCar → Placement → Mode`를 `PawnRelative`로 바꾼다.

`E` 키(차량을 내 앞에 배치)는 모드와 무관하게 동작한다.

## 7. Human Factors — eye 스냅

`DA_Ergonomics_TestCar`에 5th / 50th / 95th 세 위치가 눈높이만 다르게 들어 있다(105 / 120 / 135cm).

- **VR 모드**: 내 시점이 이동한다
- **MR 모드**: 차량이 이동한다 (실제 몸은 못 옮기므로)
- ⚠️ **MR + MarkerAnchor면 아무것도 안 움직이는 게 정상** — eye point가 이미 물리적으로 실재하므로
  차를 옮기면 캘리브레이션과 싸운다. 로그에 그 이유가 찍힌다.

---

## 아무 반응이 없는 키들 (배선 안 됨)

IMC에는 매핑돼 있지만 **C++ 바인딩이 없어 눌러도 아무 일도 없다.** Varjo 예제에서 애셋만
넘어온 것들이다. 내일 이걸로 오진하지 말 것:

`H`(손 시각화) · `G`(gaze) · `C`(dynamic tracking) · `Y`·방향키(depth range) · `I`(foveation 시각화)

## 손 추적

**CXMR에는 손 관련 코드가 없다.** 플러그인이 제공하는 것:

| 층 | 출처 | 제공 |
|---|---|---|
| 관절 스켈레톤 | 엔진 `OpenXRHandTracking` | `Get Hand Tracking State` → Key Locations/Rotations/Radii |
| 상호작용 포즈 | `VarjoHandInteraction` | `GetHandInteractionAimPose` / `GetHandInteractionGripPose` |

pinch/poke/palm은 C++ motion source로만 있고 BP 함수는 aim/grip뿐.
⚠️ Varjo 문서가 시키는 `Get Motion Controller Data`는 UE 5.7에서 deprecated — 예제도 실제로는
`Get Hand Tracking State`를 쓴다.
