# CXMR 헤드셋 실측 체크리스트 (Varjo XR-4)

첫 실기 검증용. **지금까지 MR 기능 중 실기에서 확인된 것은 하나도 없다** — 전부 PIE(헤드셋 없음)까지만
검증돼 있다. 이 문서는 무엇을 어떤 순서로 확인할지와, "안 되는 것처럼 보이지만 정상"인 경우를 적어둔다.

테스트 맵: `/Game/Map/L_Main`. 실행하면 `BP_CXMRGameMode`가 `BP_CXMRPawn`을 스폰한다.

---

## 0. 시작 전 30초

| 확인 | 기대 |
|---|---|
| `VehicleRoot_TestCar`가 앞쪽 2.5m에 보임 | 흰 큐브 2개(바디 + 캐빈)로 된 더미 차량 |
| 모니터에 컨트롤 창이 따로 뜸 | 손의 3D 패널은 기본으로 꺼져 있다. 필요하면 §5 |
| 콘솔 `CXMR.DebugMarkers 1` | 마커 계측기 켜기 — §3에서 씀 |

---

## 1. MR 패스스루 — 가장 먼저

**`M` 키** (또는 컨트롤 창 Display 탭의 Mixed reality 체크박스).

- ✅ 기대: 실제 방이 보이기 시작
- ⚠️ **가상 하늘·바닥이 남아 실제 세계를 가리면** `B`(VR Background)를 눌러 끈다.
  `SkyAtmosphere` / `ExponentialHeightFog` / `VolumetricCloud` / `SM_SkySphere` / `Floor`에
  `CXMRSceneObjectComponent(VROnly)`가 붙어 있어 한 번에 화면에서 빠진다. 조명·반사에는 계속 남으므로 MR에서도
  가상 물체 밝기가 VR과 같아야 한다(2026-09-14 수정 — 전에는 액터를 숨겨 스카이라이트가 꺼지면서 어두워졌다).
- ❌ **컨트롤 창의 Mixed reality 체크가 안 켜지면** = CVar를 못 찾은 것. 로그에
  `LogCXMR: Warning: Mixed reality toggle ignored` 가 찍힌다. (상태를 거짓으로 바꾸지 않도록 만든 동작)

### 🔴 `B`를 껐는데 검정 화면이면 — 알파 문제 (미해결)

**2026-07-27 실측**: MR을 켜고 `B`로 VR 배경을 끄면 패스스루가 아니라 **검정 화면**이 됐다.
`T`(Depth Test)를 켰을 때만 패스스루가 나타났다.

패스스루 조건은 *"Pixels with RGBA(0,0,0,0) display only the VST image"* — **알파가 0인 픽셀에만**
실세계가 보인다. 배경 액터를 숨겨도 알파가 1이면 그냥 검정이다. `T`에서만 보이는 이유는 depth
test가 **컴포지터 레이어**에서 처리되어 알파 합성 경로를 우회하기 때문이다.

**유력 용의자는 `PP_MR`이다.** Capabilities §4-1 step 7에 기록된 로직이
*"`CustomDepth < SceneDepth` → Opacity 0, **아니면 1.0**"* 이므로, 마스크 구멍 바깥 전 영역에
**불투명 알파를 칠하게 된다.** 이것이 사실이면 `B` 검정 · `N` 안 보임 · `U` 패스스루 없음이
한 번에 설명된다.

**→ 전용 문서로 분리했다: `Docs/Passthrough-Black-Screen.md`**

리빌드도 에디터도 없이 콘솔만으로 원인을 가리는 절차가 들어 있다. 용의자는 넷이고
(`PP_MR` / TSR 알파 / scene color format / 알파 전파), 각각 콘솔 한 줄로 갈린다.

리빌드했다면 먼저 `CXMR.DumpMRState` 한 줄로 전부 찍어본다 — 콘솔로는 못 보는
"플러그인이 판단하는 MR 상태 vs CXMR 캐시" 대조가 들어 있고, 둘이 어긋나면 알파 이전에 그게 원인이다.

## 2. 뷰 오프셋 / Depth

