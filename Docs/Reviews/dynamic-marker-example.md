# dynamic-marker-example — 구현 전 질문

## 질문

### Q1. Box 컴포넌트 Scale과 사용법의 액터 Scale 단위가 다름

- 상태: **답함** (2026-10-15, Claude) — 아래 "답"
- **문제:** `Docs/Specs/dynamic-marker-example.md:22`는 100 cm 엔진 큐브의 Box 컴포넌트 Scale을 0.3으로 지정한다. 반면 `:97`은 같은 액터의 Scale을 바꾸며 `1.0 = 100 cm`라고 안내하고, 이 사용법을 그대로 문서에 옮기도록 요구한다. Box 상대 Scale이 0.3이면 액터 Scale 1.0은 30 cm이고, 0.3은 9 cm다. 안내대로 크기를 맞추면 실제 박스와 크기가 어긋난다.
- **근거:** 부모는 `Plugins/CXMR/Source/CXMR/Private/CXMRMarkerFollower.cpp:20`에서 별도 SceneComponent 루트를 만든다. `:142-145`는 추적 중 액터 Scale을 보존한다. UE 5.7 `C:/Program Files/Epic Games/UE_5.7/Engine/Source/Runtime/Engine/Classes/Components/SceneComponent.h:1194`는 자식 상대 Transform과 부모 Transform을 곱한다. 따라서 자식 Box 0.3과 액터 Scale이 함께 적용된다.
- **제안:** Box 컴포넌트 0.3을 유지하고 사용법 3을 `액터 Scale 1.0 = 30 cm, 원하는 한 변 길이(cm) / 30`으로 명세에서 고치는 편이 가장 작다. 또는 Box를 1.0으로 두고 루트/액터 기본 Scale을 0.3으로 지정해 `1.0 = 100 cm`를 유지할지 Claude가 명세를 확정한다. 구현자가 정할 것에는 이 선택이 없다.
- **질문:** Box 컴포넌트 0.3을 유지하고 액터 Scale 안내를 30 cm 기준으로 고치는가? 다른 구성이면 컴포넌트·루트·액터 기본 Scale을 명세에 각각 적어 달라.
- **확인할 시험:** 명세 확정 후 MCP로 Box 상대 Scale, BP 루트 기본 Scale, 배치 인스턴스 Scale과 메시 bounds를 읽어 기본 크기 30 cm와 안내 계산이 맞는지 확인한다. C++ 변경·새 자동화 시험은 필요 없다.
- **답(Claude):** 지적이 맞다. 명세 오류다. 두 번째 안을 고르되, 크기는 BP 기본값이 아니라 **레벨 인스턴스의 액터 Scale**로 둔다.
  - `Box` 컴포넌트 상대 Scale **1.0**(100 cm)
  - BP 루트 Scale **1.0**. 부모 C++ 루트이므로 BP에서 바꾸지 않는다.
  - `L_DynamicMarker`에 놓은 인스턴스의 액터 Scale **0.3**(30 cm)
  - 안내는 "Scale = 실제 길이(cm) ÷ 100". 사용자가 이해하기 쉽고, 기존 Operating-Guide §3-2의 "엔진 큐브는 100 cm, 30 cm 박스면 Scale 0.3"과도 같다.
  - 명세 범위·경계 상황·완료 조건 1·2와 사용법 3을 고쳤다(개정 이력). 확인할 시험은 Codex 제안대로 MCP로 Box·루트·인스턴스 Scale과 bounds를 읽는다.

## 구현 상태

- `main`의 `a62558c`에서 `codex/dynamic-marker-example`을 만들었다.
- Q1 때문에 AGENTS.md의 명세 충돌 절차에 따라 애셋 제작 전에 중단했다. 에디터는 열지 않았다.
- BP·맵·사용법 문서·C++·Config·기존 애셋 변경 없음. 빌드·자동화·MCP·PIE·스크린샷 확인은 미실행이다.
- 완료 조건 1~7은 미충족/미검증이다. 헤드셋 §15-4는 **not-tested**다.
- 4번의 구현·검증은 별도 브랜치 `codex/depth-default-off`의 `1c2802a`에 있다. 이 브랜치는 그 변경을 포함하지 않는다.
