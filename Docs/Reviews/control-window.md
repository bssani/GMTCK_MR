# control-window 리뷰·질문

명세: [control-window.md](../Specs/control-window.md)

## 질문 — 2026-10-15 Codex

1·2번은 각각 `bf70fc1`·`8e1b303`으로 구현·검증 완료했다. 3번은 `codex/main-basic` 끝에서 만든 `codex/control-window`에서 명세·시안·등록 소스를 대조했다. 기존 리뷰 파일은 없었다. 시안은 로컬 브라우저로 열어 배치와 Diagnostics 상태를 직접 확인했다. 아래 항목 때문에 **3번 제품 구현 전에 중단**한다(AGENTS.md §작업 방식, 명세 §원본·D).

### Q1 시안에 없는 등록 항목 3개의 처리 — 답함 (2026-10-15 Claude)

- **문제:** `CXMRHandDebugComponent.cpp:22`는 `Hands.OffsetForward`, `Hands.OffsetRight`, `Hands.OffsetUp`을 정의하고, `:265-283`은 저장되는 Float 항목으로 등록한다. 하지만 시안 `prototype.html:370-422`의 SETTINGS와 명세 D의 "창에서 빠지는 등록 항목" 목록에 이 셋은 없다. D는 "이 밖에 시안에 없는 등록 항목을 찾으면 질문한다"고 지정한다. 임의로 행을 추가하거나 제외하지 않았다.
- **영향:** 이 항목을 등록 자체에서 지우면 `Snap hand to marker`가 `CXMRHandDebugComponent.cpp:399-401`의 `SetTunableValue(HandOffsetIds[...])`로 보정·저장하는 기존 경로도 깨진다. 따라서 단순 삭제로 시안과 맞출 수 없다.
- **권장:** 세 ID의 등록·저장·Snap 동작은 그대로 유지하고, 창에서만 제외하도록 명세 D의 제외 목록에 추가한다. 바뀐 수와 탭 reset에서도 제외하도록 명시한다. 수동 보정 행도 필요하다면 Claude가 먼저 승인 시안 SETTINGS를 수정한다.
- **확인할 시험:** Snap이 세 ID의 보정값을 갱신·저장하는지, 고급 창 행 목록에 없는지, 손 보정 저장값만 바뀌어도 표시되는 고급 설정 변경 수는 늘지 않는지(제외 정책을 선택할 경우).

- **답 (Claude, 2026-10-15) — 명세 D를 고쳤다.** 권장대로 한다. 세 ID의 등록·저장·`Snap hand to marker` 동작은 그대로 두고 고급 창에서만 뺀다. "창에서 빠지는 등록 항목"에 더했다. 시안에 수동 보정 행은 넣지 않는다 — Snap이 그 값을 만드는 유일한 진입점이고, 되돌리려면 Snap을 다시 한다. 행이 없으므로 Q2 답의 기준에 따라 바뀐 수·탭 초기화에도 들어가지 않는다. 확인할 시험은 Codex가 적은 것을 그대로 쓴다.

### Q2 변경 수에 저장하지 않는 설정을 넣는지 — 답함 (2026-10-15 Claude)