| 키 | 기능 | 확인 |
|---|---|---|
| `K` | View Offset | 컨트롤 창 `Render from` 버튼이 `Cameras` ↔ `Eyes`로 바뀐다(튜닝 창 View offset 1 ↔ 0). 근거리 물체를 볼 때 정렬감 차이. 시작은 VR `Eyes` → MR 켜면 `Cameras`(헤드셋 기본). `K`로 한 번 고르면 MR 전환 뒤에도 유지되는지 본다 |
| `T` | Depth Test | 손을 눈앞에 대면 가상 물체보다 앞에 보이는지 |
| `U` | Env Depth | depth estimation 활성. **`T`가 먼저 켜져 있어야 한다** |
| `Y` | Depth Test **Range** on/off | 컨트롤 창 Range가 `0.00 - 0.75 m` ↔ `unbounded` |
| `←`/`→` | NearZ 감소/증가 | 누르고 있으면 연속 변화 |
| `↓`/`↑` | FarZ 감소/증가 | 〃 |

### ⚠️ flickering이 보이면 range를 먼저 의심한다

**이전 판의 "기본 0~0.75m = 손 범위"는 틀린 서술이었다.** 0.75는 플러그인 구조체에 저장만 돼
있고, range가 **비활성이면 컴포지터에는 `farZ = HUGE_VALF`가 전달된다**(`DepthPlugin.cpp:58-59`).
그래서 실제로는 **방 전체가 무한 거리까지** depth test 대상이었고, 원거리·반사면의 불안정한 추정
depth 때문에 가상 물체가 심하게 깜빡였다.

지금은 CXMR이 range를 **기본으로 켜고 0.0–0.75m로 시작**한다. 확인할 것:
1. `T`를 켠 상태에서 깜빡임이 줄었는가
2. `Y`로 range를 **끄면** 깜빡임이 다시 나타나는가 (원인 확정)
3. `↑`로 FarZ를 넓혀가며 **어느 거리부터** 불안정해지는지 — 이 값이 실차 세팅의 기준이 된다

⚠️ **range 밖은 실세계가 통째로 사라진다**(컴포지터 설계). 좁히는 게 안전한 방향이 아니다 —
좁힐수록 실세계가 보이는 영역이 줄어든다. 방 전체를 보려던 게 아니라면 정상.

### 튜닝 창으로 값 정하기 (2026-09-14 추가)

모니터 왼쪽 위 **`CXMR Tuning` 창**에서 숫자를 직접 넣을 수 있다(`Operating-Guide.md` §7). 위 3번의 FarZ는
화살표 키로 조금씩 옮기는 것보다 여기서 넣는 게 빠르다.

1. **Depth → Range far**를 바꾸면 손이 가상 물체를 가리기 시작하는 거리가 바뀌는가
2. 앱을 껐다 켜도 그 값이 그대로인가 (`Saved/CXMR/Tuning.json`에 PC별로 저장)
3. **MR에서 차가 여전히 어두우면** **Display → Exposure compensation**을 올려 VR과 비슷해지는 값을 적는다
4. **근거리 가상 물체(가상 손 포함)가 실물보다 위·아래로 어긋나면** **Mixed reality → View offset**을
   0(눈)과 1(카메라) 사이로 옮겨 가며 어느 값에서 겹치는지 적는다
5. **Vehicle placement**의 `[-][+]`가 넘버패드와 같은 방향으로 움직이는가 — 멀어짐·오른쪽·위·시계 방향이 `+`

## 3. 마커 — 캘리브레이션의 토대

### 🔴 0단계 — `V`를 먼저 누른다

**마커 추적은 세션 시작 시 꺼져 있다.** `V`를 누르기 전에는 마커 이벤트가 단 한 건도 오지 않는다.
이 단계가 이 문서에 빠져 있어서 "감지 자체가 안 된다"로 오진하기 쉬웠다. 컨트롤 창의 Marker tracking이 체크
되는지 본다. 안 켜지면 로그에 `LogCXMR: Warning: Marker tracking could not be enabled`이 찍힌다.

**그 다음 `CXMR.DebugMarkers 1`을 켜고 실물 마커를 시야에 넣는다.**

1. **감지 자체**: 마커 pose가 그려지는가? 로그에 `LogCXMRDebug: DETECTED id=N`이 찍힌다.
2. 🔴 **그 `N`을 읽는다.** 프로파일(`DA_MarkerProfile_TestCar`)은 **ID 0**으로 authoring돼 있는데,
   **0은 Varjo 플러그인이 "무효 ID" 센티넬로 쓰는 값**이다(`VarjoMarkersPlugin.cpp:150, :162` —
   timeout·tracking mode 설정을 거부한다). 실물 마커는 0을 보고하지 않으므로 **프로파일이 그대로면
   차량은 영원히 안 움직인다.** 로그에 `LogCXMRPlacement: Warning: ... contains id 0`이 찍힌다.
   → 에디터에서 `DA_MarkerProfile_TestCar`의 `Marker Id`를 **실측한 `N`으로 바꾼다.**
