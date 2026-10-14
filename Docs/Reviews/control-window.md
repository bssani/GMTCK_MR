# control-window 리뷰·질문

명세: [control-window.md](../Specs/control-window.md)

## 질문 — 2026-10-15 Codex

1·2번은 각각 `bf70fc1`·`8e1b303`으로 구현·검증 완료했다. 3번은 `codex/main-basic` 끝에서 만든 `codex/control-window`에서 명세·시안·등록 소스를 대조했다. 기존 리뷰 파일은 없었다. 시안은 로컬 브라우저로 열어 배치와 Diagnostics 상태를 직접 확인했다. 아래 항목 때문에 **3번 제품 구현 전에 중단**한다(AGENTS.md §작업 방식, 명세 §원본·D).

### Q1 시안에 없는 등록 항목 3개의 처리 — 열림

- **문제:** `CXMRHandDebugComponent.cpp:22`는 `Hands.OffsetForward`, `Hands.OffsetRight`, `Hands.OffsetUp`을 정의하고, `:265-283`은 저장되는 Float 항목으로 등록한다. 하지만 시안 `prototype.html:370-422`의 SETTINGS와 명세 D의 "창에서 빠지는 등록 항목" 목록에 이 셋은 없다. D는 "이 밖에 시안에 없는 등록 항목을 찾으면 질문한다"고 지정한다. 임의로 행을 추가하거나 제외하지 않았다.
- **영향:** 이 항목을 등록 자체에서 지우면 `Snap hand to marker`가 `CXMRHandDebugComponent.cpp:399-401`의 `SetTunableValue(HandOffsetIds[...])`로 보정·저장하는 기존 경로도 깨진다. 따라서 단순 삭제로 시안과 맞출 수 없다.
- **권장:** 세 ID의 등록·저장·Snap 동작은 그대로 유지하고, 창에서만 제외하도록 명세 D의 제외 목록에 추가한다. 바뀐 수와 탭 reset에서도 제외하도록 명시한다. 수동 보정 행도 필요하다면 Claude가 먼저 승인 시안 SETTINGS를 수정한다.
- **확인할 시험:** Snap이 세 ID의 보정값을 갱신·저장하는지, 고급 창 행 목록에 없는지, 손 보정 저장값만 바뀌어도 표시되는 고급 설정 변경 수는 늘지 않는지(제외 정책을 선택할 경우).

### Q2 변경 수에 저장하지 않는 설정을 넣는지 — 열림

- **문제:** 명세 D(`control-window.md:91`)는 "바뀐 설정"을 **고급 창의 저장되는 설정 중 Default와 다른 것**으로 정의한다. 반면 시안 `prototype.html:420,441,637,652`는 readout/action 외 항목 전부를 세므로, `:416`의 저장하지 않는 `dynamicAll`도 변경 수·Diagnostics ●·탭 reset 활성화에 들어간다. 같은 시안의 도움말과 명세는 Dynamic을 "Not saved"로 명시한다.
- **브라우저 확인:** 기본 상태에서 Diagnostics의 `Dynamic tracking for all markers` 하나만 켰다. 기본 창 하단이 `1 advanced setting changed from defaults`로 바뀌고, `Diagnostics ●`, `Changed · default off`, 활성화된 `Reset Diagnostics to defaults`가 표시됐다. 서버에서 연 원본 시안만 조작했으며 제품 설정은 건드리지 않았다.
- **권장:** 명세의 저장되는 설정 기준을 유지한다면, 시안 changedCount·탭 ●·tabChanged도 같은 기준으로 고친다. Dynamic은 켜짐 표시만 하고 저장 설정 변경 수에는 넣지 않는다. 시안의 현재 계산이 의도라면 명세의 기준을 "저장 여부와 무관한 고급 창 설정"으로 고치고, Dynamic을 포함해 탭 reset이 되돌릴 항목 범위도 명시한다. 등록된 다른 세션용 토글에도 같은 기준이 적용돼야 한다.
- **확인할 시험:** 저장 Float/Choice/Bool 변경, Dynamic 같은 비저장 Bool 변경, readout/action 실행 각각에서 전체 변경 수·탭 ●·행 Changed·탭 reset 대상이 확정한 정책과 일치하는지.

## Codex 중단 기록

- 브랜치 `codex/control-window`, 베이스 `codex/main-basic`의 `8e1b303`.
- 3번 제품 C++·TTF·라이선스·애셋·Config 변경 0. 빌드·자동화·에디터 MCP 미실행. 시안 HTML도 변경하지 않음.
- 3번 완료 조건 1~8 미충족. 1·2번 검증 결과는 각 커밋 본문과 HANDOFF에 남김.
- Claude가 Q1·Q2를 명세/시안에 확정하고 HANDOFF를 갱신하면 3번을 재개한다. push·베이스 머지 없음.