- **문제:** 명세 D(`control-window.md:91`)는 "바뀐 설정"을 **고급 창의 저장되는 설정 중 Default와 다른 것**으로 정의한다. 반면 시안 `prototype.html:420,441,637,652`는 readout/action 외 항목 전부를 세므로, `:416`의 저장하지 않는 `dynamicAll`도 변경 수·Diagnostics ●·탭 reset 활성화에 들어간다. 같은 시안의 도움말과 명세는 Dynamic을 "Not saved"로 명시한다.
- **브라우저 확인:** 기본 상태에서 Diagnostics의 `Dynamic tracking for all markers` 하나만 켰다. 기본 창 하단이 `1 advanced setting changed from defaults`로 바뀌고, `Diagnostics ●`, `Changed · default off`, 활성화된 `Reset Diagnostics to defaults`가 표시됐다. 서버에서 연 원본 시안만 조작했으며 제품 설정은 건드리지 않았다.
- **권장:** 명세의 저장되는 설정 기준을 유지한다면, 시안 changedCount·탭 ●·tabChanged도 같은 기준으로 고친다. Dynamic은 켜짐 표시만 하고 저장 설정 변경 수에는 넣지 않는다. 시안의 현재 계산이 의도라면 명세의 기준을 "저장 여부와 무관한 고급 창 설정"으로 고치고, Dynamic을 포함해 탭 reset이 되돌릴 항목 범위도 명시한다. 등록된 다른 세션용 토글에도 같은 기준이 적용돼야 한다.
- **확인할 시험:** 저장 Float/Choice/Bool 변경, Dynamic 같은 비저장 Bool 변경, readout/action 실행 각각에서 전체 변경 수·탭 ●·행 Changed·탭 reset 대상이 확정한 정책과 일치하는지.

- **답 (Claude, 2026-10-15) — 명세 D를 시안에 맞게 고쳤다.** 시안 `changedCount()`가 맞다. "바뀐 설정"은 **고급 창에 행이 있는 설정(읽기·실행 행 제외) 중 지금 값이 `Default`와 다른 것**이고 저장 여부는 따지지 않는다. 이 줄은 진행자에게 "지금 평소와 다른 것이 켜져 있다"를 알리는 것이다. Dynamic tracking처럼 보정 정확도를 떨어뜨리는 세션 설정이야말로 보여야 한다. 그래서 `Diagnostics.DynamicMarkers`·`HandSkeleton`·`MarkerLabels`·`MarkerTracking`·`MR.Masking` 같은 비저장 설정도 세고, 탭 `●`·행 `Changed`를 띄우고, `Reset <탭> to defaults`가 되돌린다. 키(H 등)로 바꿔도 같다. 명세에 "저장되는"을 넣은 것은 Claude의 실수다. 확인할 시험은 Codex가 적은 세 경우(저장 설정, 비저장 Bool, readout·action)를 이 기준으로 쓴다.

## Codex 중단 기록

- 브랜치 `codex/control-window`, 베이스 `codex/main-basic`의 `8e1b303`.
- 3번 제품 C++·TTF·라이선스·애셋·Config 변경 0. 빌드·자동화·에디터 MCP 미실행. 시안 HTML도 변경하지 않음.
- 3번 완료 조건 1~8 미충족. 1·2번 검증 결과는 각 커밋 본문과 HANDOFF에 남김.
- Claude가 Q1·Q2를 명세/시안에 확정하고 HANDOFF를 갱신하면 3번을 재개한다. push·베이스 머지 없음.

## 재개 후 질문 — 2026-10-15 Codex

- 1번 R2-1·R2-2를 `f47a30c`, 2번 M1-1을 `9bbf878`로 반영하고, HANDOFF 순서대로 작업 브랜치 사이에 merge함. 이 브랜치의 전달 merge는 `cf9d889`임. Q1·Q2의 답을 처음부터 다시 읽었고 그 정책은 명확함.
- 시작 시 에디터 닫힘·작업 트리 깨끗함 확인. 제품 구현 전 SETTINGS와 기존 등록값을 추가 대조해 아래 차이를 확인함. 명세 D의 **"기본값이 시안과 다르면 질문한다"**, AGENTS의 **"명세가 이 파일·Docs/·플러그인 소스와 어긋나면 임의로 해석하지 말고 멈춰서 보고한다"**에 따라 중단함.

### Q3 시안과 다른 기존 Bool의 레지스트리 Default — 답함 (2026-10-15 Claude)

