# 에디터에서 해야 할 작업 (2026-07-27 실기 세션 후속)

C++ 쪽은 전부 반영됐다. 여기 있는 것은 **`.uasset` 편집이라 에디터가 열려 있어야 하는** 작업이다.

⚠️ **C++ 빌드는 에디터를 닫고, 애셋 편집은 에디터를 열고.** 같은 턴에 섞지 않는다
(Operating-Guide §10 — 실제로 워킹트리가 반쯤 지워진 전례가 있다).

순서는 우선순위 순이다. 1번이 다른 모든 MR 검증의 전제조건이다.

---

## 1. ✅ PP_MR 알파 — 적용됨 (2026-09-13), XR-4 확인만 남음

**2026-09-13 처리** (UnrealAgent Python으로 아래 1-2를 적용했다):

- **구멍 밖 Opacity**: 상수 1 대신 **(1 − `PostProcessInput0` 알파) × 구멍 계수**를 쓴다. RGB에도 같은 계수를
  곱해 premultiplied를 유지한다. 가상 물체는 1, 빈 배경은 0, 마스크 구멍은 0이 나간다.
  - ⚠️ **2026-09-14 정정**: 처음(09-13)에는 `1 −` 없이 알파를 그대로 통과시켰다. 실기에서 **VR은 반투명, MR은
    검은 화면**이 됐다. 언리얼 내부 알파는 **0 = 불투명, 1 = 비어 있음**이고(`OpenXR.AlphaInvertPass` 설명,
    `OpenXRHMD.cpp:152`, 기본 꺼짐), OpenXR 런타임은 1 = 불투명으로 읽는다. Varjo 원본이 씬 알파 대신
    depth 비교로 알파를 새로 만든 이유가 이것이다.
- **`If`의 `A > B` 입력**: Varjo 원본의 `Min(Ceil(Custom − Scene), 1)` 대신 **상수 1**을 쓴다. 원래 식은
  "빈 픽셀은 두 depth가 무한대로 같다"는 가정으로 알파 0을 만들었다. 이제는 빈 픽셀을 (1 − 입력 알파) = 0이 처리하므로
  그 가정이 필요 없다. `A == B`는 미연결이라 `A > B`를 따라 1이 된다. 그래서 커스텀 뎁스와 메인 패스를 둘 다 그리는
  물체가 뚫리지 않는다.
- 🔴 **같이 찾은 회귀**: 08-11 커밋(`20e118d`)에서 PP_MR의 콜렉션 참조가
  `/Game/VarjoContent/MixedReality/PP_MRParameters`로 바뀌어 있었다. 그런데 C++(`CXMRMaskingComponent`)은
  `/CXMR/Core/Materials/PP_MRParameters`에 값을 쓴다. 그래서 **`N`이 머티리얼에 닿지 않았다**. 게다가
  `Content/VarjoContent/`는 gitignore라 **새 clone에서는 참조가 깨진다**. 참조를 `/CXMR/`로 되돌렸다.
  - ⚠️ Python으로 콜렉션을 바꿀 때는 `parameter_name`을 같은 값으로 다시 넣되 `notify_mode=ALWAYS`를 줘야 한다.
    컴파일은 이름이 아니라 `ParameterId`(GUID)로 파라미터를 찾고, GUID를 다시 푸는 곳이 PostEditChange뿐이다.
- **`L_Main` PostProcessVolume**: PP_MR 가중치를 0(7월 실기 때의 임시 우회)에서 **1로 되돌렸다**.

**확인한 것**: 컴파일 에러 없음(PS 86 instructions). 저장된 `.uasset`에 `VarjoContent` 참조 0개.
asset registry 의존성은 `/CXMR/Core/Materials/PP_MRParameters` 하나뿐이다.

**남은 것**: 헤드셋에서 §5 표 1번(`M`+`B` → 패스스루, `N` → 구멍)을 본다. 그래도 검으면
[Passthrough-Black-Screen.md](Passthrough-Black-Screen.md) §2의 C → B → D로 간다. A는 이번 수정으로 소거됐다.

---

(이하 원래 기록)

**증상**: MR을 켜고 `B`로 VR 배경을 끄면 패스스루가 아니라 **검정 화면**.
`T`(Depth Test)를 켤 때만 패스스루가 보임.

**✅ 원인 확정 (2026-07-27) — 1-1 판별은 이미 끝났다.**
필립님이 실기에서 **PostProcessVolume의 `PP_MR` 값을 0으로 내리자 곧바로 MR이 동작**했다.
이것이 1-1이 시키는 A/B 테스트 그 자체다. **1-2로 바로 간다.**

