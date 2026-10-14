# 에이전트 작업 방식과 기록 체계

이 문서는 Codex·Claude·사용자가 이 리포에서 **어떻게 일하고, 알게 된 것을 어디에 모으는지** 적은 것이다. 지켜야 할 규칙은 [AGENTS.md](../AGENTS.md)에 있고, 이 문서는 그 규칙을 수행하는 절차와 명령을 담는다.

같은 방식을 먼저 쓴 리포가 vBoR(`C:\Git\vBoR`)이다. 그쪽 `Docs/Agent-Workflow.md`에 이 방식이 생긴 이유와 시행착오(Codex 앱 연동 확인, `codex exec`를 쓰지 않게 된 경위, 토큰 사용)가 있다.

작성 2026-10-14 (Claude). 절차가 바뀌면 이 문서를 고친다.

---

## 1. 역할과 흐름

| 누가 | 하는 일 | 못 하는 일 |
|---|---|---|
| **Claude** (Claude Code, 로컬) | 작업 명세, 코드 리뷰, 조사, 문서·리포 설정, 빌드·자동화 시험 재현, 머지·push(사용자 승인 뒤) | 요청이 없으면 제품 C++·애셋을 고치지 않는다 |
| **Codex** (데스크톱 앱, 로컬 Windows) | 명세대로 구현, 로컬 빌드, 자동화 시험, 커밋, 명세가 맡긴 에디터 작업 | 명세 밖 기능 추가, push·머지 |
| **사용자** | 결정, 헤드셋 PC에서 시험, 실물 배치(마커·박스·조명), 머지 승인 | — |

두 에이전트는 **git과 파일로 연결된다.** 진입점은 리포 루트의 `HANDOFF.md`(git 무시)이고, 오래 남을 내용은 명세·리뷰 파일에 있다.

1. Claude가 `Docs/Specs/<명세>.md`를 쓰고 베이스 브랜치(`main` 또는 `project/<이름>`)에 넣는다.
2. Claude가 `HANDOFF.md`의 "지금 할 일"을 적는다. 사용자가 Codex 앱에 시작 문구를 보낸다.
3. Codex가 베이스에서 `codex/<명세>` 브랜치를 만들어 구현하고 커밋한다. 보고 ①~⑤는 커밋 본문에 쓴다. `HANDOFF.md`의 "Codex 결과"를 채운다.
4. 사용자가 Claude에 리뷰 문구를 보낸다. Claude가 `git diff <베이스>...codex/<명세>`와 커밋 본문을 읽고, 빌드와 시험을 직접 다시 돌려 재현한다. **결과는 `Docs/Reviews/<명세>.md`에 회차로 남기고**, 명세를 고쳐야 하는 지적은 명세 파일을 직접 고친다. 둘 다 그 브랜치에 커밋하고 push한다.
5. 열린 항목이 있으면 Claude가 `HANDOFF.md`를 갱신하고 2로 돌아간다. Codex는 같은 브랜치에 **새 커밋**으로 반영한다.
6. 리뷰 통과 뒤 사용자 승인으로 Claude가 베이스에 머지하고 push한 다음 작업 브랜치를 지운다. 템플릿(`main`) 변경을 프로젝트 브랜치가 받아야 하면 이어서 merge·빌드한다(§2.4).
7. 헤드셋 시험은 사용자가 헤드셋 PC에서 한다. 결과(로그·관찰)를 받으면 Claude가 리뷰 파일이나 다음 명세에 기록한다.

주의:

- **작업 폴더 하나를 둘이 같이 쓴다.** 한 번에 한쪽만 작업한다. Codex가 브랜치를 바꿔 두므로 작업 전에 `git branch --show-current`와 `git status`를 본다.
- **명세가 모호하면 구현자는 멈추고 묻는다.**
- **사용자는 내용을 옮기지 않는다.** 명세·리뷰·질문은 모두 파일에 있다. 사용자가 보내는 것은 짧은 문구 두 개뿐이다.
- **역할을 바꿔 쓸 때가 있다.** 사용자 요청으로 Claude가 코드를 쓰면 리뷰는 Codex에 맡긴다.