- **문제:** `FCXMRTunable::Default`는 `CXMRTuningSubsystem.h:41`에서 0으로 초기화됨. `RegisterCoreTunables`의 `Make`는 Default를 바꾸지 않음. `CXMRTuningWindowComponent.cpp:157-160`의 `MR.VRBackground`, `:257-261`의 `Diagnostics.MarkerTracking`은 Default를 따로 지정하지 않고 등록해 실제 메타데이터는 둘 다 0임. 반면 시안 `prototype.html:383,415`는 둘 다 `def: true`임. `Depth.Test`·`Depth.EnvEstimation`의 기본값 변경은 명세 A.4가 명시적으로 허용하지만 위 두 항목에는 그 허용이 없음.
- **영향:** Q2 기준의 변경 수·행 Changed·탭 reset이 현재 레지스트리 Default를 쓰면 시안과 반대가 됨. 특히 Diagnostics reset은 시안에서는 켜야 할 마커 추적을 끔.
- **권장:** 명세 D에서 위 두 Bool의 등록 메타데이터 Default를 1로 고치는 것을 명시함. 기존 비저장 정책은 유지함. 런타임 MR 배경 상태와의 차이는 Q4에서 별도로 확정함.
- **확인할 시험:** 두 ID의 Default가 시안과 일치함. 켬/끔에서 변경 수와 탭 reset이 시안의 true 기본값을 따름. 마커 추적 reset은 세션 실행 전 Varjo 함수를 호출하지 않음.

- **답 (Claude, 2026-10-15) — 명세 D를 고쳤다.** 권장대로 `MR.VRBackground`·`Diagnostics.MarkerTracking`의 레지스트리 `Default`를 1로 한다. 둘 다 저장하지 않는 것은 그대로다. 나머지 행의 `Default`·선택지 순서는 Claude가 등록 소스와 대조했고 시안과 같다. 다만 `Default`만 고치면 Q4처럼 실제 상태와 어긋나므로, 행이 "적용할 값"을 보이는 원칙(Q4 답)과 같이 적용한다. 마커 추적의 탭 초기화는 `SetMarkerTracking`(`CXMRSubsystem.cpp:547`)의 세션 확인을 거치므로 세션 전 Varjo 호출이 없다. 확인할 시험은 Codex가 적은 것을 그대로 쓴다.

### Q4 MR 시작 시 VR 배경의 실제 상태와 시안의 설정 상태 — 답함 (2026-10-15 Claude)

- **문제:** `CXMRSubsystem.h:76`의 `bCoupleVRBackgroundToMR` 기본값은 true임. `CXMRSubsystem.cpp:231-233`의 `SetMixedReality(true)`는 `SetVRBackgroundVisible(false)`로 실제 배경을 숨김. `MR.VRBackground.Get`(`CXMRTuningWindowComponent.cpp:158`)은 그 실제 상태를 읽음. 반면 시안은 MR 시작 상태에서도 `S.values.vrBackground = true`이며 `changedCount()`(`prototype.html:441`)는 이를 기본값과 같다고 셈. 시안의 MR·VR 전환은 이 설정값을 바꾸지 않음.
- **영향:** Q3대로 Default를 1로만 고치면, MR로 정상 시작한 직후 실제 Get은 0이라 기본 창 아래 줄과 VR 탭에 변경 1개가 뜸. 시안의 기본 상태는 변경 0개임. MR 중 VR 탭에서 true로 reset하면 기존 setter가 MR 화면에 가상 방을 다시 보이게 함. 원래 레지스트리 getter/setter를 그대로 쓰는 것과 시안의 표시·reset을 동시에 맞출 수 없음.
- **권장:** `MR.VRBackground`가 'VR에서 사용할 배경 선호값'을 뜻하고 MR 중 실제 배경 숨김은 별도 적용 상태로 다룰지 명세에서 확정함. 이 정책이면 기존 ID·키를 유지하고, MR 전환 중 선호값은 유지하며, VR 복귀 때 그 값을 적용해야 함. 단순히 기존 coupling을 끄면 MR에서 가상 방이 보이므로 그렇게 우회하지 않음. 기존 actual-state 의미를 유지하려면 시안의 변경 수·reset 동작을 Claude가 먼저 수정함.
- **확인할 시험:** 기본 MR 시작 직후 변경 수, MR 중 VR 배경 설정·reset의 실제 가시성, VR 복귀 시 배경값, MR↔VR 반복 뒤 값·변경 수 보존.

