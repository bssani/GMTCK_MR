# CXMR 헤드셋 실측 체크리스트 (Varjo XR-4)

첫 실기 검증용. **지금까지 MR 기능 중 실기에서 확인된 것은 하나도 없다** — 전부 PIE(헤드셋 없음)까지만
검증돼 있다. 이 문서는 무엇을 어떤 순서로 확인할지와, "안 되는 것처럼 보이지만 정상"인 경우를 적어둔다.

테스트 맵: `/Game/Map/L_Main`. 실행하면 `BP_CXMRGameMode`가 `BP_CXMRPawn`을 스폰한다.

---

## 0. 시작 전 30초

| 확인 | 기대 |
|---|---|
| `VehicleRoot_TestCar`가 앞쪽 2.5m에 보임 | 흰 큐브 2개(바디 + 캐빈)로 된 더미 차량 |
| 모니터에 컨트롤 창이 따로 뜸 | 진행자용 Review / Alignment / Settings 통합 창 하나(§5) |
| 콘솔 `CXMR.DebugMarkers 1` (또는 컨트롤 창 Settings → Marker axes and labels) | 마커 계측기 켜기 — §3에서 씀 |

---

## 1. MR 패스스루 — 가장 먼저

**`M` 키** (또는 상단 MR / VR 버튼).

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
| `K` | View Offset | 컨트롤 창 `Render from` 버튼이 `Cameras` ↔ `Eyes`로 바뀐다(튜닝 창 View offset 1 ↔ 0). 근거리 물체를 볼 때 정렬감 차이. 시작은 VR `Eyes` → MR 켜면 `Cameras`(헤드셋 기본). `K`로 한 번 고르면 MR 전환 뒤에도 유지되는지 본다. **0.5초에 걸쳐 부드럽게 옮겨 가는지**(툭 튀지 않는지) — 로그에 `View offset glide stopped`가 찍히면 런타임이 값을 받지 않은 것 |
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
4. **근거리 가상 물체가 실물보다 위·아래로 어긋나면** **Mixed reality → View offset**을
   0(눈)과 1(카메라) 사이로 옮겨 가며 어느 값에서 겹치는지 적는다
5. **Vehicle placement**의 `[-][+]`가 넘버패드와 같은 방향으로 움직이는가 — 멀어짐·오른쪽·위·시계 방향이 `+`

## 3. 마커 — 캘리브레이션의 토대

### 🔴 0단계 — 마커 추적이 켜졌는지 본다

**2026-09-15부터 MarkerAnchor 차량은 시작할 때 마커 추적을 자동으로 켠다**(XR 세션이 올라올 때까지 최대 20초 재시도).
로그에 `LogCXMRPlacement: Marker tracking started for calibration.`이 찍히고 컨트롤 창의 Marker tracking이 체크된다.
20초 안에 못 켜면 `Marker tracking could not be started within 20 s` 경고가 찍힌다 → 헤드셋이 올라온 뒤 `V`를 누른다.

**그 다음 `CXMR.DebugMarkers 1`을 켜고 실물 마커를 시야에 넣는다.**

1. **감지 자체**: 마커 pose가 그려지는가? 로그에 `LogCXMRDebug: DETECTED id=N`이 찍힌다.
   마커 위에 **`ID N  Stationary` 라벨이 헤드셋에서 읽히는가**(글자가 나를 향하는가). `mode ?`면 플러그인이 그 마커를 모른다
2. **학습**: 차를 넘버패드·튜닝 창으로 실물에 맞추고, 마커가 모두 보일 때 튜닝 창 **Learn marker layout**.
   로그에 `Learned marker layout ... N added`. 프로파일에 번호를 넣을 필요는 없다(무효 id 0은 무시된다)
3. **복원** ★ 가장 중요: 앱을 껐다 켠다 → 마커가 보이면 차가 **학습한 자리에** 놓이고, 1.5초 뒤 튜닝 창에 `calibrated`.
   필요 마커 하나를 가리면 차가 움직이지 않고 빠진 ID가 표시돼야 한다. 전부 보이면 정착 후 자동 복원돼야 한다. 조금 옮긴 뒤 저장을 한 번 요청하고 새 평균·자동 저장 완료 뒤 다시 실행한다
4. **드리프트**: 몇 분 두고 차량이 밀리는지 관찰. 반사면이 주 악화 요인.
5. **재정렬**: `R` 키. 마커 추적을 껐다 켜서 Detected를 다시 유도한다.

