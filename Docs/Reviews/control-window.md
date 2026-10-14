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
