# 에디터에서 해야 할 작업 (2026-07-27 실기 세션 후속)

C++ 쪽은 전부 반영됐다. 여기 있는 것은 **`.uasset` 편집이라 에디터가 열려 있어야 하는** 작업이다.

⚠️ **C++ 빌드는 에디터를 닫고, 애셋 편집은 에디터를 열고.** 같은 턴에 섞지 않는다
(Operating-Guide §10 — 실제로 워킹트리가 반쯤 지워진 전례가 있다).

순서는 우선순위 순이다. 1번이 다른 모든 MR 검증의 전제조건이다.

---

## 1. 🔴 PP_MR 알파 — 패스스루가 안 뜨는 원인 (최우선)

**증상**: MR을 켜고 `B`로 VR 배경을 끄면 패스스루가 아니라 **검정 화면**.
`T`(Depth Test)를 켤 때만 패스스루가 보임.

**가설**: `PP_MR`이 마스크 구멍 **바깥 전 영역에 Opacity 1.0**을 써서 화면 전체를 불투명하게
만든다. 패스스루는 알파 0인 픽셀에만 보이므로, 배경을 숨겨도 남는 것은 알파 1의 검정이 된다.

### 1-1. 먼저 판별한다 (30초)

1. `/Game/Map/L_Main` 열기
2. `PostProcessVolume_0` → `Rendering Features > Post Process Materials > Array`에서
   **`PP_MR` 항목을 일시 제거** (지우지 말고 배열에서 빼두기)
3. Play → `M`(MR on) → `B`(VR 배경 off)

| 결과 | 다음 |
|---|---|
| **패스스루가 뜬다** | 가설 확정 → 1-2로 |
| 여전히 검정 | `PP_MR`이 아니다 → 1-3으로 |

### 1-2. 확정된 경우 — 머티리얼 수정

`/CXMR/Core/Materials/PP_MR`을 연다.

고쳐야 할 것: **마스크 구멍이 아닌 픽셀에서 알파를 건드리지 않아야 한다.**
현재는 상수 1.0을 내보내 배경의 자연스러운 알파 0을 덮어쓴다.
→ 그 자리에 `SceneTexture:PostProcessInput0`의 **알파를 그대로 통과**시킨다.

- 마스크 구멍(= `CustomDepth < SceneDepth`이고 `MRMask`가 1)에서는 지금처럼 Opacity 0
- 그 외에는 입력 알파 패스스루

⚠️ `PP_MR`은 `/CXMR/` **공용 콘텐츠**다. 고치면 모든 프로그램에 영향이 간다.
⚠️ **`N`(마스킹) 회귀를 반드시 함께 확인한다** — 구멍이 계속 뚫려야 한다.

### 1-3. PP_MR이 아닌 경우 — 알파 파이프라인

이 경우는 프로젝트 설정 쪽이다. 확인 순서:

- `Project Settings > Rendering > Postprocessing > Enable alpha channel support`
  (config의 `r.PostProcessing.PropagateAlpha=True`가 실제로 먹고 있는지)