사용자가 보내는 문구:

- Codex 앱에: `HANDOFF.md 읽고 '지금 할 일' 진행해.`
- Claude에: `Codex 끝났어. 리뷰해.`

### 1.1 이 리포의 이전 방식

| 기간 | 방식 | 남은 기록 |
|---|---|---|
| 2026-07 ~ 2026-09-21 | Claude 혼자 기획·구현·빌드·에디터 조작·커밋 | 커밋(Claude 공동 저자), `Docs/Operating-Guide.md` 등 |
| 2026-10-06 ~ 2026-10-08 | Codex 혼자 superpowers 흐름(설계 → 계획 → 구현 → 자체 검증) | `Docs/superpowers/`, `Docs/Plans/`, `Docs/Design-Review-Validation.md` |
| 2026-10-14 ~ | 이 문서의 방식 | `Docs/Specs/`, `Docs/Reviews/` |

---

## 2. 명령

### 2.1 시작 전

- 규칙 파일(AGENTS.md·CLAUDE.md), 맡은 명세와 리뷰 파일을 읽는다.
- `git branch --show-current`, `git status` — 작업 폴더가 깨끗한지, 누가 브랜치를 바꿔 두었는지.
- Unreal Editor가 켜져 있는지: `tasklist /FI "IMAGENAME eq UnrealEditor.exe"`. **켜져 있으면 빌드·브랜치 전환·머지를 하지 않는다.**

### 2.2 빌드

에디터를 닫고 실행한다.

```
"C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" GMTCK_MREditor Win64 Development -Project="C:\Git\GMTCK_MR\GMTCK_MR.uproject" -WaitMutex -DisableAdaptiveUnity
```

- 통과 기준: `Result: Succeeded`, CXMR·GMTCK_MR 모듈의 `warning`·`error` 0.
- **`-DisableAdaptiveUnity`는 작업 끝의 확인 빌드와 리뷰 재현에 붙인다.** 평소 빌드는 git에서 바뀐 파일을 unity 밖에서 따로 컴파일해 익명 네임스페이스 이름 충돌이 안 보인다. 헤드셋 PC의 첫 빌드는 unity 빌드라 거기서 터진다(2026-09-15).
- 로컬 도구 플러그인(UnrealAgent, McpAutomationBridge)도 함께 빌드된다. 거기서 나온 경고·오류는 따로 적고 소스는 고치지 않는다.

### 2.3 자동화 시험

2026-10-07 Codex가 쓴 명령이다(헤드셋 없이, 화면 밖 렌더링).

```
"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Git\GMTCK_MR\GMTCK_MR.uproject" -unattended -nop4 -nosplash -RenderOffscreen -nohmd -nocef -ddc=InstalledNoZenLocalFallback -SharedDataCachePath=None -ShaderWorkingDir="C:\Git\GMTCK_MR\Intermediate\ShaderWorking" -ExecCmds="Automation RunTests CXMR.;Quit" -TestExit="Automation Test Queue Empty" -abslog="C:\Git\GMTCK_MR\Saved\<이름>.log"
```

- 바꾼 부분만 돌릴 때는 `CXMR.` 대신 `CXMR.Alignment`처럼 좁힌다. 지금 접두어: `CXMR.Alignment`, `CXMR.Panels`, `CXMR.Parts`, `CXMR.Regression`, `CXMR.ReviewFix`, `CXMR.ReviewState`, `CXMR.USB`.
- 결과: 로그의 `Found N automation tests`, 각 `Result={Success}`, `TEST COMPLETE. EXIT CODE: 0`.
- **자동화 시험 통과는 헤드셋 시험 통과가 아니다.** 마커·손 추적·패스스루·깊이는 헤드셋에서만 확인된다.