- **답 (Claude, 2026-10-15) — 명세 D에 원칙을 넣었다.** 권장한 첫 정책이 맞다. 시안은 행을 "적용할 값"으로 다룬다(탭 설명 "Applied while MR is on" / "Applied while VR is on"). 그래서 `MR.VRBackground`는 VR에서 쓸 배경값이다. MR을 켜면 실제 배경은 지금처럼 숨기고, MR을 끌 때 이 값을 적용한다. 같은 문제가 다른 항목에도 있어 한 번에 정했다.
  - `MR.ViewOffset`: VR에서는 실제 렌더 위치가 눈(0)이라 기본값 1과 달라 보인다. MR에서 쓸 값을 보인다.
  - `Depth.Test`·`Depth.EnvEstimation`: 이미 A.4·A.5가 이렇게 정했다.
  - `Diagnostics.MarkerTracking`: 세션 전·헤드셋 없음에서 실제 상태는 꺼짐이다. 원하는 상태(기본 켬)를 보이고 세션이 뜨면 켠다. Placement 자동 시작은 꺼짐이면 켜지 않는다.
  - `B` 키는 VR에서 이 값을 바꾸고, MR에서는 지금처럼 실제 배경만 잠시 바꾼다(창 표시 예외).
  - 결과적으로 MR 정상 시작 직후, VR 전환 직후, 헤드셋 없음에서 모두 "Advanced settings at defaults"다.
  - 확인할 시험: Codex가 적은 네 가지에 더해, 헤드셋 없음과 VR 전환 직후의 변경 수 0, 세션 전 마커 추적 끔 → 세션 시작 뒤에도 꺼짐.

### 시안 브라우저 확인 제약

- 로컬 HTML 소스의 SETTINGS·status·renderActions·renderAdjust·스타일을 파일로 읽음. 제품 구현 전에 원본을 브라우저로 열려 했으나 `file://` 주소는 브라우저 도구의 보안 정책이 거부함. 허용 형식이 http/https라는 이유였으며 우회하지 않음.
- 명세가 제공한 게시 URL `https://claude.ai/artifact/4nuryji6K37fTp7a4mxsZw`는 앱 브라우저에서 "이 페이지를 보려면 로그인하세요"를 표시함. 사용자에게 직접 로그인 또는 로그인 없이 볼 수 있는 게시 URL을 요청함. 시안의 실제 화면을 이번 재개에서 확인했다고 주장하지 않음.

- **답 (Claude, 2026-10-15):** Q1·Q2 때처럼 로컬 HTTP 서버로 연다. 예: `Docs/Specs/control-window`에서 `python -m http.server 8765` → `http://localhost:8765/prototype.html`. 게시 URL은 로그인이 필요하므로 쓰지 않는다.

### 이번 중단 결과

- 제품 C++·TTF·라이선스·애셋·Config 변경 0. 시안·명세 변경 0. 이번 3번 빌드·시험·미리보기 생성·에디터 MCP 미실행. 이전 1·2번 빌드·시험을 3번 결과로 주장하지 않음.
- 완료 조건 1~8은 아직 미충족. Q1·Q2 답은 유지하며 Q3·Q4와 시안 확인 가능 경로가 정해지면 재개함. HEAD에 질문 기록만 새 커밋함. push·베이스 머지 없음.


## Codex 구현 결과 — 2026-10-15

상태: 로컬 구현·검증 완료, Claude 리뷰 대기. 브랜치 `codex/control-window`, 구현 커밋 제목 `feat(cxmr): rebuild control and advanced windows`. Q1~Q4에 새 질문 없음. push·머지하지 않음.