- **TSR의 알파 처리** — `r.AntiAliasingMethod=3`이다. TSR이 알파를 1로 resolve하면 같은 증상이 난다
- `r.SceneColorFormat` — Capabilities §2가 경고하는 64bit 포맷 이슈 (단 그 증상은 "패스스루만
  보임"이라 이번과 반대 방향이다)
- `r.DefaultBackBufferPixelFormat=4`가 알파를 담을 수 있는 포맷인지

---

## 2. WBP_CXMRControlPanel — 행 추가

C++ 쪽 `BindWidgetOptional` 계약이 늘었다. WBP에 없는 이름은 조용히 스킵되므로,
**아래 위젯을 추가하기 전까지 해당 기능은 패널에 나타나지 않는다.**

현재 WBP에는 `Btn_` 14 + `Txt_` 12 = **26개**가 있다(커밋 `32028c4`의 "계약 26/26").
그 뒤로 손 추적·depth range·착좌가 C++에 추가되면서 계약이 커졌다.

### 추가할 위젯 (기존 토글 행 레이아웃을 그대로 복제하면 된다)

| 이름 | 종류 | 용도 |
|---|---|---|
| `Btn_Hands` | Button | 손 시각화 토글 (`H`와 같은 기능) |
| `Txt_Hands_State` | TextBlock | ON / OFF |
| `Btn_DepthRange` | Button | Depth Range 토글 (`Y`와 같은 기능) |
| `Txt_DepthRange_State` | TextBlock | ON / OFF |
| `Txt_DepthRange` | TextBlock | `0.00 - 0.75 m` 또는 `unbounded` |
| `Btn_PrevManikin` | Button | 착좌 이전 |
| `Btn_NextManikin` | Button | 착좌 다음 |
| `Txt_ManikinName` | TextBlock | 마니킨 이름 |
| `Txt_ManikinPos` | TextBlock | `2 / 3` |

C++이 클릭 바인딩과 텍스트·색을 전부 처리한다. **WBP는 이름만 맞으면 되고 이벤트 그래프는 비운다**
(유령 이벤트 그래프가 컴파일을 막은 전례 — 커밋 `0414e61`).

### ⚠️ 행이 늘면 `PanelDrawSize`도 늘려야 한다

`BP_CXMRPawn → Panel Draw Size`가 현재 `432 × 721`이고, **WBP 콘텐츠 높이와 일치해야** 잘리지
않는다. 행을 추가한 뒤 WBP의 실제 콘텐츠 높이를 재서 두 값을 맞춘다.
물리 크기가 커지는 게 싫으면 `Panel Scale`(현재 0.03)을 함께 낮춘다.

---

## 3. 마커 프로파일 ID — 실물 번호로

`/Game/Vehicle/Example/DA_MarkerProfile_TestCar` → `Markers[0] → Marker Id`

현재 **0**인데, **0은 Varjo 플러그인이 "무효 ID" 센티넬로 쓰는 값**이다
(`VarjoMarkersPlugin.cpp:150, :162`가 timeout·tracking mode 설정을 거부한다).
실물 인쇄 마커는 0을 보고하지 않으므로 **지금 상태로는 차량이 절대 안 움직인다.**

**실측 절차**:
1. 헤드셋에서 `V`(마커 추적 on) → 콘솔 `CXMR.DebugMarkers 1`
2. 실물 마커를 시야에 넣고 로그에서 `LogCXMRDebug: DETECTED id=N` 확인
3. 그 `N`을 `Marker Id`에 입력

C++ 쪽에는 이미 가드를 넣어서, ID 0이면 프로파일 이름을 담은 경고가 찍힌다:
`LogCXMRPlacement: Warning: Marker profile '...' contains id 0 ...`

---

## 4. IA_CXMR_CycleManikin — 착좌 순환 스틱 (선택)

착좌 순환은 **패널 버튼으로 이미 동작한다.** 스틱으로도 하고 싶을 때만 하면 된다.

1. `/CXMR/Core/Input/Actions/`에 `IA_CXMR_CycleManikin` 생성 (**Value Type: Axis1D**)
2. `IMC_Varjo`에 매핑. **남은 키가 별로 없다** — 다음은 이미 점유 중이다:
   - `M B N K T U V H R E F` — CXMR 기능
   - `Y` `←→↑↓` — **depth range (신규)**
   - `G` `C` `I` — Varjo 예제 미배선 액션 (재활용 가능)
   - 컨트롤러: 왼쪽 스틱 X + A/B(턴테이블), 오른쪽 스틱 X/Y(트림·차량), 오른쪽 트리거(패널)
3. `BP_CXMRPawn → VarjoInput → Cycle Manikin Action`에 지정

트림/차량 순환과 동일한 히스테리시스(0.6 진입 / 0.3 복귀)가 자동 적용된다.

---

## 5. 확인할 것 (애셋 작업 후)

| 항목 | 기대 |
|---|---|
| 1 | `M` + `B` → 패스스루. **그리고 `N`으로 마스크 구멍이 여전히 뚫리는지** |
| 2 | 패널에 Hands / Depth Range / 착좌 행이 보이고, 각 키와 동기화되는지. 패널이 안 잘리는지 |
| 3 | 마커 위에 차량이 얹히고, 마커를 움직이면 따라오는지 |
| 4 | 스틱 한 번 튕김에 착좌가 한 칸만 넘어가는지 |
