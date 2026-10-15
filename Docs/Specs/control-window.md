# 컨트롤 창 새로 만들기 + MR로 시작

- 베이스 브랜치: `main`. 2026-10-14 사용자 요청으로 여러 작업을 한 번에 하므로, 구현은 `codex/main-basic` 끝에 쌓는다(HANDOFF). 리뷰는 그 브랜치와의 diff로 한다.
- 상태: 구현 대기 — [oct-review-fixes.md](oct-review-fixes.md)와 [main-basic.md](main-basic.md)가 `main`에 머지된 뒤 시작한다

## 목적

사용자 결정(2026-10-14):

- 진행자 창은 디자인부터 문구까지 다시 만든다.
- 앱은 **MR로 시작**하고, 깊이 테스트와 실세계 깊이 추정이 켜진 상태여야 한다(착용자에게 자기 손이 보여야 함).
- 기본 창에는 MR/VR 전환, 보정과 보정 진행 상태만 둔다. 세부 옵션은 **Advanced 버튼으로 여는 별도 창**에 둔다.

시안은 사용자가 승인했다(2026-10-14).

## 원본 — 시안이 기준이다

**[control-window/prototype.html](control-window/prototype.html)** 을 브라우저로 열어 본다. 위쪽 "Prototype controls"의 상황 버튼과 마커 체크로 상태를 바꿀 수 있다. 사용자가 본 게시본은 https://claude.ai/artifact/4nuryji6K37fTp7a4mxsZw 이고 내용은 같다(글꼴 형식만 다름).

시안이 정하는 것 — 명세에 다시 적지 않는다:

- 두 창의 배치·크기 비율·색·글꼴·간격·모서리
- **모든 화면 문구.** 대소문자·구두점·`·`·`…`까지 그대로 쓴다.
- 상태 줄의 경우와 문구: 스크립트의 `status()`
- 버튼이 켜지고 꺼지는 조건과 막혔을 때의 이유 문구: `renderActions()`, `renderAdjust()`
- 고급 창의 탭·묶음·행 순서·라벨·도움말·기본값: 스크립트의 `SETTINGS` 배열. `reg`가 레지스트리 ID이고 `new:`는 새로 등록할 항목이다.
- 실행 행의 버튼 문구와 결과 문구: `row()`, `runAction()`

시안에서 만들지 않는 것: 맨 위 "Prototype controls" 상자, 아래 "What this design changes", 페이지 제목 줄. 가짜 제목 표시줄(— ▢ ✕)은 OS 창 장식으로 대신한다. 시안의 예시 값(마커 3·7, Preview vehicle A/B, PreviewRig, 0.12 cm, 손끝 좌표, 1920 × 1080 등)은 실제 데이터로 채운다.

**시안에 없는 화면 요소는 만들지 않는다. 시안대로 할 수 없으면 리뷰 파일 "질문"에 적고 멈춘다.**

## 범위

### A. MR로 시작

1. 새 설정 `MR.StartInMR`(Bool, 저장, 기본 켬). 고급 창 MR 탭 첫 행이다.
2. 켜져 있으면 시작할 때 MR을 "요청" 상태로 둔다. `IsXRSessionRunning()`(`CXMRSubsystem.cpp:42`)이 참이 되는 즉시 다음 순서로 켠다. 세션 전 Varjo 호출은 헤드셋 PC에서 크래시가 난다(AGENTS.md).
   1. MR
   2. 깊이 테스트(`Depth.Test` 설정값)
   3. 깊이 범위(`Depth.Range` 켬, Near/Far 설정값)
   4. 실세계 깊이 추정(`Depth.EnvEstimation` 설정값, 깊이 테스트가 켜졌을 때만)

   View offset을 세션 뒤에 적용하는 지금 방식(`CXMRSubsystem.cpp:377-391`)과 같은 틀로 한다.