### 껐다 켤 때마다 차 위치가 조금씩 달라지면 (2026-09-16 수정)

저장과 복원 모두 마커 표본에 영향을 받으므로 **로그로 구분한다**. 저장 캡처는 보정용 평균과 분리한 새 표본을 쓰고, 복원은 모든 필요 마커의 정착 평균을 쓴다.

- [ ] 저장 로그의 마커별 표본 수·흩어짐·평균 월드 좌표·LocalOffset과 차량 Root 위치·yaw를 기록한다.
- [ ] 앱 재시작 세 번마다 `CXMR.DumpMarkers`와 `LogCXMRAlignment` 복원 로그를 남긴다.
- [ ] `Restored saved world pose`면 저장한 Root 위치와 같은지, `Restored from markers`면 최대 원점 이동 거리(cm)와 최종 위치·yaw·fit 오차를 비교한다.
- [ ] 기본 원점 재사용 허용값은 위치 0.3cm, 단일 마커 yaw 0.3deg다. 이 값의 현장 적합성은 아직 `not-tested`다.
- [ ] 필요 마커가 한 개인 프로필은 한 개로 보정·복원이 되는지 확인한다. 두 개 이상인 프로필은 하나만 보여도 배치되던 이전 경로가 없어야 한다.
- [ ] Marker Follower의 움직이는 마커 번호가 보정 필요 수·관측 수·Learn 결과에 들어가지 않는지 확인한다.

### 마커 추종을 보려면 freeze를 꺼야 한다

기본 설정(`Freeze After Calibration = ON`)에서는 **프로필의 필요 마커 전부와 정착 평균으로 완료한 뒤 고정된다.** 마커를 움직여도 차는 따라오지 않는다 — 이게 실내 설계 의도다.

추종·재수렴을 확인하려면 `VehicleRoot_TestCar → Placement → Freeze After Calibration`을 **끈다.**
그러면 차량이 마커를 따라 움직인다(`Marker Update Threshold` cm 이상 움직였을 때만 갱신 —
지터로 차가 떨지 않게 하는 값, 기본 0.5cm).

⚠️ 이전에는 freeze를 꺼도 추종이 안 됐다. `Detected`(ID당 세션당 1회)만 구독하고 있어서
**최초 1프레임의 pose에 영구 고정**됐고, 마커를 놓쳤다 다시 잡아도(그때는 `Moved`로 온다)
재정렬되지 않았다. 지금은 `Moved`도 반영한다.

⚠️ **다른 ID의 마커는 프로파일에 없어서 무시된다**(설계상 정상). 멀티마커를 쓰려면
프로파일에 엔트리를 추가하고 **각 마커의 차량 기준 오프셋을 실측해 넣어야** 한다.
필요 마커 수는 프로필의 유효한 Calibration ID 집합으로 정한다. Marker Follower ID는 제외한다.

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

## 5. CXMR Control / Advanced settings

이 절은 헤드셋 PC에서 확인한다. 실제 헤드셋 검증은 **not-tested**다.