### 2.4 프로젝트 브랜치에 템플릿 변경 넣기

`main` 변경을 `project/<이름>`에 넣을 때는 작업 폴더를 건드리지 않게 별도 worktree에서 merge·빌드한다.

- worktree 경로가 길면 UBT가 "action paths are longer than 260 characters"로 실패한다. `subst W: <worktree의 상위 폴더>`로 짧게 만든 뒤 `W:\...\GMTCK_MR.uproject`로 빌드하고, 끝나면 `subst W: /D`.
- 빌드는 `-DisableAdaptiveUnity`로 한다. 성공한 뒤에만 push한다.
- 지울 때 `git worktree remove --force`가 긴 경로 때문에 폴더 삭제에 실패할 수 있다. 그때는 폴더를 지운 뒤 `git worktree prune`.

### 2.5 애셋·맵 확인

- `Scripts/VerifyDesignReviewAssets.py` — 번들 애셋과 템플릿 맵(`/Game/Map/L_Main`)을 로드해 Root·Loader 구조를 저장 없이 확인한다(2026-10-08 Codex 작성).
- 애셋 구조를 바꾸는 마이그레이션 스크립트는 명세가 허락할 때만 쓴다. `Scripts/MigrateDesignReviewAssets.py`는 2026-10-08 실행 때 관계없는 번들 애셋까지 다시 저장했다.

---

## 3. 에디터 MCP

에디터 안 UnrealAgent 서버 `http://localhost:55559/mcp`에 HTTP로 붙는다. 에디터가 열려 있을 때만 동작한다.

| 클라이언트 | 이름 | 등록 위치 |
|---|---|---|
| Claude Code | `unreal` | 리포 루트 `.mcp.json`(이 PC 전용, `.gitignore`) |
| Codex | `unreal` | 리포 `.codex/config.toml`(이 PC 전용, `.gitignore`). 2026-10-14 기준 등록만 확인했고 Codex에서 호출해 본 기록은 없다 |

에디터와 빌드가 작업 폴더 하나를 같이 쓰므로 한 작업 안에서 순서를 지킨다: 코드 단계(에디터 닫힘, 빌드·시험) → 에디터 단계(에디터 열림, MCP) → 다시 코드가 필요하면 에디터를 닫고 코드 단계로.

**Claude:** 읽기는 자유롭게 한다(레벨 액터, 애셋 값, 출력 로그, 스크린샷). 리뷰에서는 Codex가 만든 에디터 쪽 결과를 MCP로 읽어 확인한다. 쓰기는 사용자가 그 작업을 요청했을 때만 한다. 에디터가 꺼져 있으면 Claude가 직접 켠다.

**Codex:**

1. 명세의 "만들 것"에 레벨·Blueprint·애셋 값이 Codex 몫으로 적혀 있을 때만 쓴다. 그 밖에는 읽기만 한다.
2. C++ 빌드·시험을 먼저 통과시키고, 에디터를 열어 작업하고, 저장하고, 스크린샷·조회로 확인한 뒤 에디터를 닫는다.
3. 시작할 때 사용자 에디터가 이미 열려 있으면 쓰지 않고 보고하고 멈춘다.
4. 하지 않는 것: 애셋 삭제·이름 바꾸기, `Config/*.ini` 변경, 명세가 허락하지 않은 기존 레벨·애셋 수정, Python 실행으로 이 금지를 우회하는 것.
5. 보고 ①에 바뀐 `.umap`·`.uasset` 경로, ③에 MCP로 한 작업, ⑤에 확인 스크린샷 경로와 사용자가 에디터에서 볼 것을 적는다. 바이너리는 diff 리뷰가 안 되므로 Claude가 MCP로 읽어 확인한다.

### 3.1 에디터 Python 함정 (Claude가 2026-07~09에 실제로 겪은 것)

