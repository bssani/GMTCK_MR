# 움직이는 마커 시험용 예제 레벨

- 베이스 브랜치: `main`
- 상태: 구현 대기

## 목적

사용자 요청(2026-10-15): 10-15 헤드셋 시험(2시간)에서 동적 마커(박스에 붙인 마커를 메시가 따라가는지)를 시험하지 못했다. 지금은 시험 PC에서 BP를 만들고 레벨에 놓아야 시작할 수 있다. **열고 바로 VR Preview를 누르면 되는 예제 레벨**을 리포에 넣는다.

- 마커 번호는 **300**으로 시작한다.
- 마커 번호는 Details에서 바꿀 수 있어야 한다. 시험 박스에 다른 번호를 붙일 수 있기 때문이다.

필요한 기능은 이미 있다. `ACXMRMarkerFollower`는 `main`에 머지됐다(`Docs/Specs/marker-follower.md`, `CXMRMarkerFollower.h:28-38`). **이번 작업은 애셋 두 개와 문서뿐이다.** C++는 바꾸지 않는다.

## 범위 — 전부 Codex가 에디터 MCP로 한다

### 1. Blueprint `/Game/Examples/BP_DynamicMarkerBox`

- 부모: **CXMR Marker Follower**(`ACXMRMarkerFollower`)
- 컴포넌트: 부모의 루트 아래에 Static Mesh `Box` 하나를 붙인다.
  - 메시: `/Engine/BasicShapes/Cube`
  - Scale 0.3(30 cm 정육면체). **실물 박스 크기는 모른다.** 이 크기는 근사값이고, 사용자가 맞춘다.
  - 머티리얼: 엔진에 이미 있는 것을 쓴다(구현자가 정함). 새 머티리얼 애셋을 만들지 않는다.
  - 충돌 없음
- Class Defaults:

| 값 | 설정 | 이유 |
|---|---|---|
| `Marker Id` | **300** | 사용자 지정 |
| `Tracking Mode` | Dynamic | 움직이는 물체 시험이 목적이다. Stationary 비교는 Details에서 바꾼다 |
| `Follow` | true | 부모 기본값 |
| `Marker To Actor` | 항등 | 마커 축 방향을 아직 모른다(아래 "범위 밖") |
| `When Lost` | Hold | 부모 기본값 |
| `Show Label` | true | 부모 기본값 |

### 2. 레벨 `/Game/Map/L_DynamicMarker`

- `/Game/Map/L_Main`을 복제해 만든다. MR에 필요한 구성을 그대로 가져오기 위해서다: 알파를 내보내는 PPV(`PP_MR`), 조명, 하늘·바닥의 VROnly 표시(`Docs/Operating-Guide.md` §5 "새 레벨을 만들면…").
- 차량과 관련된 액터를 지운다: `ACXMRVehicleRoot`와 그 자식 BP, 차량용 마스크 큐브(`BP_MaskCube`) 등. **지운 액터와 남긴 액터 목록을 보고 ⑤에 적는다.**
- 반사 캡처는 구현자가 정한다(지우거나 다시 빌드). 다만 PIE 화면에 "needs to be rebuilt" 같은 문구가 뜨면 안 된다.
- `BP_DynamicMarkerBox`를 하나 놓는다. 자리는 플레이어 시작점 앞 약 1 m, 높이 약 1 m다. 마커가 보이기 전에도 라벨 `Marker 300 · waiting`이 보이게 하려는 것이다.
- 레벨에 놓은 인스턴스는 Class Defaults를 덮어쓰지 않는다. 사용자가 이 인스턴스의 Details에서 `Marker Id`를 바꾼다.

### 3. 문서

- `Docs/Operating-Guide.md` §3-2 "마커를 따라가는 메시 만들기"의 맨 앞에 "예제 레벨로 바로 시험하기"를 더한다. 아래 "사용법"을 그대로 쓴다.
- `Docs/Headset-Test-Checklist.md` §15-4를 아래 "시험 절차"로 바꾼다. 제목은 "15-4. 박스에 붙인 마커를 메시가 잘 따라가는가"를 유지한다.

## 범위 밖

- **C++ 변경.** PIE 중에 `Marker Id`를 바꾸면 반영하는 기능은 지금 없다(`CXMRMarkerFollower.cpp:203-211`은 `Tracking Mode`만 다룬다). 번호는 PIE를 멈추고 바꾼다. 필요하면 사용자가 따로 요청한다.
- 마커 축 방향에 맞춘 `Marker To Actor` 기본값. 어느 축이 마커 면 바깥인지는 헤드셋 사진으로 확인해야 한다.
- 시작 맵 바꾸기(`Config/DefaultEngine.ini`), 키 추가, 컨트롤 창 버튼
- 차량 보정과 같이 시험하는 구성. 차량이 있는 레벨은 `L_Main`이다.

## 지켜야 할 조건

- `L_Main`과 `/Game/Vehicle/`의 기존 애셋을 바꾸지 않는다. 복제만 한다.
- `/CXMR/` 애셋, `Plugins/CXMR/Content/`, `Config/` 변경 없음
- 새 바이너리는 위 두 개뿐이다(레벨 복제로 따라오는 `_BuiltData`가 생기면 그것도 보고한다). 그 밖의 `.uasset`·`.umap` 변경이 diff에 있으면 보고한다.
- C++ 변경 없음. 그래서 빌드는 확인용으로 한 번만 한다.

## 근거