3. **정렬**: 오프셋이 0이므로 **차량 원점이 마커 위에 정확히 얹혀야 한다.**
4. **드리프트**: 몇 분 두고 차량이 밀리는지 관찰. 반사면이 주 악화 요인.
5. **재정렬**: `R` 키. 마커 추적을 껐다 켜서 Detected를 다시 유도한다.

### 마커 추종을 보려면 freeze를 꺼야 한다

기본 설정(`Freeze After Calibration = ON`, `MinMarkersToCalibrate = 1`)에서는 **첫 감지 한 번으로
캘리브가 끝나고 얼어붙는다.** 마커를 움직여도 차는 따라오지 않는다 — 이게 실내 설계 의도다.

추종·재수렴을 확인하려면 `VehicleRoot_TestCar → Placement → Freeze After Calibration`을 **끈다.**
그러면 차량이 마커를 따라 움직인다(`Marker Update Threshold` cm 이상 움직였을 때만 갱신 —
지터로 차가 떨지 않게 하는 값, 기본 0.5cm).

⚠️ 이전에는 freeze를 꺼도 추종이 안 됐다. `Detected`(ID당 세션당 1회)만 구독하고 있어서
**최초 1프레임의 pose에 영구 고정**됐고, 마커를 놓쳤다 다시 잡아도(그때는 `Moved`로 온다)
재정렬되지 않았다. 지금은 `Moved`도 반영한다.

⚠️ **다른 ID의 마커는 프로파일에 없어서 무시된다**(설계상 정상). 멀티마커를 쓰려면
프로파일에 엔트리를 추가하고 **각 마커의 차량 기준 오프셋을 실측해 넣어야** 한다.
지금은 `MinMarkersToCalibrate = 1`로 두어 단일 마커로 완료되게 해뒀다.
(`Moved`를 듣기 전에는 2번째 마커가 `DetectedCalib`에 들어갈 수 없어 **멀티마커가 구조적으로
완성될 수 없었다.** 이제 2 이상으로 올려도 동작한다.)

## 4. Masking — 실물을 화면에 통과시키기

**`N` 키**. 테스트용 `MaskCube_Test`가 앞쪽 1.1m 눈높이에 있다(평소엔 **보이지 않는 게 정상** —
CustomDepth에만 그려진다).

- ✅ 기대: 그 큐브 모양대로 패스스루가 뚫려 실제 세계가 보인다
- ❌ 안 뚫리면 확인 순서: PPV의 Infinite Extent → PP_MR 머티리얼 → `PP_MRParameters`의
  `MRMask` 스칼라가 1로 가는지
- ⚠️ **§1의 알파 문제가 먼저 해결돼야 판정할 수 있다.** 패스스루 자체가 안 뜨는 상태에서는
  마스킹이 되는지 안 되는지 구분이 불가능하다 — 2026-07-27 세션의 "아예 안 보임"이 그 상황이었다.
- `MaskParameters`가 비어 있으면 이제 로그에 `LogCXMRMask: Warning: Masking cannot be applied`가
  찍힌다(이전에는 무성으로 죽었다). 기본값은 C++에서 `PP_MRParameters`로 붙는다.

실물 스티어링 휠 테스트는 이 큐브를 휠 위치·크기로 옮기면 된다.

## 5. 컨트롤 패널 — 위치 조정이 필요할 것

**기본으로 꺼져 있다.** `BP_CXMRPawn → Show Hand Panel`을 켜야 나타난다. 켜면 패널은 **왼손 컨트롤러**에
붙고, **오른손 트리거**로 클릭한다.

- 클릭 키: `VarjoController_Right_Trigger_Axis` (아날로그 트리거, 액추에이션 0.5)
- ⚠️ **`PanelOffset(8,0,4)` / `PanelRotation(pitch -25, yaw 180)` / `PanelScale 0.03`은 추정값이다.**
  손에 대해 어디에 걸리는지 보고 `BP_CXMRPawn`에서 조정 — **리빌드 불필요**.