- [ ] 앱 시작 후 `Headset starting` → `Headset running`, MR → 깊이 테스트 → 범위 → 실세계 깊이 추정 순서가 로그·화면에 맞는다. 손이 차량 앞에 보이는지 §1·§2 순서로 확인한다.
- [ ] 세션이 없으면 20초 뒤 `Headset not running`과 VR이 표시된다. 늦게 세션을 시작하면 MR·마커 추적 요청이 적용된다. 시작 전 추적을 꺼두면 자동으로 켜지지 않는다.
- [ ] 기본 창은 MR/VR·상태를 항상 표시하고 Calibration / Vehicle 탭을 제공한다. Advanced settings는 별도 창 하나다. 다시 열기, X로 숨기기, 같은 탭·위치 복원, 기본 창 닫기·PIE 종료 시 함께 닫기를 확인한다.
- [ ] Starting, Waiting for markers, Reading markers, saved position reused, recalculated from markers, Adjusted · not saved, Saving alignment, Headset not running의 문구와 관측 마커가 실제 상태에 맞는다.
- [ ] 필요 마커를 모두 유지하고 Save alignment 또는 Enter를 한 번만 누른다. 새 평균의 진행률 뒤 자동 저장된다. 저장 중 하나를 가리거나 관측이 만료되면 취소되며 빠진 ID를 표시한다. 다시 요청해야 한다.
- [ ] 10초 안에 마커당 최소 표본을 채우지 못하면 시간 초과가 표시되고 이전 파일이 유지된다.
- [ ] 버튼·넘버패드 조정이 같은 방향이며 Change since last save에 누적된다. 저장·Discard changes·Recalibrate 뒤 누적량이 초기화된다. 0.1 / 1 / 5 cm 선택과 Turn per press를 확인한다.
- [ ] First-time setup for a new rig의 Align to my eyes는 유효한 HMD 위치로만 배치한다. Vehicle 탭은 실제 카탈로그를 선택하고 그룹 없는 차량은 Alignment group 줄이 없다.
- [ ] 고급 MR의 Depth range, far edge를 바꾸면 가림 거리가 변한다. VR에서 바꾼 MR 깊이·Render from 값은 MR 복귀 때 적용된다. VR background는 MR 실제 가시성과 별도로 유지된다. B 키의 MR 임시 토글과 VR 설정 변경을 구분한다.
- [ ] 정상 기본 시작·VR 전환·헤드셋 없음에서 Advanced settings at defaults다. H·Dynamic을 켜면 변경 수·탭 ●·Changed가 따라가며 탭 초기화가 세션 설정도 되돌린다.
- [ ] Diagnostics의 Dynamic tracking for all markers는 새 감지 마커에도 적용되고, 끄면 보정 프로필·Marker Follower의 원래 모드 또는 Stationary로 복귀한다. 라벨로 확인한다. 앱 재시작에는 꺼짐이다.
- [ ] Calibration의 Delete saved alignment for <group>은 첫 클릭만으로 삭제하지 않는다. 5초 안에 두 번째 클릭해야 삭제된다. Discard changes는 저장 파일을 지우지 않는다.
- [ ] 모니터 배율 125%·150%, 두 창 크기 변경, 모든 탭의 아래쪽 조작·문구를 확인한다. 패키지를 만들 때 Pretendard 포함·표시도 확인한다.

## 6. 차량 선택

| 조작 | 기대 |
|---|---|
| 오른쪽 스틱 상하 | 차량 순환 (Test Car A ↔ B) |
| Vehicle 탭 차량 버튼 | 지정한 차량 로드 |
| `E` | 차량을 내 앞에 배치 |

디자인 옵션은 `project/pqdq-usb-access` 브랜치에 있다.

## 7. Human Factors — eye 스냅

**조작: 컨트롤 창 Settings → Optional eye references → Authored eye position `[-][+]`.** (스틱으로 하려면 `VarjoInput → Cycle Manikin Action`에
Axis1D IA를 지정한다 — 애셋이 아직 없다.)

⚠️ **이 절은 지금까지 검증이 불가능했다.** Ergonomics 컴포넌트는 선택적으로 추가해야 한다. 과거에는 구현돼 구독까지 하고 있었지만
`RequestErgonomicsStep`을 부르는 곳이 **하나도 없어서** 발동 자체가 안 됐다. 이 문서가
"MR + MarkerAnchor면 아무것도 안 움직이는 게 정상"이라고 적어둔 탓에, 안 움직이는 것이
정상인지 미배선인지 구분할 수도 없었다. 필요한 프로젝트에서 Ergonomics 컴포넌트·프로필을 추가한 뒤 컨트롤 창의 Authored eye position 버튼을 쓴다.

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
  - `Hand visualization ON — ... hand data received: left no, right no` + 5초 뒤 `No hand data from the OpenXR runtime` —
    **런타임이 손을 한 번도 보내지 않았다**(2026-09-15 테스트 PC에서 실제로 이 상태였다: 확장은 켜졌는데 acquired가 한 번도 없음).
    앱 쪽은 할 게 없다 → Varjo Base > Settings > System > Experimental > Hand tracking(Varjo / Ultraleap)을 확인하고 플레이를 다시 시작.
    튜닝 창 Hands의 `Right index tip`도 `no hand data from the runtime`으로 나온다(`not tracked (lost)`는 받다가 놓친 것)
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
   3칸)이 되어 `H` 스켈레톤과 생성 손 접촉(§12-4)이 같이 옮겨지고 `Tuning.json`에 저장된다. 다시 1을 해서 1cm 안쪽인지 본다