3. 20초 안에 세션이 뜨지 않으면 VR로 두고 상태를 "Headset not running"으로 바꾼다(시안 "No headset"). 그 뒤 세션이 뜨면 그때 위 순서로 MR을 켠다. 20초는 마커 추적 시작 재시도(`CXMRPlacementComponent.cpp:507`)와 같은 값이다.
4. `Depth.Test`·`Depth.EnvEstimation`은 지금 저장되지 않는 설정이다. **저장되는 설정으로 바꾸고 기본값을 켬으로** 한다. `Depth.Range`는 창에서 빼고 시작할 때 항상 켠다. 범위를 끄면 방 전체가 깜빡인다(`Docs/Headset-Test-Checklist.md` §2).
5. VR 버튼은 MR만 끈다. 깊이 설정값은 그대로 두고, MR로 돌아오면 다시 적용한다.

### B. 기본 창 — CXMR Control

지금 창(`UCXMRTuningWindowComponent` + `SCXMRControlPanel`, 왼쪽 메뉴 3페이지, 900×760)을 시안의 왼쪽 창으로 바꾼다.

| 시안 요소 | 연결할 기능 |
|---|---|
| MR / VR 버튼 | MR 켜기·끄기(`MR.MixedReality`). 세션 시작 대기 중이면 MR은 "요청" 표시 |
| 헤드셋 칩 | 세션 실행 중 / 시작 대기(20초 안) / 없음 |
| 모드 설명 줄 | MR이면 `Depth.FarZ` 값을 넣은 문구, VR이면 `MR.VRBackground`에 따른 문구 |
| 상태 줄 | 아래 "상태 판정" |
| Markers 목록 | 필요 마커(oct-review-fixes U-1)와 마커별 관측 여부 |
| Save alignment | 한 번 누르면 저장(아래 C) |
| Discard changes | `Vehicle.ResetOffset` |
| Recalibrate | `Vehicle.Recalibrate` |
| 이동 간격 0.1 / 1 / 5 cm | `Vehicle.MoveStep`. 저장값이 셋 중 하나가 아니면 아무것도 선택 안 됨 |
| Away … Turn ⟳ | `Vehicle.Away` / `Right` / `Up` / `Turn`의 Step(+1·−1). Turn은 `Vehicle.YawStep` |
| Change since last save | 마지막 저장·버리기·보정 뒤 누른 조정량의 합. 진행자 기준 방향(Away·Right·Up)과 회전, cm·° |
| First-time setup의 Align to my eyes | 지금의 눈 정렬 시작(`UCXMRTuningWindowComponent::StartInitialAlignment`, `CXMRTuningWindowComponent.cpp:535`) |
| Vehicle 탭 차량 목록 | Loader 카탈로그 차량. "Alignment group" 문구는 그룹이 있을 때만, 없으면 그 줄을 숨긴다 |
| 바뀐 고급 설정 수 문구, Advanced settings… | 고급 창 열기 |

- **상태 판정**: Placement가 상태 하나를 구조체로 낸다. 담을 것: 단계(시작 대기·없음·마커 대기·정착 중·정렬됨·조정됨·저장 중), 복원 종류(world·markers·new·saved), 원점 이동 거리, 빠진 마커 ID, 진행률 0~1. 문구는 이 구조체에서 만드는 순수 함수로 하고 시험한다. 각 경우는 시안 `status()`와 1:1이다.
  - world·markers는 oct-review-fixes U-2의 결과다. new는 저장 없이 마커로 보정한 경우, saved는 이번 세션에서 방금 저장한 경우다.
- Enter 키는 Save alignment 버튼과 같은 동작을 한다. 넘버패드는 조정 버튼과 같은 동작을 하고, 누른 양이 "Change since last save"에 들어간다.

### C. 저장은 한 번 누르기

Save alignment(또는 Enter)를 한 번 누르면 다음 순서로 진행한다.

1. 캡처를 시작한다. 상태는 "Saving alignment"이고 진행률이 표시된다.
2. 마커마다 평균이 끝나면(oct-review-fixes R1-1의 최소 표본) **자동으로 저장**한다.
3. 그 사이 마커가 하나라도 빠지면 저장을 취소하고 "Adjusted · not saved"로 돌아간다. 이유 문구도 띄운다.

oct-review-fixes가 "확정 → 다시 눌러 저장" 두 단계로 끝났다면 여기서 한 단계로 바꾼다.

### D. 고급 창 — CXMR Advanced settings

