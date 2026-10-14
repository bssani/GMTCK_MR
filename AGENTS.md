# GMTCK_MR — CXMR 혼합현실 차량 평가 템플릿 (UE 5.7 + Varjo XR-4)

Varjo XR-4 MR로 실물(clay 모형·의자·박스) 위에 가상 차량을 겹쳐 보는 차량 평가 템플릿이다. 핵심은 이식할 수 있는 플러그인 `Plugins/CXMR/`이고, 프로그램마다 이 프로젝트를 복제해 `/Game/` 데이터만 바꿔 쓴다.

작업 전에 읽을 것:

- [Docs/Operating-Guide.md](Docs/Operating-Guide.md) — 지금 있는 기능과 조작, 애셋 경로 규칙
- [Docs/Varjo-Capabilities.md](Docs/Varjo-Capabilities.md) — Varjo가 실제로 지원하는 것(사실관계 원본)
- [Docs/Headset-Test-Checklist.md](Docs/Headset-Test-Checklist.md) — 헤드셋 시험 순서, 정상인데 안 되는 것처럼 보이는 경우
- 맡은 명세 `Docs/Specs/<명세>.md`와 그 리뷰 `Docs/Reviews/<명세>.md`

이 파일은 Codex와 Claude Code가 함께 읽는 규칙의 원본이다. `CLAUDE.md`는 이 파일을 가져오고 Claude 전용 내용만 덧붙인다. 규칙을 바꿀 때는 이 파일을 고친다. 명령·순서 같은 절차는 [Docs/Agent-Workflow.md](Docs/Agent-Workflow.md)에 있다.

## 작업 방식 — 명세·리뷰는 Claude, 구현은 Codex (2026-10-14 사용자 결정)

- 작업 단위는 [Docs/Specs/](Docs/Specs/)의 명세 파일 하나다. 명세의 범위와 완료 조건만 구현한다. 명세에 없는 기능·리팩터링·"나중에 쓸" 추상화를 더하지 않는다.
- 명세가 이 파일·`Docs/`·플러그인 소스와 어긋나면 임의로 해석하지 말고 멈춰서 보고한다. 재량은 명세의 "구현자가 정할 것"에 적힌 항목뿐이다.
- 브랜치는 `codex/<명세 파일 이름>`, 베이스는 명세에 적힌 브랜치다. 템플릿 작업은 `main`, 특정 프로그램 작업은 `project/<이름>`이다.
- 작업을 끝낼 때 보고한다: ① 바뀐 파일 ② 명세의 완료 조건별 충족 여부 ③ 명세와 다르게 한 것과 이유 ④ 쓴 Varjo·엔진 API를 소스로 확인했는지(경로:줄) 추정인지 ⑤ "로컬에서 확인할 것" — 빌드로 확인할 부분, 에디터에서 할 일(Blueprint·애셋·레벨), 헤드셋으로 시험할 항목([Headset-Test-Checklist](Docs/Headset-Test-Checklist.md)의 절 번호로).
- 같은 보고를 그 작업 마지막 커밋 메시지의 본문에도 쓴다. 리뷰어가 `git log`로 읽는다.
- 리뷰 지적을 반영할 때는 같은 브랜치에 새 커밋을 쌓는다. amend·rebase로 리뷰한 커밋을 고쳐 쓰지 않는다.
- 리뷰는 Claude가 `<베이스>...<브랜치>` diff를 이 파일·`Docs/`·명세 기준으로 한다. 리뷰 통과와 로컬 빌드·시험 재현 전에는 머지하지 않는다.
- **Codex는 push·머지하지 않는다.** Claude가 리뷰 파일을 커밋할 때 브랜치를 push한다. 머지는 사용자 승인 뒤 Claude가 하고, 머지하면 push하고 작업 브랜치를 지운다. `project/*` 브랜치는 지우지 않는다.
- **주고받는 내용은 대화가 아니라 파일로 넘긴다.** 사용자가 두 에이전트 사이에서 내용을 옮기지 않게 하기 위해서다.
  - 리뷰 결과는 [Docs/Reviews/](Docs/Reviews/)`<명세 이름>.md`에 회차별로 남는다. 항목마다 상태가 `열림` 또는 `반영함 (<커밋>)`이다.
  - 명세를 고쳐야 하는 지적은 Claude가 명세 파일을 직접 고친다(개정 이력에 남는다).
  - **리뷰 항목 형식:** 항목마다 세 칸을 둔다. **문제**(위치 파일:줄·발생 조건·영향)는 반드시 해결한다. **제안**(접근 방법, 쓸 API의 소스 경로:줄, 주의점)은 권장이다 — Codex가 다르게 하면 보고 ③에 이유를 적는다. **확인할 시험**은 새로 넣거나 고칠 자동화 시험이다. 정확한 형태가 중요할 때만 짧은 **스케치**(참고용 의사코드)를 붙인다. Claude는 완성 코드를 주지 않는다 — 구현자가 그대로 붙이면 독립 확인이 사라진다.
  - Codex는 작업을 시작할 때 자기 명세의 리뷰 파일을 읽고 `열림` 항목을 반영한다. 반영하면 그 항목의 상태를 `반영함 (<커밋 제목>)`으로 바꾸고 같은 커밋에 넣는다(자기 해시는 같은 커밋 안에 넣을 수 없다. Claude가 다음 리뷰 때 해시로 바꾼다). 질문은 리뷰 파일의 "질문" 절에 쓰고 멈춘다.
  - **구현은 Codex 데스크톱 앱에서 한다.** 사용자가 진행을 앱에서 직접 본다. Claude는 `codex exec`로 구현을 시작하지 않는다.
  - **진입점은 리포 루트의 `HANDOFF.md`다(git에 올리지 않는다).** Claude가 지금 할 일·브랜치·커밋·명세·리뷰 파일·확인 상태를 적어 둔다. Codex는 작업을 시작할 때 이 파일을 먼저 읽고, 거기 적힌 명세와 리뷰 파일을 직접 읽는다. "지금 할 일"이 없으면 아무것도 하지 않고 그렇게 답한다.
  - Codex는 작업을 끝내면 `HANDOFF.md`의 "Codex 결과" 절을 채운다: 완료 또는 중단, 커밋, 리뷰 항목별 상태, 질문 유무, 빌드·시험 결과 요약. 자세한 내용은 커밋 본문과 리뷰 파일에 있다. 그리고 앱 채팅에 "끝났다"고 짧게 알린다. 사용자가 Claude에 리뷰를 요청한다.