5. Varjo Base → Settings의 hand tracking이 **Varjo**인지 **Ultraleap**인지 적어 둔다

⚠️ 손을 빠르게 움직일 때만 어긋나고 멈추면 맞는다면 오프셋이 아니라 **지연**이다 — 보정하지 말 것.

측정 결과에 따라 다음 단계(손 poke로 패널 누르기)의 실현 가능성이 정해진다.

## 11. 모니터 화면 — 관전 카메라 (2026-09-15 추가)

고급 창 **Diagnostics → Monitor → Monitor shows**:
1. `Smoothed view` → 모니터가 착용자 시점을 따라가되 흔들림이 줄고 수평이 유지되는가. 헤드셋 화면은 그대로여야 한다
2. `Orbit vehicle` → 모니터가 차 주위를 천천히 도는가(튜닝 창 Monitor에서 거리·속도)
3. `Headset mirror` → 원래 미러로 돌아오는가
4. 켠 동안 헤드셋 프레임이 떨어지는지 본다(장면을 한 번 더 그린다). 떨어지면 세션 중에는 끈다
- ⚠️ MR에서는 이 화면에 실제 방이 없다(정상 — 패스스루는 엔진에 오지 않는다)

---

## 12. 프로그램 전용 기능

USB 포트·손 접촉은 `project/pqdq-usb-access` 브랜치에 있다.

## 13. 차량 공용 정렬

실제 헤드셋에서는 아직 확인하지 않았다. 차량 1·2·3에 같은 `Alignment Group`, 동일한 보정용 마커 IDs를 지정하고, 원점 차이는 각 `Vehicle Root Offset`으로 맞춘다.

- [ ] 1번 차량을 초기 맞추기·미세조정하고, 모든 마커를 유지한 채 저장을 한 번 요청해 새 평균·자동 저장한다. 패널에 그룹 이름이 표시된다.
- [ ] 마커를 가린 상태에서 2·3번 차량으로 바꾼다. 확인한 앵커 위치·방향이 유지되며 현재 HMD 위치로 다시 맞춰지지 않는다.
- [ ] 각 차체와 운전자 눈 기준점이 의도한 공통 기준에 놓이는지 확인한다. 모델 오프셋이 잘못된 차량은 그룹을 바꿔 해결하지 않는다.
- [ ] 2번에서 정렬을 조정·저장하고, 앱 재실행 후 1번을 현재 마커로 복원한다. 새 정렬이 적용된다.
- [ ] 필요 마커 하나를 가리면 복원을 기다리고 차가 움직이지 않아야 한다. 전부 보이면 정착 후 자동 복원하며 원점 재사용 종류를 로그에서 확인한다.
- [ ] 마커 ID 구성이 다른 차량은 공용 파일을 적용·덮어쓰지 않는다. 실패 이유가 패널에 표시된다.
- [ ] `Reset Adjustment`는 임시 조정만 취소한다. `Advanced Placement → Reset Group Alignment`는 5초 안에 두 번 클릭해야 공용 저장을 지우며, 그룹의 다른 차량도 그 저장을 다시 읽지 못한다.
- [ ] `None`으로 바꾸고 차량을 다시 로드하면 기존 차량별 저장이 남아 있다. 공유 그룹과 개별 파일이 섞이지 않는다.

## 14. MR/VR 노출 프리셋과 마이그레이션

- [ ] 저장 노출 키가 없는 PC에서는 BeginPlay와 MR/VR 전환 뒤에도 레벨 PPV의 bias·override가 유지된다.
- [ ] MR만 조정하고 VR로 전환하면 레벨 기준값으로 돌아간다. 양쪽을 조정하면 각 값이 전환·재시작 뒤 유지된다.
- [ ] 옛 `Display.Exposure`만 있는 설정은 MR·VR 양쪽으로 이어진다. 새 키가 하나라도 있으면 옛 키로 빈 모드를 채우지 않는다.
- [ ] VR에서 MR 전용 깊이·마스킹 설정이 비활성화된다. 카메라 밝기는 Varjo Base에서 확인한다.
- [ ] 실제 foveated rendering은 런타임 설정으로 확인한다. 시각화 오버레이는 제거됐다.
- [ ] 프로그램별 Blueprint의 옛 컴포넌트·입력 호출을 정리한 뒤 컴파일한다. `/CXMR/` 공통과 `/Game/` 프로그램 데이터 경계를 유지한다.