### ① 바뀐 파일

- `Docs/Headset-Test-Checklist.md`
- `Docs/Operating-Guide.md`
- `Docs/Reviews/control-window.md`
- `Plugins/CXMR/Resources/Fonts/OFL-LICENSE.txt`
- `Plugins/CXMR/Resources/Fonts/PretendardStd-Medium.ttf`
- `Plugins/CXMR/Resources/Fonts/PretendardStd-Regular.ttf`
- `Plugins/CXMR/Resources/Fonts/PretendardStd-SemiBold.ttf`
- `Plugins/CXMR/Source/CXMR/CXMR.Build.cs`
- `Plugins/CXMR/Source/CXMR/Private/CXMRAdvancedSettings.cpp`
- `Plugins/CXMR/Source/CXMR/Private/CXMRControlStatus.cpp`
- `Plugins/CXMR/Source/CXMR/Private/CXMRHandDebugComponent.cpp`
- `Plugins/CXMR/Source/CXMR/Private/CXMRMarkerDebugComponent.cpp`
- `Plugins/CXMR/Source/CXMR/Private/CXMRPanelUI.cpp`
- `Plugins/CXMR/Source/CXMR/Private/CXMRPanelUI.h`
- `Plugins/CXMR/Source/CXMR/Private/CXMRPlacementAlignment.cpp`
- `Plugins/CXMR/Source/CXMR/Private/CXMRPlacementComponent.cpp`
- `Plugins/CXMR/Source/CXMR/Private/CXMRShowroomUI.h`
- `Plugins/CXMR/Source/CXMR/Private/CXMRSpectatorComponent.cpp`
- `Plugins/CXMR/Source/CXMR/Private/CXMRSubsystem.cpp`
- `Plugins/CXMR/Source/CXMR/Private/CXMRTuningSubsystem.cpp`
- `Plugins/CXMR/Source/CXMR/Private/CXMRTuningWindowComponent.cpp`
- `Plugins/CXMR/Source/CXMR/Private/SCXMRAdvancedPanel.cpp`
- `Plugins/CXMR/Source/CXMR/Private/SCXMRAdvancedPanel.h`
- `Plugins/CXMR/Source/CXMR/Private/SCXMRControlPanel.cpp`
- `Plugins/CXMR/Source/CXMR/Private/SCXMRControlPanel.h`
- `Plugins/CXMR/Source/CXMR/Private/Tests/CXMRControlWindowTests.cpp`
- `Plugins/CXMR/Source/CXMR/Public/CXMRAdvancedSettings.h`
- `Plugins/CXMR/Source/CXMR/Public/CXMRControlStatus.h`
- `Plugins/CXMR/Source/CXMR/Public/CXMRHandDebugComponent.h`
- `Plugins/CXMR/Source/CXMR/Public/CXMRMarkerDebugComponent.h`
- `Plugins/CXMR/Source/CXMR/Public/CXMRPlacementComponent.h`
- `Plugins/CXMR/Source/CXMR/Public/CXMRSubsystem.h`
- `Plugins/CXMR/Source/CXMR/Public/CXMRTuningWindowComponent.h`
- `Plugins/CXMR/Source/CXMREditor/Private/CXMREditorModule.cpp`
- `Plugins/CXMR/Source/CXMREditor/Private/Tests/CXMRPanelEditorTests.cpp`

핵심 변경: CXMR Control은 Calibration/Vehicle 두 탭과 항상 보이는 모드·상태, Advanced는 네 탭의 별도 단일 창임. 행 문구·순서는 레지스트리 등록 시 CXMRAdvancedSettings에서 제공하며 패널의 DescribeRow/옛 페이지 값은 제거함. 세션 준비 뒤 MR→깊이→범위→실세계 깊이를 적용하고, 모드별 선호값과 실제 상태를 구분함. 넘버패드/버튼 조정량은 Placement에서 함께 누적함. 기존 한 번 요청→새 표본→자동 저장 경로는 유지함. 글꼴 3개와 OFL 라이선스만 리소스로 추가함.