- **일을 키우지 않는다 — 토큰.** vBoR에서 명세 하나에 약 3,900만 토큰이 든 뒤 정한 규칙을 그대로 쓴다.
  - **하위 에이전트를 만들지 않고, 자체 리뷰 루프(구현 → 리뷰 에이전트 → 수정 반복)를 돌리지 않는다.** 한 대화에서 혼자 구현한다. 리뷰는 Claude가 한다.
  - `~/.agents/skills`의 워크플로 스킬(`brainstorming`, `writing-plans`, `executing-plans`, `subagent-driven-development`, `requesting-code-review`, `dispatching-parallel-agents` 등)보다 이 파일이 우선한다. 명세가 설계·계획을 대신하므로 `Docs/superpowers/`에 새 spec·plan을 만들지 않는다. 그 폴더의 기존 파일은 2026-10-06~08 작업 기록으로 둔다.
  - **빌드와 전체 시험은 작업 끝에 한 번만 한다.** `-DisableAdaptiveUnity` 빌드와 `CXMR.` 전체 자동화 시험이다. 리뷰 반영도 같다. 중간에는 바꾼 부분의 시험만 돌린다. 실패를 확인하려고 일부러 하는 빌드·실행(red 단계)과 반복·스트레스 실행은 명세가 요구할 때만 한다.
  - **가장 단순한 방법으로 명세를 채운다. 막히면 질문한다.** 단순한 방법이 없으면 큰 구조를 새로 만들지 말고 리뷰 파일 "질문"에 적고 멈춘다. Claude가 명세를 고친다.
- 빌드·자동화 시험·애셋 확인 명령과 기록을 어디에 남기는지는 [Docs/Agent-Workflow.md](Docs/Agent-Workflow.md)에 있다.

## 코드 주석·UI 문구

- UI 문구와 에디터 툴팁은 영어로 쓴다.
- 코드 주석은 짧은 한국어로 쓴다. 끝을 `~함`·`~음`이나 명사로 맺는다. 무엇을 하는지보다 **왜** 그런지를 쓴다. 지금 코드의 주석이 기준이다.

```cpp
// 저장 ID와 동작은 유지하고 표시 문구만 바꿈.
// 레벨 기준값은 프리셋 적용 전에 보관함.
// 일시정지·시간 배율에 영향받지 않음.
```

- 익명 네임스페이스 헬퍼는 파일마다 구분되는 이름으로 짓는다. unity 빌드에서 같은 이름이 한 번역 단위로 묶여 충돌한다(2026-09-15 `HasRange`·`IndexTip`).

## 확정 제약 — 바꾸려면 사용자 승인