## 15. XR-4 base station 판 첫 시험 (2026-10-15)

XR-4 고정 초점에 SteamVR base station 추적을 붙인 판이다. 판매처는 "XR-4 Fixed Focus (2026)"로 표기하고, Varjo가 따로 발표한 새 모델은 아니다. 지금까지 시험한 2023판(HS-10, inside-out만)과 같은 프로젝트를 코드 수정 없이 그대로 쓴다. 바뀌는 것은 헤드셋 위치를 재는 방식뿐이다. 카메라로 하는 패스스루·마커·손·깊이는 같은 방식이지만, 2023판과 카메라가 같은지는 확인하지 못했다.

### 15-1. 준비 (Varjo 지원 문서 "Setting up SteamVR Tracking" 기준)

- [ ] SteamVR을 설치한다. base station 추적에 필요하다.
- [ ] base station: 앉아서 하는 시험이면 2개면 된다(2.0 권장).
  - 높이 2 m 이상에, 30–45° 아래를 향하게 둔다.
  - 헤드셋과 1.5 m 이상, base station끼리는 5 m 이내로 둔다.
  - 헤드셋까지 가리는 것이 없어야 한다.
  - 채널은 서로 다르게 맞춘다.
- [ ] 반사면을 가린다. base station 추적도 반사면에 방해받는다.
- [ ] Varjo Base에서 헤드셋과 base station이 연결됐는지 본다. SteamVR에서 Room setup을 한다(문서 권장 "Standing Only").
- [ ] **Room setup을 다시 하거나 base station을 옮기면 좌표 원점이 바뀐다.** 시험하는 동안에는 건드리지 않는다. 15-3이 이 전제로 잰다.
- [ ] **Varjo 컨트롤러는 SteamVR 추적에서 쓸 수 없다**(같은 문서). 데스크톱 창과 키보드로 조작한다.

### 15-2. 앱을 켜자마자 — OpenXR 런타임이 Varjo인가

SteamVR을 설치하면 시스템 OpenXR 런타임이 SteamVR로 바뀔 수 있다(이 PC에서 확인하지 않았다). 그러면 Varjo 확장이 없어서 MR·마커·깊이가 전부 조용히 안 된다.

- [ ] `Saved/Logs/GMTCK_MR.log`에서 `OpenXR runtime supported extensions:` 아래 목록을 본다. 다음이 있어야 한다:
  - `XR_VARJO_marker_tracking`
  - `XR_VARJO_composition_layer_depth_test`
  - `XR_VARJO_environment_depth_estimation`
  - `XR_VARJO_view_offset`
  - `XR_EXT_hand_tracking`

  없으면 Varjo Base나 SteamVR 설정에서 OpenXR 런타임을 Varjo로 되돌리고 앱을 다시 켠다.
- [ ] 같은 목록에 **`XR_HTCX_vive_tracker_interaction`**이 있는지 적는다. 있으면 나중에 Vive Tracker로 움직이는 실물을 추적할 수 있다. 없으면 이 런타임으로는 트래커를 못 쓴다.
- [ ] 로그 파일을 통째로 붙여 준다.

### 15-3. 원점이 세션 사이에 그대로인가 — 저장 위치 재사용(U-2)의 근거

base station 추적은 원점이 base station에 묶여 있어서, inside-out보다 세션 사이에 원점이 그대로일 가능성이 높다. 그렇다면 "맞춘 차 위치를 그대로 저장"하는 방식이 가장 정확해진다(`Docs/Specs/oct-review-fixes.md` U-2).

- [ ] 마커로 보정한다. 키로 차를 옮기지 않는다.
- [ ] PIE 중 World Outliner에서 차량 Root 액터(`BP_VehicleRoot_TestCar`)를 골라 Details의 Location·Rotation을 적는다.
- [ ] PIE를 끝냈다 다시 시작하고, 같은 자리에서 보정해 다시 적는다. 세 번 반복한다.
- [ ] 2023판으로도 같은 방법을 해 볼 수 있으면 같이 적는다. 그래야 비교가 된다.
- [ ] 세 값이 1 mm 안에서 같으면 원점이 그대로인 것이다. 몇 mm 이상 다르면 원점이 움직이거나 마커 측정이 흔들리는 것이다. 두 가지는 지금 빌드로는 구분할 수 없다. 마커 좌표 로그(oct-review-fixes R1-5) 뒤에 구분한다.

