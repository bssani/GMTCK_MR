# MR 깊이 테스트를 기본으로 끄기

- 베이스 브랜치: `main`
- 상태: 머지함 (`653d2b7`)

## 목적

사용자 헤드셋 시험(2026-10-15, XR-4 base station 판, Varjo Base Pro 4.16)에서 확인한 것:

- Varjo Base 4.16에 **Occlusion** 기능이 생겼다(Settings > Mixed reality > Mixed reality effects > Occlusion, Pro 전용). 손이 가상 물체 앞에 보이게 한다.
- 앱의 깊이 테스트("Real objects in front of the car")와 실세계 깊이 추정을 켠 채 Varjo Base Occlusion을 켜면 화면이 이상하게 보였다. 두 가지가 같은 일을 겹쳐서 한다.
- 사용자 결정: **앱의 깊이 테스트는 기본으로 끈다.** 손 가림은 Varjo Base Occlusion으로 한다. 필요하면 진행자가 고급 창이나 `T`로 켠다.

지금은 MR이 시작되면 깊이 테스트와 실세계 깊이 추정이 자동으로 켜진다(control-window 명세 D절 "MR 시작 순서").

## 범위

### 1. 기본값

| 항목 | 지금 | 바꿀 값 | 위치 |
|---|---|---|---|
| `Depth.Test` 레지스트리 기본값 | 1 | **0** | `CXMRTuningWindowComponent.cpp:219-220` |
| `UCXMRSubsystem::bRequestedDepthTest` 초기값 | true | **false** | `CXMRSubsystem.h:45` |

`Depth.EnvEstimation`(기본 1), `Depth.Range`, `Depth.NearZ`(0), `Depth.FarZ`(0.75)는 그대로 둔다. 실세계 깊이 추정은 깊이 테스트가 켜질 때만 적용된다(`CXMRSubsystem.cpp:260-269`). 그래서 진행자가 깊이 테스트를 켜면 예전과 같은 화면이 된다.

### 2. 문구 — 시안 [prototype.html](control-window/prototype.html)에 이미 반영함

Claude가 시안을 먼저 고쳤다(이 명세와 같은 커밋). C++ 문구를 시안과 같게 맞춘다.

| 자리 | C++ 위치 | 새 문구 |
|---|---|---|
| 고급 창 `Depth.Test` 도움말 | `CXMRAdvancedSettings.cpp:10` | `Hands and nearby real objects cover the virtual car instead of disappearing behind it. Leave off when Occlusion is on in Varjo Base.` |
| 상태줄 "Starting the headset session" 설명 | `CXMRControlStatus.cpp:13` | `MR switches on as soon as the headset is ready.` |
| 컨트롤 창 모드 설명, MR이고 깊이 테스트가 꺼졌을 때 | `SCXMRControlPanel.cpp:129` | `Depth test is off: real hands appear behind the virtual car unless Occlusion is on in Varjo Base.` |

라벨(`Real objects in front of the car`)과 다른 행은 바꾸지 않는다.

### 3. 문서

- `Docs/Operating-Guide.md`
  - 428행 MR 시작 순서 문단: 깊이 테스트는 기본으로 꺼져 있다. 그래서 실세계 깊이 추정은 적용하지 않는다고 쓴다. 손 가림은 Varjo Base Pro 4.16 이상의 Occlusion → Hands로 한다. 앱 깊이 테스트와 함께 켜지 않는다.
  - 키 표(40행 `T`)에는 손대지 않는다.
- `Docs/Headset-Test-Checklist.md`
  - 157행: "손이 차량 앞에 보이는지"를 고친다. 기본은 깊이 테스트 꺼짐이다. 손 가림은 Varjo Base Occlusion으로 확인한다.
  - §2의 `T` 행(59행)에 한 줄을 더한다: 기본은 꺼짐이고, Varjo Base Occlusion과 함께 켜지 않는다.

## 범위 밖

- Varjo Base Occlusion을 앱에서 켜거나 끄는 것. 그런 API는 플러그인에 없다. Varjo Base 설정이다.
- 깊이 범위·실세계 깊이 추정의 기본값 변경
- 저장된 `Tuning.json` 값을 고쳐 쓰는 마이그레이션(아래 "지켜야 할 조건")
- 나이트 모드 등 이번 시험에서 본 다른 Varjo Base 기능

## 지켜야 할 조건