- 안 보이면: 너무 가까워 근접 클리핑에 잘렸을 수 있다(`PanelOffset` X를 늘린다).
- 모양은 데스크톱 컨트롤 창과 같다(2026-09-15부터 C++ 패널, 튜닝 창과 같은 디자인). 체크박스·버튼을 레이저로 누른다.
  내용이 길면 스크롤된다 — 헤드셋에서 스크롤이 불편하면 `Panel Draw Size` Y를 키우고 `Panel Scale`을 줄인다

## 6. Viewer — 차량/트림/CMF

| 조작 | 기대 |
|---|---|
| 오른쪽 스틱 좌우 | 트림 순환 (Base ↔ Sport — 위 큐브 모양이 바뀜) |
| 오른쪽 스틱 상하 | 차량 순환 (Test Car A ↔ B) |
| 컨트롤 창 Viewer 탭 `Next CMF` | 재질 변경 (Plain ↔ Grid) |
| 왼쪽 스틱 좌우 / `A`·`B` | 턴테이블 회전 |

⚠️ **턴테이블은 `Placement.Mode = PawnRelative`일 때만 동작한다.** 지금은 마커 테스트를 위해
`MarkerAnchor`로 두었으므로 **회전이 막혀 있는 게 정상.** 회전을 보려면 에디터에서
`VehicleRoot_TestCar → Placement → Mode`를 `PawnRelative`로 바꾼다.

`E` 키(차량을 내 앞에 배치)는 모드와 무관하게 동작한다.

## 7. Human Factors — eye 스냅

**조작: 컨트롤 창 Viewer 탭 → Human factors → Manikin `[-][+]`.** (스틱으로 하려면 `VarjoInput → Cycle Manikin Action`에
Axis1D IA를 지정한다 — 애셋이 아직 없다.)

⚠️ **이 절은 지금까지 검증이 불가능했다.** 컴포넌트는 완전히 구현돼 구독까지 하고 있었지만
`RequestErgonomicsStep`을 부르는 곳이 **하나도 없어서** 발동 자체가 안 됐다. 이 문서가
"MR + MarkerAnchor면 아무것도 안 움직이는 게 정상"이라고 적어둔 탓에, 안 움직이는 것이
정상인지 미배선인지 구분할 수도 없었다. 이제 컨트롤 창의 Manikin 버튼이 기본 경로다.

`DA_Ergonomics_TestCar`에 5th / 50th / 95th 세 위치가 눈높이만 다르게 들어 있다(105 / 120 / 135cm).

- **VR 모드**: 내 시점이 이동한다
- **MR 모드**: 차량이 이동한다 (실제 몸은 못 옮기므로)
- ⚠️ **MR + MarkerAnchor면 아무것도 안 움직이는 게 정상** — eye point가 이미 물리적으로 실재하므로
  차를 옮기면 캘리브레이션과 싸운다. 로그에 그 이유가 찍힌다.

---

## 8. 손 추적 — `H` 키

`H`를 누르면 추적된 손 관절이 그려진다(왼손 파랑 / 오른손 주황, 손가락 뼈대 연결).

- ✅ 기대: 손을 시야에 넣으면 관절 26개가 손을 따라 움직인다
- 로그가 답을 말해준다:
  - `LogCXMRHands: Hand tracker present: yes/NO` — 플러그인 자체가 있는지
  - `LogCXMRHands: Warning: LEFT/RIGHT hand acquired/lost` — 실제 추적 획득·상실 시점
- ⚠️ **손이 얼어붙은 채 남아 보이면 그건 버그가 아니라 방지된 상황이다** — 트래커는 추적이 끊겨도
  마지막 포즈를 계속 반환한다(원점으로 튀는 걸 막으려고). CXMR은 `bIsTracked`를 확인해 그리지
  않으므로, 손이 사라지면 실제로 추적이 끊긴 것이다.
- 확인할 것: **추적 범위**(어느 각도·거리에서 끊기는지), **지터**, 컨트롤러를 든 손도 잡히는지.

### 손이 실제 손과 어긋나 보이면 (2026-09-14 실측: 스켈레톤이 손보다 위·앞에 떴다)

CXMR 쪽 변환은 확인했다 — 폰·카메라에 오프셋이 없고, 손 관절과 머리 위치가 같은 트래킹 원점에서 온다.
그러니 차이는 **트래킹 데이터**나 **렌더 위치**에서 온다. 튜닝 창 **Hands** 분류로 가른다.