- **이식성.** `Plugins/CXMR/`의 코드와 `/CXMR/...` 애셋은 프로젝트 모듈과 `/Game/...` 콘텐츠에 의존하지 않는다. 프로그램마다 다른 값(차량·마커 번호·정렬값)은 `/Game/`이나 설정 파일에 둔다. 애셋 경로를 쓸 때는 항상 `/CXMR/...`(플러그인, 모든 프로그램에 영향)인지 `/Game/...`(이 프로젝트)인지 밝힌다.
- **Varjo 사실관계는 소스가 기준이다.** 우선순위: `Plugins/VarjoPlugin/Source/**`(VarjoOpenXR 1.2.7) > [Docs/Varjo-Capabilities.md](Docs/Varjo-Capabilities.md) > Varjo 공식 문서. 공식 지원 매트릭스와 일부 문서는 틀린 것이 확인됐다. 엔진 API는 `C:\Program Files\Epic Games\UE_5.7\Engine\Source\` 헤더로 확인한다.
- **Varjo 런타임 함수는 OpenXR 세션이 running일 때만 부른다.** 세션 전에 부르면 헤드셋 PC에서 플레이 시작 즉시 크래시가 났다(2026-09-15). 이 PC의 PIE로는 재현되지 않는다.
- **MR 알파.** 언리얼 내부 알파는 0이 불투명이다. `PP_MR`의 Opacity는 `(1 − 씬 알파) × 구멍 계수`다. 반대로 넣으면 VR이 반투명, MR이 검정이 된다(2026-09-13~14). 알파를 내보내지 않는 후처리 머티리얼은 `PostProcessInput0`을 읽어야 알파가 보존된다.
- **VR 전용 오브젝트를 MR에서 숨길 때는 main pass만 끈다.** 액터를 숨기면 실시간 스카이라이트가 꺼져 남은 메시가 어두워진다(2026-09-14).
- **헤드셋에서 확인하기 전까지 시험 상태는 `not-tested`다.** 자동화 시험·PIE 통과는 헤드셋 통과가 아니다. 커밋·문서·보고에 헤드셋 검증을 주장하지 않는다. 헤드셋 시험은 사용자가 별도의 헤드셋 PC에서 한다 — 이 PC에는 Varjo 런타임이 없다.
- **실제 차량 데이터를 추측하지 않는다.** CAD 파트 분할, 슬롯·마운트 위치, 트림·CMF 값은 프로퍼티로 두고 근사임을 드러낸다. 메시를 고치지 않는다.
- **플러그인 `Content/` 아래 `.uasset`은 명세가 허락한 것만 바꾼다.** diff에 다른 바이너리가 있으면 의도한 변경인지 보고한다. 사용자 작업일 수 있으니 restore·삭제하지 않는다.

## 브랜치

- `main` = 템플릿. `project/<이름>` = 프로그램별 장기 브랜치(머지해서 지우는 대상이 아니다). 템플릿 수정은 `main`에 넣고 프로젝트 브랜치로 merge해서 받는다.
- **`main`에는 기본 기능만 둔다(2026-10-14 사용자 결정).** 프로그램 전용 기능은 그 `project/<이름>`에만 둔다. 지금 `project/pqdq-usb-access` 전용: USB 포트 표식, USB 손 접촉. 무엇이 `main`에 남는지는 [Docs/Specs/main-basic.md](Docs/Specs/main-basic.md) "목적".
- 짧은 작업 브랜치(`codex/*` 등)는 머지 직후 지운다.

## 빌드·환경

- UE 5.7(엔진 `C:\Program Files\Epic Games\UE_5.7`). 모듈: 프로젝트 `GMTCK_MR`, 플러그인 `CXMR`·`CXMREditor`.
- 빌드: `Engine/Build/BatchFiles/Build.bat GMTCK_MREditor Win64 Development -Project=C:\Git\GMTCK_MR\GMTCK_MR.uproject -WaitMutex`. 작업 끝의 확인 빌드에는 `-DisableAdaptiveUnity`를 붙인다 — 평소 빌드는 git에서 바뀐 파일을 따로 컴파일해 unity 이름 충돌을 숨긴다.
- 자동화 시험은 `CXMR.` 접두어 전체다. 명령은 [Agent-Workflow §2](Docs/Agent-Workflow.md#2-명령).
- **에디터는 `.uasset`을 잠근다.** 빌드·브랜치 전환·머지 전에 Unreal Editor가 닫혀 있는지 본다. 에디터가 켜진 채 머지하다 실패하면 작업 트리가 반쯤 지워진다(2026-07-22).
- **로컬 도구 플러그인 `Plugins/UnrealAgent`, `Plugins/McpAutomationBridge`는 사용자 PC 전용 개발 도구다**(junction, `.gitignore`). 고치지 않고, 커밋하지 않고, CXMR 코드·애셋에서 참조하지 않는다.
- **에디터 MCP**는 에디터 안 UnrealAgent 서버 `http://localhost:55559/mcp`다. Claude는 `.mcp.json`, Codex는 `.codex/config.toml`의 `unreal`로 붙는다(둘 다 이 PC 전용, `.gitignore`). 에디터가 열려 있을 때만 동작한다.
  - Codex는 명세가 에디터 작업(레벨·Blueprint·애셋 값)을 Codex 몫으로 적었을 때만 MCP로 쓴다. 그 밖에는 읽기만 한다. 끝나면 저장하고 에디터를 닫는다.
  - 애셋 삭제·이름 바꾸기, `Config/*.ini` 변경, 명세가 허락하지 않은 기존 레벨·애셋 수정은 하지 않는다. MCP의 Python 실행으로 이 금지를 우회하지 않는다.
  - 시작할 때 사용자 에디터가 이미 열려 있으면 빌드하지 않고 MCP로 쓰지도 않는다. 저장하지 않은 사용자 작업과 부딪힐 수 있다. 그렇게 보고하고 멈춘다.