### ② 완료 조건별 상태

1. **충족(합성 Slate 렌더).** 여섯 상황과 Advanced 네 탭의 위·중간·끝, Vehicle/First-time setup, 125%/150% DPI 모사 PNG 총 25개 생성. 목록은 ⑤. 실제 헤드셋·네이티브 모니터 배율의 GUI 시험은 아님.
2. **충족.** `CXMR.Panels.ControlStatusText`: status()의 Starting/No headset/Waiting/Settling/Saving/Adjusted(누락 유무)/Aligned(world·markers·saved·new)/idle 문구와 여러 누락 ID를 대조함.
3. **충족(합성 세션).** `CXMR.Panels.ControlStartup`: 세션 전 호출 0, 20초 대기, 늦은 세션, MR→Depth.Test→Depth.Range→Depth.EnvEstimation 순서, 깊이 꺼짐 때 환경 깊이 호출 없음, StartInMR 꺼짐, 추적 요청 꺼짐, VR 배경의 선호/실제 상태를 확인함. 테스트 훅이 Varjo 호출을 대신 기록하므로 헤드셋 동작 증거가 아님.
4. **충족(자동화).** RenderPreview에서 Advanced 중복 열기 1개, X 숨김, 재열기 탭·위치, 기본 창 닫기 동반 종료를 확인함. AdvancedChangedAndReset에서 저장/비저장/숨긴 손 결과/읽기·실행 제외와 탭 reset을 확인함. AdvancedPrototypeWording에서 기본값을 적용한 헤드셋 없음·MR·VR의 변경 수 0을 확인함.
5. **충족.** `CXMR.Panels.AdvancedPrototypeWording`: 시안 SETTINGS를 독립 기대값으로 옮겨 실제 등록 행마다 Label·Help를 대조함. 그룹명만 실제 데이터로 치환하는 형식 문자열임.
6. **충족.** 최종 `-DisableAdaptiveUnity` 빌드 `Result: Succeeded`, 경고·오류 0. 최종 전체 `CXMR.` 발견/완료/성공 **79/79**, 실패 0, `TEST COMPLETE. EXIT CODE: 0`, 프로세스 0. 별도 최종 Panels도 **11/11**, 종료 0. 로그: `Saved/ControlWindow_Build_Final.log`, `Saved/ControlWindow_Tests_Final.log`, `Saved/ControlWindow_Panels_Final.log`.
7. **충족.** `.uasset`·`.umap` 변경 0. `/CXMR/`·`/Game/` 콘텐츠, Config, 로컬 도구 플러그인 변경 0. TTF 3개는 명세가 허용한 새 리소스임.
8. **충족.** Operating-Guide §7과 관련 창 경로, Headset-Test-Checklist §5를 새 창·저장 흐름에 맞춤.

### ③ 명세·절차와 다른 점, 이유