### 15-4. 박스에 붙인 마커를 메시가 잘 따라가는가

**CXMR Marker Follower** 클래스로 한다(명세 `Docs/Specs/marker-follower.md`). **이 클래스가 머지된 빌드에서만 된다.** 시험 PC에서 git pull 후 빌드한다. Varjo 예제 맵은 쓰지 않는다.

1. [Operating-Guide §3-2](Operating-Guide.md#3-2-마커를-따라가는-메시-만들기)의 "만드는 법"대로 `BP_Dynamic`을 만들어 `L_Main`에 놓는다. `Tracking Mode`는 Stationary로 시작한다.
2. VR Preview에서 박스를 보이게 든다. 메시가 박스로 오고, 라벨이 `Stationary`인지 본다.
3. 박스를 천천히, 그다음 빠르게 움직인다. 메시가 얼마나 늦게 따라오는지, 멈췄을 때 떨리는지, 손이 마커를 가리면 언제 `lost`가 되는지 본다.
4. 레벨에 놓은 `BP_Dynamic`을 고르고 Details에서 `Tracking Mode`를 Dynamic으로 바꾼다. PIE 중 바꿔도 된다. 라벨이 `Dynamic`이 되면 3을 반복한다.
5. 마커 축 표시(튜닝 창 "Show marker axes and labels")를 켜고, 빨강·초록·파랑 중 어느 축이 마커 면 바깥을 향하는지 사진 한 장을 남긴다.
6. 결과는 한두 줄이면 된다.

- **박스가 보이는 상태로 Learn marker layout을 누르지 않는다.** 지금은 보이는 마커를 전부 차량 보정 마커로 등록한다(리뷰 `Docs/Reviews/marker-follower.md`, 1번 작업에서 고침).
- `BP_Dynamic` 크기는 BP 안의 메시나 레벨 액터의 Scale로 맞춘다. 감지·이동 뒤에도 레벨 액터의 크기가 유지되는지 본다(같은 리뷰 R1-1 반영).
- 시작 전 추적 꺼짐에는 경고하지 않는다. 추적이 켜졌다 꺼질 때만 "Marker tracking is off" 경고가 한 번 나오는지 본다(같은 리뷰 R1-2 반영).

이 절의 실제 헤드셋 시험 상태는 **not-tested**다. 로컬 빌드·합성 이벤트 시험은 실제 마커 추적 결과가 아니다.

### 15-5. 결과와 함께 적을 것

- 헤드셋 정확한 모델명(Varjo Base 기기 정보), Varjo Base 버전, SteamVR 버전, base station 버전·개수
- 15-2 로그, 15-3 숫자, 15-4 관찰

### 15-6. 2026-10-15 결과 (사용자, 2시간)

- 장비: XR-4 base station 판, Varjo Base **Pro 4.16**, SteamVR, base station.
- **Varjo Base Occlusion**(4.16에서 새로 생김): 앱의 깊이 테스트·실세계 깊이 추정과 같이 켜면 화면이 이상했다. Apple Vision Pro만큼 깔끔하지 않았다. → 앱 깊이 테스트를 기본으로 끈다(`Docs/Specs/depth-default-off.md`).
- **나이트 모드**(Varjo Base): 밝기를 낮춰 밤처럼 보인다. 야간 차량 평가 후보다(`Docs/Varjo-Capabilities.md` §15).
- **15-4(움직이는 마커)는 시간 안에 시험하지 못했다.** → 바로 시험할 수 있게 예제 레벨 `L_DynamicMarker`를 만든다(`Docs/Specs/dynamic-marker-example.md`).
- 15-2(확장 목록)·15-3(원점 재사용) 숫자는 받지 못했다. 다음 시험에서 한다.

## 아무 반응이 없는 키 (제거된 매핑)

`G` 시선 점, `I` foveation 오버레이, `F` 손목 클릭, `C` 전체 마커 Dynamic 전환은 매핑에서 제거했다.
턴테이블·트림 입력도 제거됐다. 마커별 `Tracking Mode`는 프로필에서 정한다.
`Y`와 방향키는 Depth Test Range에 배선돼 있다(§2). 넘버패드가 안 먹으면 **NumLock**부터 본다.

실제 헤드셋 검증은 이 목록으로 별도 수행한다. 빌드·합성 자동화 통과를 실측 완료로 적지 않는다.