- 부모 클래스와 Details 값: `CXMRMarkerFollower.h:28-38`
- 마커 번호가 0일 때의 동작, 구독, 라벨: `CXMRMarkerFollower.cpp:32-71`, `:183-200`
- 레벨에 놓인 크기 보존: `CXMRMarkerFollower.cpp:142-145`
- 게임 모드가 Pawn을 스폰하므로 레벨에 Pawn을 둘 필요가 없다: `Config/DefaultEngine.ini:6`, `Docs/Operating-Guide.md` 26행
- 차량이 없을 때 컨트롤 창은 "No vehicle loaded"를 보인다: `CXMRTuningWindowComponent.cpp:478`

## 구현자가 정할 것

- 머티리얼, 반사 캡처 처리, 정확한 배치 좌표
- 레벨을 복제한 뒤 지울 액터를 고르는 기준. 차량 기능에만 쓰이는 액터를 지운다. 애매하면 남기고 보고한다.

## 경계 상황

- 헤드셋 없음(이 PC의 PIE) → 박스가 놓인 자리에 그대로 있다. 라벨은 `Marker 300 · waiting`. 세션 전 Varjo 호출은 없다(부모 클래스가 이미 보장함).
- 차량 없음 → 컨트롤 창과 고급 창이 오류 없이 열린다. 상태줄은 보정 전 상태를 보인다. Calibration 동작 버튼을 눌러도 크래시가 없다.
- 인스턴스의 `Marker Id`를 0으로 바꿈 → 부모 동작대로 아무것도 하지 않고 경고를 한 번 남긴다.
- 인스턴스의 `Marker Id`를 다른 번호(예: 301)로 바꿈 → 다음 PIE부터 그 번호를 따라간다. 라벨이 `Marker 301 · waiting`이다.

## 완료 조건

1. `BP_DynamicMarkerBox`의 부모와 Class Defaults가 위 표와 같다. 에디터 MCP로 읽어 보고 ⑤에 적는다.
2. `L_DynamicMarker`에 차량 액터가 없고, `BP_DynamicMarkerBox` 인스턴스가 하나 있으며, 그 `Marker Id`는 300이다(인스턴스 덮어쓰기 없음). 남은 액터 목록을 보고 ⑤에 적는다.
3. 이 PC에서 `L_DynamicMarker`를 열고 PIE(데스크톱)를 10초 돌린다. 박스와 라벨 `Marker 300 · waiting`이 보이고, 로그에 CXMR 오류가 없다. 스크린샷을 남긴다. MCP로 PIE를 돌릴 수 없으면 그 이유와 대신 확인한 방법을 보고 ③에 적는다.
4. 같은 PIE에서 컨트롤 창과 고급 창이 열리고 오류가 없다.
5. `-DisableAdaptiveUnity` 빌드에서 경고·오류가 0이고, `CXMR.` 전체가 통과한다(C++ 변경이 없다는 확인).
6. 문서 두 곳을 고친다. 바이너리 변경은 위 애셋뿐이다.
7. 끝나면 저장하고 에디터를 닫는다.

## 사용법 (Operating-Guide §3-2로 옮길 내용)

1. `/Game/Map/L_DynamicMarker`를 연다.
2. 실물 박스에 붙인 마커 번호가 300이 아니면, World Outliner에서 `BP_DynamicMarkerBox`를 고르고 Details의 `Marker Id`를 바꾼다. **PIE를 멈춘 상태에서** 바꾼다. PIE 중에 바꾼 번호는 반영되지 않는다.
3. 박스 크기가 30 cm가 아니면 같은 액터의 Scale을 바꾼다(1.0 = 100 cm).
4. VR Preview를 누른다. 마커가 보이면 메시가 박스로 간다.
5. 메시가 박스와 어긋나면 `Marker To Actor`로 맞춘다. 마커가 윗면 가운데에 붙어 있으면, 박스 중심은 마커에서 박스 높이의 절반만큼 안쪽에 있다. 어느 축이 면 바깥인지는 고급 창 Diagnostics → `Marker axes and labels`로 본다.

## 시험 절차 (체크리스트 §15-4로 옮길 내용)

1. `L_DynamicMarker`를 열고, 마커 번호를 "사용법" 2대로 맞춘다. `Tracking Mode`는 Dynamic이다.
2. VR Preview에서 박스를 보이게 든다. 메시가 박스로 오는지, 라벨이 `Dynamic`인지 본다.
3. 박스를 천천히, 그다음 빠르게 움직인다. 볼 것: 메시가 얼마나 늦게 따라오는지, 멈췄을 때 떨리는지, 손이 마커를 가리면 언제 `lost`가 되는지.
4. PIE 중 World Outliner에서 `BP_DynamicMarkerBox`를 고르고 `Tracking Mode`를 Stationary로 바꾼다. 라벨이 `Stationary`가 되면 3을 반복한다.
5. 고급 창 Diagnostics → `Marker axes and labels`를 켠다. 빨강·초록·파랑 중 어느 축이 마커 면 바깥을 향하는지 사진을 한 장 남긴다.
6. 결과는 한두 줄이면 된다. Dynamic과 Stationary의 차이, 마커 축 사진.

- 이 레벨에는 차가 없다. 차량 보정은 `L_Main`에서 한다.
- 이 절의 헤드셋 시험 상태는 **not-tested**다.

## 로컬에서 확인할 것

- 빌드: `-DisableAdaptiveUnity`. 자동화: `CXMR.` 전체.
- 에디터: 완료 조건 1~4(MCP 읽기·PIE 스크린샷)
- 헤드셋(사용자, not-tested): 체크리스트 §15-4

## 개정 이력

- 2026-10-15: 처음 작성 (Claude). 10-15 헤드셋 시험에서 동적 마커를 시험하지 못한 뒤 사용자가 요청함