- 별도 `SWindow`이고 하나만 뜬다. 다시 열면 기존 창을 앞으로 가져온다. ✕로 닫으면 숨기고, 다시 열면 같은 위치·같은 탭이다(세션 안에서).
- 탭은 MR / VR / Calibration / Diagnostics. 탭 안 묶음과 행은 시안 `SETTINGS` 순서 그대로다.
- **행은 레지스트리 항목으로 만든다.** 라벨·도움말·단위·선택지 문구는 레지스트리 항목(`FCXMRTunable::Label` / `Help` / `Unit` / `Options`, `CXMRTuningSubsystem.h:26-55`)에 시안 문구를 넣어 **한 곳에만** 둔다.
  - 지금 패널이 문구를 따로 덮어쓰는 표(`SCXMRControlPanel.cpp:159` 부근 `DescribeRow`)와 쓰지 않는 페이지 값(`ECXMRControlPage`의 View·Diagnostics 접기)은 없앤다. 리뷰 H-5를 같이 해결한다.
- 숫자의 최소·최대·간격은 지금 등록값을 쓴다. 시안의 범위는 참고용이다. **기본값이 시안과 다르면 질문한다.**
- 탭 이름 옆 `●`, 행의 `Changed · default …`, `Reset <탭> to defaults`, 아래 줄 문구는 시안대로 한다. "바뀐 설정"은 기본 창 아래 줄의 수와 같은 기준이다: 고급 창의 저장되는 설정 중 `Default`와 다른 것.
- `requires`: 앞 설정이 꺼지면 행을 흐리게 하고 도움말 뒤에 `Turn on “…” first.`를 붙인다.
- 실행 행: 버튼 문구는 `Run`, 삭제는 `Delete` → `Click again to delete`(5초)다. 결과 문구는 버튼 왼쪽에 성공색으로 표시한다.
- **창에서 빠지는 등록 항목**:
  - `MR.MixedReality`(기본 창 버튼)
  - `Depth.Range`(항상 켬)
  - `Vehicle.SaveOffset` / `Recalibrate` / `ResetOffset` / `Away` / `Right` / `Up` / `Turn` / `MoveStep`(기본 창)
  - `Port.*`(main-basic에서 삭제됨)
  - 1번(oct-review-fixes)에서 새로 생기는 프로퍼티(마커당 최소 표본 수, 캡처 시간 제한)와 0번 Marker Follower의 값. 시안에 없으므로 창에 넣지 않는다. 레지스트리에 등록했다면 창에서만 뺀다.
  - 이 밖에 시안에 없는 등록 항목을 찾으면 질문한다. 2026-10-15 Claude가 확인했을 때 지금 등록된 47개는 전부 시안이나 이 목록에 자리가 있었다.
- **새로 등록할 항목** (`SETTINGS`의 `new:`):
  - `MR.StartInMR`
  - 마커 평균 시간(`CalibrationSettleSeconds`)
  - 원점 재사용 허용 오차 두 개(oct-review-fixes U-2의 프로퍼티)
  - 마커 좌표 로그 실행(oct-review-fixes R1-5의 `CXMR.DumpMarkers`)
  - `Diagnostics.DynamicMarkers` — "Dynamic tracking for all markers". 켜면 감지된 모든 마커(새로 감지되는 마커 포함)에 Dynamic 모드를 건다. 끄면 각 마커를 원래 모드로 되돌린다: 보정 마커는 프로필 값, Marker Follower 액터가 따라가는 마커는 그 액터의 `TrackingMode`, 나머지는 Stationary. **저장하지 않는 설정**이라 앱을 다시 켜면 꺼져 있다. 마커 라벨(`CXMRMarkerDebugComponent.cpp:223-224`)이 이미 마커별 모드를 보여 주므로, 확인은 라벨과 축으로 한다. 10-07에 지운 `C` 키와 같은 기능이고, 키는 되살리지 않는다.

### E. 모양 — CX Toolkit "Showroom"

