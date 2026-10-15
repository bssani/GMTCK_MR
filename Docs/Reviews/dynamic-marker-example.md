# dynamic-marker-example 리뷰

## 1회차 — 2026-10-15, `2003aba`

재현(Claude):
- `-DisableAdaptiveUnity` 빌드 성공(C++ 변경 없음, 갱신할 것 없음). `CXMR.` 80/80 통과, `TEST COMPLETE. EXIT CODE: 0`. 로그 `Saved/ClaudeReview1015f_Build.log`, `Saved/ClaudeReview1015f_Tests.log`.
- `main`(4번 머지 포함)과의 병합을 `git merge-tree`로 미리 봤다. 충돌 없음.

**통과. 열린 항목 없음.**

에디터 MCP로 직접 읽은 것(로그 `Saved/ClaudeReview1015f_Editor.log`):

- `BP_DynamicMarkerBox`
  - `ACXMRMarkerFollower`의 자식이다.
  - CDO 값: `MarkerId` 300, `TrackingMode` Dynamic, `Follow` true, `MarkerToActor` 항등, `WhenLost` Hold, `ShowLabel` true.
  - `Box`: `/Engine/BasicShapes/Cube`, `BasicShapeMaterial`, 상대 Scale 1.0, 충돌 없음.
- `L_DynamicMarker`
  - 액터 10개: 조명·하늘·안개·구름, PlayerStart, 하늘 구, 바닥, PPV, 박스. 차량 관련 액터는 없다.
  - PPV는 Unbound이고 블렌더블은 `/CXMR/Core/Materials/PP_MR`(가중치 1)다.
  - 하늘·안개·구름·하늘 구·바닥 다섯 개에 `CXMRSceneObjectComponent` VR Only가 붙어 있다.
  - 박스 인스턴스: 위치 (100, 0, 100), 액터 Scale 0.3, 인스턴스 값이 CDO와 같다.
  - `.umap` 이름표에 `MarkerId`가 없다. 인스턴스가 번호를 덮어쓰지 않았고, BP 기본값 300을 그대로 쓴다.
- 데스크톱 PIE에서 박스가 (100, 0, 100)에 Scale 0.3으로 있고, 라벨은 `Marker 300 · waiting`이다. 화면에 재빌드 문구가 없다. CXMR 경고·오류도 없다.

기록(조치 필요 없음):

- **`L_DynamicMarker_BuiltData.uasset`을 `.gitignore:65`(`*_BuiltData.uasset`)의 예외로 커밋한 것은 맞다.**
  - git으로 새로 받은 PC와 같게 이 파일을 치우고 레벨을 열어 봤다. `LoadErrors: Warning: ... a dependent package /Game/Map/L_DynamicMarker_BuiltData was not available`가 났다. 헤드셋 PC가 바로 이 상태다.
  - PIE는 BuiltData가 없어도 정상이었다.
  - 이 파일은 `L_Main_BuiltData`를 복사한 것이다(1.73 MB, 크기가 거의 같다). 지운 반사 캡처의 데이터가 남아 있을 수 있지만 쓰이지 않는다.
- **`L_Main`에도 같은 문제가 있다(이번 작업 전부터).** `L_Main_BuiltData`가 git에 없다. git으로 받은 헤드셋 PC에서 `L_Main`을 열면 같은 경고가 나고, 차 주변 반사 캡처(`Docs/Operating-Guide.md` §5 "L_Main 조명")에 빌드 데이터가 없다. 고칠지는 사용자가 정한다.
- 차량이 없을 때 컨트롤 창 Vehicle 탭은 비어 있다. Codex 스크린샷(`CodexDynamicMarkerExample_NoVehicle.png`)에서는 선택한 탭 버튼이 흰 상자로 보인다. 누른 직후 마우스가 위에 있던 상태로 보이며, 이 명세 범위 밖이다. 다음에 컨트롤 창을 볼 때 확인한다.
- Codex는 창 캡처에 로컬 도구 MCP(포트 3000)도 썼다. 설정·플러그인은 바꾸지 않았고, 커밋에도 들어가지 않았다.
- 헤드셋 시험 상태는 **not-tested**다(체크리스트 §15-4).

## 질문