**원인**: `PP_MR`이 마스크 구멍 **바깥 전 영역에 Opacity 1.0**을 써서 화면 전체를 불투명하게
만든다. 패스스루는 RGBA가 (0,0,0,0)인 픽셀에만 보이므로, 배경을 숨겨도 남는 것은 알파 1의 검정이다.

**왜 이런 머티리얼이었나**: Varjo 예제는 **불투명한 가상 방**에 구멍을 뚫는 전제라 바깥 알파 1이
옳다. CXMR은 반대로 **배경 자체가 패스스루**여야 하므로 같은 레시피가 정면으로 충돌한다.
예제 레시피를 전제 확인 없이 옮겨온 것이 화근이다.

### 1-1. (완료) 판별

~~PostProcessVolume에서 `PP_MR`을 배열에서 빼고 Play~~ → **실기에서 확정됨.**

### 1-2. 머티리얼 수정

`/CXMR/Core/Materials/PP_MR`을 연다.

고쳐야 할 것: **마스크 구멍이 아닌 픽셀에서 알파를 건드리지 않아야 한다.**
현재는 상수 1.0을 내보내 배경의 자연스러운 알파 0을 덮어쓴다.
→ 그 자리에 **1 − `SceneTexture:PostProcessInput0` 알파**를 쓴다. 언리얼 내부 알파는 0 = 불투명이다(2026-09-14 정정).

- 마스크 구멍(= `CustomDepth < SceneDepth`이고 `MRMask`가 1)에서는 지금처럼 Opacity 0
- 그 외에는 1 − 입력 알파

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

## 2. ~~WBP_CXMRControlPanel — 행 추가~~ → 2026-09-15 C++ 패널로 교체, 에디터 작업 없음

컨트롤 패널은 이제 **C++(`UCXMRControlPanelWidget`)이 튜닝 창과 같은 모양(`CXMRPanelUI`)으로 직접 그린다.**
`/CXMR/Core/UI/WBP_CXMRControlPanel`은 C++ 어디에서도 참조하지 않는다. 그래서 이 항목의 할 일이 전부 사라졌다.

- 예전 WBP 방식은 위젯 이름(`Btn_*` / `Txt_*`)으로 C++과 연결돼 있었고, `BindWidgetOptional`이라 위젯이 사라지거나
  이름이 틀려도 **조용히** 동작을 멈췄다(실제로 `Btn_ViewOffset` 행이 소실된 적이 있다)
- 착좌(매니킨) 행은 이제 Viewer 탭에 있다 — WBP에 위젯 4개를 추가할 필요가 없다
- 행을 추가하려면 `CXMRControlPanelWidget.cpp`의 해당 탭 함수에 한 줄을 넣는다. 레이아웃 계산도, DrawSize 맞추기도 필요 없다
  (내용이 길면 스크롤된다)
- `BP_CXMRPawn`이 옛 WBP를 `Control Panel Class`로 덮어쓰고 있어도 같은 모양이 나온다(WBP의 부모가 C++ 패널이라서).
  WBP 애셋 자체는 지워도 되지만, 지우는 것은 되돌리기 어려우니 따로 결정한다

---

## 3. 🔴 마커 프로파일 ID — 실물 번호로 (헤드셋 테스트의 전제조건)

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

**Local Offset은 이제 안 넣어도 된다** — 번호만 맞으면 현장에서 `CXMR.LearnMarkers`로 학습한다
(Operating-Guide §3-1). 하지만 **번호 등록은 여전히 에디터에서 해야 한다.** 미등록 마커를
자동으로 프로파일에 넣는 기능은 아직 없다.

---

## 3-1. 마커 캘리브레이션 영속화 (2026-08-08 추가, 미검증)

`Saved/CXMR/MarkerCalib_<프로파일명>.json`에 저장/복원하는 경로를 넣었다. **빌드만 확인했고
실제로 파일이 써지는지는 안 돌려봤다.** 첫 테스트에서 볼 것:

1. PIE에서 `CXMR.SaveCalibration` → 로그에 경로가 찍히고 그 자리에 파일이 생기는지
2. 값을 바꾸고 저장 → PIE 재시작 → 값이 살아 있는지
3. `CXMR.ResetCalibration` → 파일이 사라지고 원래 값으로 돌아가는지

⚠️ `.gitignore`가 `Saved/*`를 제외하므로 이 파일은 리포에 올라가지 않는다. **의도한 것이다** —
캘리브레이션은 물리적 설치 하나에만 유효한 값이다. 현장 값을 보관하려면 파일을 따로 받아둔다.

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