- **색**: 시안 `:root`의 토큰 값을 `CXMRPanelUI`의 색 함수(`CXMRPanelUI.cpp:160-185`)에 그대로 넣는다. 밝은 테마는 없다.
- **글꼴**: [control-window/fonts/](control-window/fonts/)의 `PretendardStd-Regular.ttf`·`Medium.ttf`·`SemiBold.ttf`와 `OFL-LICENSE.txt`를 `Plugins/CXMR/Resources/Fonts/`로 복사한다.
  - 출처: Pretendard Std v1.3.9 공식 배포본(`public/static/alternative/`)이고 SIL OFL 1.1이다. 한글이 없는 판이다(UI는 영어).
  - Slate에서 파일로 불러온다. 파일이 없으면 엔진 기본 글꼴로 대신하고 경고 로그를 남긴다(크래시 금지).
  - 패키지 빌드에 들어가게 `CXMR.Build.cs`에 등록한다(`RuntimeDependencies` 등).
  - UE의 FreeType은 WOFF2를 읽지 못한다(`FT_CONFIG_OPTION_USE_BROTLI` 꺼짐, `Engine/Source/ThirdParty/FreeType2/FreeType2-2.10.4/include/freetype/config/ftoption.h:306`). 그래서 TTF를 쓴다.
- **글꼴 크기**: Slate 글꼴 크기는 96 DPI 기준 포인트라서 시안의 CSS px × 0.75로 넣는다.

  | 시안 | Slate |
  |---|---|
  | 12px | 9 |
  | 13px | 9.75 |
  | 15px | 11.25 |
  | 18px | 13.5 |
  | 24px | 18 |

  간격·모서리·테두리는 px 값 그대로다. 렌더 결과를 시안과 나란히 놓고 크기가 맞는지 확인하고, 다르면 보고 ③에 적는다.
- 아이콘(체크·대기·스피너·X)은 Slate 브러시로 그린다. 스피너 회전 애니메이션은 넣지 않아도 된다.
- 창 크기: 기본 창 `ClientSize` 480×820, 고급 창 560×760. 둘 다 크기 조절 가능하다. 내용이 넘치면 탭 내용 영역만 스크롤한다.

## 범위 밖

- 손에 붙는 3D 패널(`UCXMRControlPanelWidget`, `bShowHandPanel` 기본 꺼짐)과 UMG 애셋. 그대로 둔다.
- `project/pqdq-usb-access`의 USB 설정 행. 머지 때 Claude가 그 브랜치에서 고급 창에 USB 묶음을 더한다(별도 작업).
- 한국어 UI, 밝은 테마
- 창 위치를 세션 사이에 기억하는 것

## 지켜야 할 조건

- 조작 키(넘버패드, Enter, H, G, I, K, T, U, Y 등 `IMC_Varjo` 매핑)는 그대로다. 키로 바꾼 상태가 창에 즉시 보여야 한다.
- 레지스트리 ID와 `Saved/CXMR/Tuning.json` 키는 그대로다(`FCXMRTunable::Id` 주석). 정렬 저장 파일도 그대로다.
- `/CXMR/`·`/Game/` 애셋을 바꾸지 않는다. 더하는 파일은 글꼴 TTF 3개와 라이선스뿐이다.
- 헤드셋 동작을 확인했다고 쓰지 않는다.

## 근거

- 창 생성: `CXMRTuningWindowComponent.cpp:701`(`SNew(SWindow)`), 크기 `CXMRTuningWindowComponent.h:43`, PIE 종료 때 창 닫기 `CXMRTuningWindowComponent.cpp:108`
- 패널: `SCXMRControlPanel.cpp`, 스타일 `CXMRPanelUI.cpp`
- 레지스트리: `CXMRTuningSubsystem.h:26-104`
- 등록 위치: Placement `CXMRPlacementComponent.cpp:961-1180`, 창 `CXMRTuningWindowComponent.cpp:148-290`, 관전 `CXMRSpectatorComponent.cpp:296-340`, 손 `CXMRHandDebugComponent.cpp:251-290`
- 세션 판정: `CXMRSubsystem.cpp:42-49`

## 구현자가 정할 것

- Slate 위젯 구조와 파일 나누기, 상태 구조체 이름
- 아이콘 그리는 방법
- 고급 창 위치를 세션 안에서 기억하는 방법

## 경계 상황