### Q1. Box 컴포넌트 Scale과 사용법의 액터 Scale 단위가 다름

- 상태: **반영함 (`2003aba`)** (2026-10-15, Codex) — Claude 답대로 구현·검증함
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

- **완료.** `codex/dynamic-marker-example`, Claude 답 `b332118` 위에 구현함. 열린 질문 없음.
- 추가: `/Game/Examples/BP_DynamicMarkerBox`, `/Game/Map/L_DynamicMarker`, 복제 때 생성된 `/Game/Map/L_DynamicMarker_BuiltData`(원래 ignore 규칙의 예외로 이 파일만 커밋함).
- BP 부모 `CXMRMarkerFollower`, 기본값 300 / Dynamic / Follow true / MarkerToActor 항등 / Hold / Show Label true. Box 상대 Scale 1.0, 루트 기본 Scale 1.0, 엔진 Cube·BasicShapeMaterial, NoCollision. BP 디스크 재로드 후에도 300 유지.
- 배치: `(100, 0, 100) cm`, 액터 Scale `(0.3, 0.3, 0.3)`. 메시 bounds extent `(15, 15, 15) cm`로 한 변 30 cm. 여섯 Class Defaults 항목이 인스턴스 값과 같음.
- 복제본에서 제거: `VehicleRoot_TestCar`, `MaskCube_Test`, `ReflectionCapture_Vehicle`.
- 최종 액터 10개: `DirectionalLight`, `SkyAtmosphere`, `SkyLight`, `ExponentialHeightFog`, `VolumetricCloud`, `PlayerStart`, `SM_SkySphere`, `Floor`, `PostProcessVolume`, `BP_DynamicMarkerBox`.
- `PP_MR` 블렌더블·Unbound와 하늘/바닥 VROnly 구성 유지. `L_Main`, `/Game/Vehicle/`, `/CXMR/`, C++·Config·도구 플러그인 변경 없음.
- 데스크톱 PIE 10초 이상(실시간 조회 33.76초)에서 박스·`Marker 300 · waiting` 확인. 같은 PIE에서 CXMR Control·Advanced settings 창을 함께 열어 캡처함. CXMR 오류 0, 재빌드 경고 문구 0.
- 추가 경계: 다음 PIE에서 301 라벨과 Scale 0.4 유지, 0은 Tick/라벨 비활성 + 예상 경고 1회. 차량 없이 Recalibrate·PlaceVehicle 입력을 처리하고 PIE 유지. 최종 300·0.3으로 복원·저장함. 실제 마커 추적 중 크기 보존은 헤드셋 미검증임.
- `-DisableAdaptiveUnity` 빌드 성공(exit 0), 컴파일 경고·오류 0. `CXMR.` 발견·완료·성공 **80/80**, 실패 0, exit 0. 4번은 독립 브랜치라 81개 시험을 포함하지 않음.
- 로그: `Saved/CodexDynamicMarkerExample_Build.log`, `Saved/CodexDynamicMarkerExample_Tests.log`, `Saved/CodexDynamicMarkerExample_Editor.log`. MCP 스크립트·결과는 `Saved/CodexDynamicMarkerExample_*`.
- 스크린샷: `Saved/CodexDynamicMarkerExample_PIE.png`, `Saved/Screenshots/CodexDynamicMarkerExample_Control.png`, `Saved/Screenshots/CodexDynamicMarkerExample_Advanced.png`, `Saved/Screenshots/CodexDynamicMarkerExample_NoVehicle.png`.
- MCP는 55559 UnrealAgent로 애셋 제작/조회/PIE/저장·종료, 3000 native MCP로 Slate 창 캡처·입력 확인. 기본 제공 vbor 연결의 다른 토큰 대신 이 프로젝트의 기존 로컬 인증으로 접속함. 설정·플러그인은 수정하지 않음. 최초 실행의 `-nocef` 충돌과 Python API 조회 오류는 제작 로그에 있으며 CXMR 실행 오류와 구분함.
- Operating-Guide 사용법과 체크리스트 §15-4를 개정 명세 그대로 옮김. 완료 조건 1~7 확인, 저장 후 에디터 종료 확인. 헤드셋 §15-4 **not-tested**.