- **위젯을 옮길 때 지우지 말고 떼어 붙인다.** `clear_children()` 뒤 `compile`·저장하면 부모에서 떨어진 위젯이 영구 삭제된다. `BindWidgetOptional`이면 컴파일 오류도 안 나서 조용히 기능이 사라진다. `get_parent().remove_child(x)` → 새 부모에 add.
- **Python으로 위젯을 지운 뒤에는 컴파일 + 강제 저장**(`save_asset(path, only_if_is_dirty=False)`)까지 한다. 안 하면 변수 GUID가 남아 WBP를 로드할 때마다 `was deleted but still has a GUID` 오류가 찍힌다.
- **Blueprint 클래스 기본값을 Python으로 CDO에 넣으면 PIE 재인스턴싱이 값을 날린다**([Operating-Guide](Operating-Guide.md) 끝). C++ 생성자에서 기본값을 주거나 명세에 사용자 몫으로 적는다.
- IMC 키 매핑은 `default_key_mappings` struct 안의 `mappings`를 꺼내 고친 뒤 struct와 애셋에 **둘 다** 되써야 저장된다.
- 헤드리스 커맨드릿(`-run=pythonscript`)은 애셋 레지스트리 스캔 전에 시작한다. `scan_paths_synchronous` + `wait_for_completion` 없이 `load_asset`하면 전부 `None`이다. 종료 코드는 성공해도 1일 수 있으니 로그에 찍은 고유 태그로 판정한다.
- 이름(enum 멤버, 프로퍼티)은 추측하지 말고 `dir()`로 확인한다. 예: `InputActionValueType.AXIS1D`.
- BP 이벤트 그래프는 Python으로 짤 수 없다. 로직은 C++에 둔다.

---

## 4. 에이전트가 못 하는 것 — 사용자 몫

- **헤드셋 시험.** 헤드셋 PC는 이 PC와 다르다. 이 PC에는 Varjo 런타임이 없어 마커·손 추적·패스스루·깊이를 확인할 수 없다. 순서는 [Headset-Test-Checklist.md](Headset-Test-Checklist.md).
- Varjo Base 설정(손 추적 실험 기능, 추적 환경 평가 등), 실물 배치(마커·박스·검정 천·조명 — [MR-Environment.md](MR-Environment.md)).
- 머지 승인.

---

## 5. 기록 체계

**세션 대화는 남지 않는다.** 결정·실측·근거는 그 자리에서 파일로 남긴다. **Codex와 Claude의 메모리는 공유되지 않는다.** 둘 다 알아야 하는 것은 리포 파일에 쓴다.

| 무엇 | 어디에 |
|---|---|
| 매번 지킬 규칙 | `AGENTS.md` (Codex·Claude 공통 원본, `CLAUDE.md`가 가져온다) |
| 지금 할 일·인계 상태 | `HANDOFF.md` (git 무시) |
| 작업 단위 | `Docs/Specs/<이름>.md` — 목적, 범위·범위 밖, API 근거, 완료 조건, 개정 이력 |
| 리뷰 | `Docs/Reviews/<이름>.md` — 회차별 항목과 상태 |
| 작업 보고 | 커밋 메시지 본문 ①~⑤ |
| 기능과 조작 | `Docs/Operating-Guide.md` |
| Varjo 사실관계 | `Docs/Varjo-Capabilities.md` |
| 헤드셋 시험 순서 | `Docs/Headset-Test-Checklist.md` |
| 일하는 방식 | 이 문서 |

원칙:

- **같은 사실을 두 곳에 쓰지 않는다.** 규칙은 AGENTS.md 하나, Varjo 사실은 Varjo-Capabilities 하나. 한쪽이 바뀌면 다른 쪽이 낡는다.
- **실측은 날짜·환경·기기 상태와 함께 적는다.** 예: "헤드셋 PC, Varjo Base 4.11.1, 마커 3개".