- 세션 시작 대기 중에 MR 버튼을 누름 → MR 요청을 유지한다(시안 "Switching on when the headset is ready…").
- 헤드셋 없음(이 PC의 PIE) → 20초 동안 Starting, 그 뒤 "Headset not running", MR 버튼 비활성, 마커 체크 불가.
- 고급 창이 열린 채 기본 창을 닫거나 PIE가 끝남 → 둘 다 닫힌다.
- Advanced를 두 번 누름 → 창 하나가 앞으로 온다.
- 키로 설정을 바꿈(H·G·I 등) → 고급 창 토글과 바뀐 수가 따라간다.
- 저장 중 마커가 빠짐 → 저장 취소, Adjusted + 이유.
- 글꼴 파일 없음 → 기본 글꼴 + 경고.
- 모니터 배율 125%·150% → 잘리는 문구가 없다.

## 완료 조건

1. 시안의 여섯 상황(Starting up, Waiting for a marker, Restored · saved position, Restored · from markers, Adjusted, not saved, No headset)과 고급 창 네 탭을 Slate로 렌더해 `Saved/ControlWindowPreview_<이름>.png`로 남긴다(지금 `CXMR.Panels.RenderPreview` 방식). 보고 ⑤에 파일 목록을 적는다. Claude가 시안과 나란히 비교한다.
2. 상태 문구 순수 함수 시험: 시안 `status()`의 경우마다 문구가 같다.
3. MR로 시작 시험:
   - 세션이 없으면 요청만 한다.
   - 세션이 뜨면 MR → 깊이 테스트 → 범위 → 실세계 깊이 순서로 부른다. 실세계 깊이는 깊이 테스트가 켜졌을 때만 부른다.
   - `StartInMR`을 끄면 VR로 시작한다.
   - 20초가 지나면 "Headset not running"이다.
4. 고급 창 시험: 하나만 뜬다. 다시 열면 같은 탭이다. 바뀐 수 계산과 탭 초기화가 맞다.
5. 문구 시험: 고급 창 행마다 레지스트리 라벨·도움말이 시안 `SETTINGS`의 `label`·`help`와 같다(시안 값을 시험 데이터로 옮긴다).
6. `-DisableAdaptiveUnity` 빌드 성공, CXMR·GMTCK_MR 모듈 경고·오류 0. `CXMR.` 전체 통과.
7. 바이너리 애셋 변경 0.
8. `Docs/Operating-Guide.md`의 컨트롤 창 설명과 `Docs/Headset-Test-Checklist.md` §5를 새 창에 맞게 고친다.

## 로컬에서 확인할 것

- 에디터 PIE(이 PC): 두 창의 모양, 탭, 고급 창 열고 닫기, 모니터 배율.
- 헤드셋 PC(사용자):
  - 앱을 켜고 몇 초 안에 MR이 되는지, 손이 차 앞에 보이는지
  - 마커를 가렸다 보이면 상태가 바뀌는지
  - Save alignment 한 번으로 저장되는지
  - 고급 창에서 Far를 바꾸면 손이 차를 가리는 거리가 바뀌는지

  체크리스트 §1·§2·§5.
- 패키지 빌드에서 Pretendard가 보이는지(사용자, 패키지를 만들 때).

## 개정 이력

- 2026-10-14: 처음 작성 (Claude). 시안 승인 — 사용자
- 2026-10-14: Vehicle 탭에서 디자인 옵션 삭제(시안 3판) — 사용자 결정, main-basic에서 기능도 뺌
- 2026-10-14: 시안 4판 — 시선 점(G)·Foveated(I) 행 삭제(사용자 결정, 10-08에 이미 지워진 기능)
- 2026-10-14: 시안 5판 — 박스 따라가기 기능은 만들지 않기로(사용자: "dynamic 되는지 확인만"). Moving objects 자리를 빼고 탭 이름은 Vehicle 그대로, 고급 창 Diagnostics에 Dynamic 스위치 하나 추가
- 2026-10-15: 1번의 새 프로퍼티는 창에 넣지 않음, "Dynamic tracking for all markers"를 끌 때 Marker Follower 마커는 그 액터의 모드로 — Codex가 멈추지 않게 미리 확정