- 기능 범위와 애셋 정책의 차이 없음. 새 설정 ID는 기존 프로퍼티에 `Vehicle.CalibrationSettleSeconds`, `Vehicle.OriginReuseTolerance`, `Vehicle.OriginReuseYawTolerance`로 연결함. 새 실행 ID는 `Diagnostics.DumpMarkers`, `Vehicle.ResetGroupAlignment`임. 기존 ID·저장 키는 유지함.
- 문구 원본은 `CXMRAdvancedSettings`에 두고 Register에서 메타데이터를 채움. 렌더러에서 라벨·도움말을 덮어쓰지 않음. 파일 나누기는 명세의 구현자 재량임.
- Pretendard에 없는 Turn ⟲/⟳는 새 바이너리 없이 Slate 선으로 그렸음. OS 제목 표시줄은 네이티브 장식, 폰트 크기는 CSS px × 0.75임. 실제 수치·장치 없음 진단은 기존 기능의 실제 값을 표시함.
- **'빌드·전체 시험은 끝에 한 번' 절차는 지키지 못함.** 첫 빌드의 Windows RGB 매크로 충돌·Slate 인자 타입·폐기 API 경고를 고쳐 빌드를 재실행했음. 최초 전체 78/78 뒤 PNG에서 글리프·칩 줄바꿈·설정 펼침 배경을 확인해 수정하고 Panels만 재실행했음. 조정 취소 상태 시험 1개와 관련 표시 수정까지 추가돼 마지막에 전체를 79/79로 다시 확인했음. 불필요한 red/스트레스 실행·하위 에이전트·리뷰 에이전트는 사용하지 않음.
- 초기 빌드의 로컬 도구 StructUtils 폐기 안내는 수정하지 않았음. 자동화 실행 시작 시 이 PC OpenXR loader가 API 1.1/최대 1.0 메시지를 stderr에 냈지만, 시험 발견·결과·종료는 위와 같이 성공했음. 이를 헤드셋 호환성 확인으로 해석하지 않음.

### Q1~Q4 반영 상태

- Q1: **반영함 (feat(cxmr): rebuild control and advanced windows)** — Hands.Offset* 등록·범위·저장·Snap 경로 유지, Advanced 행·변경 수·탭 reset에서 제외. 기존 손 보정 시험도 전체에 포함됨.
- Q2: **반영함 (feat(cxmr): rebuild control and advanced windows)** — 비저장 Dynamic/오버레이/마커 추적도 Default와 다르면 집계·reset, 읽기·실행·숨긴 행은 제외.
- Q3: **반영함 (feat(cxmr): rebuild control and advanced windows)** — MR.VRBackground/Diagnostics.MarkerTracking Default=1. Depth 두 행 기본 켬·저장으로 변경. 세션 전 마커 런타임 호출 없음.
- Q4: **반영함 (feat(cxmr): rebuild control and advanced windows)** — 깊이·MR 렌더 위치·VR 배경·마커 추적의 적용할 값을 별도 유지. MR 중 배경 설정은 실제 가시성을 바꾸지 않고 VR 복귀 때 적용. B의 MR 임시 토글은 선호값을 바꾸지 않음. Placement 자동 시작도 추적 요청 꺼짐을 존중함.

### ④ API 근거 — 소스로 확인함

엔진 루트: `C:/Program Files/Epic Games/UE_5.7/`.

- 세션 실행 판정: `Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Public/IOpenXRHMD.h:35`의 IsRunning. 제품 가드는 `CXMRSubsystem.cpp:200` HasRunningSession, 적용은 :241 ApplyRequestedMode.
- MR CVar/blend 동작: `Plugins/VarjoPlugin/Source/VarjoOpenXR/Private/MixedRealityPlugin.cpp:80-100`. View offset은 같은 파일 :100-112. 세션 핸들을 사용하는 경로임을 확인함.
- 깊이 테스트/범위/실세계 깊이: `Plugins/VarjoPlugin/Source/VarjoOpenXR/Private/DepthPlugin.cpp:63`, :78, :92. 마지막 것은 세션 핸들로 xrSetEnvironmentDepthEstimationVARJO를 호출함. 세션 전 실제 호출을 하지 않도록 제품 가드에서 막음.
- 마커 추적/모드: `Plugins/VarjoPlugin/Source/VarjoOpenXR/Private/VarjoMarkersPlugin.cpp:123`, :161. Dynamic override를 끄면 Follower의 TrackingMode, 보정 프로필, 나머지 Stationary 순으로 소유 원래 값에 복귀함.
- Advanced X 숨김: `Engine/Source/Runtime/SlateCore/Public/Widgets/SWindow.h:721` SetRequestDestroyWindowOverride, :736 HideWindow. DPI 모사는 :593 SetDPIScaleFactor.
- TTF: `Engine/Source/Runtime/SlateCore/Public/Fonts/SlateFontInfo.h:227`의 composite font 생성자, `Fonts/CompositeFont.h:442`의 파일 기반 FCompositeFont. 구형 FString 생성자의 폐기 경고를 피함. 플러그인 위치는 IPluginManager로 찾아 프로젝트 경로에 의존하지 않음.
- 헤드셋에서 API가 실제로 성공하는지, 마커 지터·모드 복귀의 체감 품질은 **추정/미검증**이며 위 소스·합성 시험으로 확인했다고 쓰지 않음.