- **저장값이 기본값보다 우선한다.** `Tuning.json`에는 사용자가 창에서 바꾼 값만 들어간다(`CXMRTuningSubsystem.cpp:65-70` 복원, `:127-130` 저장, `:141-146` 기본값으로 되돌리면 지움). 어떤 PC에 `Depth.Test: 1`이 저장돼 있으면 그 PC는 계속 켜진 채 시작한다. 이 동작을 바꾸지 않는다.
- MR 시작 순서는 그대로다: MR → `Depth.Test` → `Depth.Range` → (깊이 테스트가 켜졌을 때만) `Depth.EnvEstimation` → 마커 추적(`CXMRSubsystem.cpp:255-270`). 깊이 테스트가 꺼져 있어도 `SetDepthTestEnabled(false)`를 보내는 지금 동작을 유지한다.
- 키 `T`(깊이 테스트), `U`(실세계 깊이 추정), `Y`와 방향키(범위)의 동작
- 세션 전에는 Varjo 함수를 부르지 않는다(AGENTS.md)
- `.uasset`·`.umap`·`Config/` 변경 없음

## 근거

- Varjo Base Occlusion 설명과 "Depth awareness": `Docs/Varjo-Capabilities.md` §15 "Varjo Base의 Occlusion 기능"
- 깊이 테스트 호출: `CXMRSubsystem.cpp:255`(`UVarjoOpenXRFunctionLibrary::SetDepthTestEnabled`)
- 레지스트리 등록: `CXMRTuningWindowComponent.cpp:217-266`
- 기존 시험: `CXMRControlWindowTests.cpp`
  - `:61` `CXMR.Panels.ControlStartup` — `:67`에서 깊이 테스트를 직접 켜고 시작하므로 그대로 통과해야 한다.
  - `:215-216` — 기본값이 시안과 같은지 본다. `Depth.Test`는 이제 0이다.
  - `:158` — 도움말이 시안과 같은지 본다.
  - `:218` — 헤드셋이 없을 때 기본 깊이 설정을 유지하는지 본다.

## 구현자가 정할 것

- 시험을 기존 시험에 더할지 새 시험으로 만들지

## 경계 상황

- 저장값 없음 → 세션 시작 후 깊이 테스트 꺼짐. 실세계 깊이 추정 호출 없음.
  - 시험: 기본값으로 늦은 세션 시작. 호출 순서가 `MR, Depth.Test, Depth.Range, Diagnostics.MarkerTracking`이다. `IsDepthTestOn()`과 `IsEnvironmentDepthEstimationOn()`은 false다.
- `Tuning.json`에 `Depth.Test: 1` → 예전처럼 켜지고 실세계 깊이 추정도 적용된다.
  - 시험: 저장값 1을 넣은 레지스트리로 시작하면 `bRequestedDepthTest`가 true다. 세션 시작 호출에 `Depth.EnvEstimation`이 들어간다.
- MR 중 `T`로 켬 → 실세계 깊이 추정 기본값(1)이 적용된다. 기존 시험 `:79-80`이 확인한다.
- 고급 창 MR 탭 변경 수 → 기본 상태에서 0이다. 깊이 테스트를 켜면 1이다. "Reset MR to defaults"를 누르면 꺼진다.
  - 시험: 실제 레지스트리(`CXMRTuningWindowComponent` 등록)로 확인한다.
- 헤드셋 없음(이 PC의 PIE) → Varjo 호출 없음. 기존 시험 `:81-82`가 확인한다.

## 완료 조건

1. 기본값 두 곳이 바뀌고, 시안 기본값(`def: false`)과 레지스트리 기본값이 같다.
2. 문구 세 곳이 시안과 글자까지 같다. `CXMR.Panels.AdvancedPrototypeWording`과 `CXMR.Panels.ControlStatusText`에서 확인한다.
3. "경계 상황"의 시험이 들어 있고 통과한다.
4. `-DisableAdaptiveUnity` 빌드에서 경고·오류가 0이고, `CXMR.` 전체가 통과한다.
5. 문서 두 곳을 고친다. 바이너리·Config 변경은 0이다.

## 로컬에서 확인할 것

- 빌드: `-DisableAdaptiveUnity`. 자동화: `CXMR.` 전체.
- 헤드셋(사용자, not-tested):
  - Varjo Base Occlusion을 끈 채 MR을 시작한다. 손이 차 뒤로 숨는지 본다(깊이 테스트 꺼짐).
  - Occlusion → Hands를 켠다. 손이 차 앞에 보이는지 본다.
  - 고급 창에서 깊이 테스트를 켠다. 10-15에 본 이상한 화면이 다시 나오는지 본다(비교용).
  - 체크리스트 §2·§5.

## 개정 이력

- 2026-10-15: 처음 작성 (Claude). 사용자 헤드셋 시험 결과에 따른 결정