1. **얼마나 틀렸나 재기** — `CXMR.DebugMarkers 1`로 마커가 잡힌 상태에서 오른손 검지 끝을 마커 **중앙에 댄다**.
   `Tip minus marker`가 오차다(머리 기준 fwd / right / up, cm). 손끝 두께 때문에 1cm 안팎은 정상
2. **어긋남이 일정한가** — 마커를 가까운 곳(약 40cm)과 먼 곳(약 70cm)에 두고 1을 반복한다
   - 두 곳의 오차가 비슷하다 → 고정 오프셋. 4로 바로 맞춘다
   - 멀수록 커진다 → 거리(깊이) 오차. 오프셋으로는 한 거리에서만 맞으니 두 수치를 적어 온다
3. **View offset에 따라 달라지나** — Mixed reality → View offset을 1(카메라)과 0(눈)으로 바꿔 1을 반복한다.
   오차가 크게 바뀌면 렌더 위치 문제다
4. **맞추기** — 검지 끝을 마커 중앙에 댄 채 **Snap hand to marker**를 누른다. 오차가 그대로 보정값(`Hand offset`
   3칸)이 되어 `H` 스켈레톤과 가상 손이 같이 옮겨지고 `Tuning.json`에 저장된다. 다시 1을 해서 1cm 안쪽인지 본다
5. Varjo Base → Settings의 hand tracking이 **Varjo**인지 **Ultraleap**인지 적어 둔다

⚠️ 손을 빠르게 움직일 때만 어긋나고 멈추면 맞는다면 오프셋이 아니라 **지연**이다 — 보정하지 말 것.

측정 결과에 따라 다음 단계(손 poke로 패널 누르기)의 실현 가능성이 정해진다.

## 아무 반응이 없는 키들 (배선 안 됨)

IMC에는 매핑돼 있지만 **C++ 바인딩이 없어 눌러도 아무 일도 없다.** Varjo 예제에서 애셋만
넘어온 것들이다. 이걸로 오진하지 말 것:

`G`(gaze) · `C`(dynamic tracking) · `I`(foveation 시각화)

~~`Y`·방향키(depth range)~~ → **이제 배선됐다.** §2 참조 — flickering의 원인이었다.

## 손 인터랙션 — 아직 없는 것

시각화만 붙였다. **손으로 무언가를 누르거나 잡는 기능은 없다.** 플러그인이 제공하는 것:

| 층 | 출처 | 제공 |
|---|---|---|
| 관절 스켈레톤 | 엔진 `OpenXRHandTracking` | `IHandTracker::GetAllKeypointStates` (CXMR이 쓰는 것) |
| 상호작용 포즈 | `VarjoHandInteraction` | `GetHandInteractionAimPose` / `GetHandInteractionGripPose` |

pinch/poke/palm은 C++ motion source로만 있고 BP 함수는 aim/grip뿐이다.
⚠️ Varjo 문서가 시키는 `Get Motion Controller Data`는 UE 5.7에서 deprecated — 예제도 실제로는
`Get Hand Tracking State`를 쓴다.
⚠️ `OpenXRHandTracking`은 엔진 기본 비활성 플러그인이라 `.uproject`에서 명시적으로 켰다.

---

## 9. 가상 손 vs 깊이 비교 (2026-09-13 추가)

**왜 하는가**: MR에서는 가상이 **깊이와 상관없이** 카메라 영상 위에 그려진다. 그래서 가상 콘솔을 쓰면
**실제 손이 가상 콘솔에 가려진다.** 손을 보이게 하는 두 방법을 같은 자리에서 비교한다.

**준비**: 앉아서 손을 뻗을 거리(40~70cm)에 **가상 표면**이 있어야 한다. 레벨에 Cube를 하나 두고,
헤드셋으로 그 큐브가 보이는 자리에 실물 박스를 맞춰 놓는다. `E`로 TestCar를 앞에 불러 차체 큐브를 써도 된다.

### 9-1. 손 추적이 버티는가 — `H`

§8과 같은 방법이다. **USB 케이블을 쥔 채로 박스 쪽으로 손을 뻗어** 관절이 계속 따라오는지 본다.
끊기는 자세를 적어 둔다. 9-2 가상 손의 품질은 이 추적 품질을 넘을 수 없다.

### 9-2. 가상 손 — `CXMR.VirtualHands 1`