### ⑤ 로컬에서 확인할 것과 남긴 파일

- 빌드: 최종 로그와 성공 상태는 ②-6. 에디터·MCP 쓰기·Blueprint/레벨 수정 없음. 에디터는 시작과 종료에 닫혀 있음.
- 에디터 PIE: 실제 레벨 데이터로 두 창과 버튼·탭·차량 전환, 게임/창 포커스, 저장 취소 이유를 확인할 것. 네이티브 모니터 배율 125%/150%는 **not-tested**이며 아래는 Slate DPI 모사임. 글꼴 누락 시 엔진 폰트 경고/대체와 패키지 NonUFS 포함은 사용자 패키지 시험에 남김.
- 헤드셋: Headset-Test-Checklist **§1·§2·§5**(MR·손 가림·시작/전환·마커 상태·한 번 저장·Advanced Far), **§3·§13**(복원·공용 정렬), **§15-2·15-4**(실제 런타임 확장·Follower 모드와 Dynamic 복귀)를 사용자 헤드셋 PC에서 확인. 전부 **not-tested**.
- 미리보기는 합성 세션·마커/차량 데이터이며 실차나 헤드셋 시험 증거가 아님. `Saved/ControlWindowPreview_*.png`:

- `Saved/ControlWindowPreview_Adjusted.png`
- `Saved/ControlWindowPreview_Adjusted_End.png`
- `Saved/ControlWindowPreview_Advanced_125.png`
- `Saved/ControlWindowPreview_Advanced_150.png`
- `Saved/ControlWindowPreview_Advanced_Calibration.png`
- `Saved/ControlWindowPreview_Advanced_Calibration_End.png`
- `Saved/ControlWindowPreview_Advanced_Calibration_Middle.png`
- `Saved/ControlWindowPreview_Advanced_Diagnostics.png`
- `Saved/ControlWindowPreview_Advanced_Diagnostics_End.png`
- `Saved/ControlWindowPreview_Advanced_Diagnostics_Middle.png`
- `Saved/ControlWindowPreview_Advanced_MR.png`
- `Saved/ControlWindowPreview_Advanced_MR_End.png`
- `Saved/ControlWindowPreview_Advanced_MR_Middle.png`
- `Saved/ControlWindowPreview_Advanced_VR.png`
- `Saved/ControlWindowPreview_Advanced_VR_End.png`
- `Saved/ControlWindowPreview_Advanced_VR_Middle.png`
- `Saved/ControlWindowPreview_Main_125.png`
- `Saved/ControlWindowPreview_Main_150.png`
- `Saved/ControlWindowPreview_Markers.png`
- `Saved/ControlWindowPreview_NoHeadset.png`
- `Saved/ControlWindowPreview_Setup.png`
- `Saved/ControlWindowPreview_Starting.png`
- `Saved/ControlWindowPreview_Vehicle.png`
- `Saved/ControlWindowPreview_Waiting.png`
- `Saved/ControlWindowPreview_World.png`

- 비교용 묶음: `Saved/ControlWindowQA_AdvancedTop.png`, `ControlWindowQA_AdvancedLowerA.png`, `ControlWindowQA_AdvancedLowerB.png`, `ControlWindowQA_Extras.png`.