- ✅ 기대: 추적된 손이 살색 캡슐 손으로 그려지고, **오른손 엄지 끝과 검지 끝 사이에 검은 USB-C 플러그**가
  들린다. 금속 끝은 검지 방향을 향한다
- 가상 큐브 **앞**에서는 손이 보이고, 큐브 **안으로** 넣은 부분은 가려져야 한다. 둘 다 가상이라 앞뒤가 정확하다
- 추적이 끊기면 손이 **사라지는 게 정상**이다. 마지막 자세로 얼어붙지 않게 만들었다
- ⚠️ `T`(depth test)는 **끈다.** 켜 두면 실제 손도 같이 보여 손이 두 개가 된다
- 조정은 `BP_CXMRPawn → VirtualHands`에서 한다. 리빌드는 필요 없다
- **가상 손이 실제 손보다 위·앞에 뜨면** §8 "손이 실제 손과 어긋나 보이면" 절차로 맞춘다. 보정은 `H` 스켈레톤과
  가상 손에 똑같이 들어가므로 스켈레톤으로 맞추면 가상 손도 같이 맞는다
  - 플러그 위치·각도: `Plug Offset` / 크기: `Plug Body Size` / 들고 있는 손: `Plug Hand`
  - 손가락 굵기: `Radius Scale` / 색: `Skin Color`(Play를 시작할 때 적용된다)
- 헤드셋 없이 모양만 볼 때: `CXMR.VirtualHands.Preview 1`. 카메라 앞에 고정 자세 손 두 개가 나온다

### 9-3. 실제 손 — 깊이 비교

`CXMR.VirtualHands 0` → `T` → `U`

- ✅ 기대: 가상 큐브 앞에 실제 손과 USB 케이블이 보인다
- 볼 것: 손 가장자리가 지저분한가 / **실물 박스 표면과 가상 큐브 표면이 겹치는 곳이 깜빡이는가** /
  가는 케이블이 보이는가
- 깜빡이면 실물 박스 윗면을 가상 표면보다 1~2cm 낮게 둔다

### 9-4. 포트 표시 — `CXMR USB Port Target`

가상 CAD에는 포트가 구멍으로만 있어서 USB가 꽂히는지 알 수 없다. 포트마다 표식을 두면
**가상 플러그가 그 포트에 곧게 맞춰질 때 포트 테두리가 초록으로** 켜진다.

- **배치**: `CXMR USB Port Target`를 레벨에 끌어다 포트 구멍 중심에 놓는다. **화살표가 포트 밖, 앉은
  사람 쪽**을 향해야 한다. 포트 개수만큼 둔다
  - 차량이 캘리브레이션으로 움직이면 표식도 따라가야 한다. 차량 BP 안에 Child Actor로 넣거나, 레벨에 둔
    차량 액터에 Attach한다
- **켜지는 조건**: 플러그 금속 끝이 포트 중심에서 `Enter Distance`(1.5cm) 안에 있고, 방향이 포트 축과
  `Max Angle`(25°) 이내. `Exit Distance`(3cm) 밖으로 나가야 꺼진다(경계에서 깜빡이지 않게)
- **가상 손을 꺼도 동작한다**(9-3 깊이 비교 중에도). 손 추적만 되면 판정한다
- 로그: `LogCXMRPort: Port '...' ALIGNED (plug lined up)`
- 테두리 크기 `Frame Size`(기본 1.4 × 0.9cm) / 평소엔 숨기고 맞췄을 때만 보이려면 `Show When Idle` 끄기
- ⚠️ **초록 = "그 자세로 포트에 곧게 맞췄다"**이다. 끝까지 꽂혔다는 뜻이 아니다 — CAD에는 소켓이 없다.
  손 추적이 mm 단위로 정확하지 않아 기준을 느슨하게 뒀다
- 9-2에서 맞춘 `Plug Offset`이 판정에도 그대로 쓰인다. 가상 플러그가 실제 플러그와 겹치게 먼저 맞춘다

### 9-5. 결과와 함께 적을 것

- 헤드셋 PC의 **Varjo Base 버전과 라이선스(Pro 여부)**. Pro라면 Base 설정의 Occlusion > Hands도 시험한다
- 모니터의 미러 화면에 **패스스루가 같이 보이는가**. 9-3 방식은 이게 돼야 주변 사람들에게 보여줄 수 있다.
  9-2는 UE 화면만으로 보여줄 수 있다
