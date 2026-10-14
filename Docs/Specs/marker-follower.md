# 마커를 따라가는 액터 — BP로 만들어 마커 번호를 지정

- 베이스 브랜치: `main`
- 상태: 구현 대기 — **급함.** 사용자가 2026-10-15에 다른 PC에서 git으로 받은 빌드로 시험한다. 다른 작업보다 먼저 한다.

## 목적

사용자 요청(2026-10-14):

- 실물 박스에 Varjo 마커를 붙이고 움직이면 **가상 메시가 박스를 잘 따라가는지** 헤드셋에서 확인한다. Stationary와 Dynamic 추적을 비교한다.
- 방식: 플러그인에 C++ 부모 클래스를 만든다. 사용자가 그 자식 Blueprint(예: `BP_Dynamic`)를 만들어 **Details에서 마커 번호를 지정**하고, 그 BP 안에 원하는 메시를 넣어 레벨에 놓는다. 콘솔 명령 방식은 쓰지 않는다(사용자가 거절).

현재 상태:

- CXMR에는 마커를 따라 움직이는 액터가 없다. 마커 축·라벨 디버그(`CXMRMarkerDebugComponent`)는 선만 그린다.
- Varjo 예제 맵의 `BP_MarkerControls`는 리포에 없고(`.gitignore`), 시험에 쓰지 않는다.

## 범위

### 1. C++ 부모 클래스 (플러그인, `Blueprintable`, 레벨에 놓을 수 있음)

이름은 구현자가 정한다(예: `ACXMRMarkerFollower`, 표시 이름 "CXMR Marker Follower"). 루트는 `USceneComponent`이고, 자식 BP가 그 아래에 메시를 붙인다.

Details에 보일 값 — 카테고리 `CXMR|Marker Follower`. 툴팁은 영어:

| 값 | 기본 | 뜻 |
|---|---|---|
| `MarkerId` | 0(미지정) | 따라갈 마커 번호. 0이면 아무것도 하지 않고 경고 한 줄을 남긴다 |
| `TrackingMode` | Dynamic | 이 마커의 Varjo 추적 모드(Dynamic / Stationary) |
| `bFollow` | true | 끄면 그 자리에 멈춘다. 마커 이벤트는 받지만 움직이지 않는다 |
| `MarkerToActor` | 항등 Transform | 마커 기준으로 액터 원점이 어디 있는지(cm·deg). 메시를 실물 박스에 맞출 때 쓴다 |
| `WhenLost` | Hold | Hold: 마지막 자리에 둔다. Hide: 숨긴다. 다시 보이면 복귀 |
| `bShowLabel` | true | 액터 위에 `Marker <ID> · Dynamic` / `· Stationary` / `· lost` 라벨. 모드는 런타임이 실제로 보낸 값(`UCXMRSubsystem::GetMarkerTrackingMode`)이다 |

Blueprint에서 부를 함수·이벤트:

- `SetFollow(bool)`, `SetTrackingMode(Mode)` — 키나 버튼에 연결할 수 있게 한다.
- `IsMarkerTracked()` — 지금 관측 중인지.
- `OnMarkerFound`, `OnMarkerLost` — BP에서 받을 이벤트(`BlueprintImplementableEvent` 또는 델리게이트, 구현자가 정함). 예를 들어 놓쳤을 때 색을 바꾸는 데 쓴다.

### 2. 동작

- `UCXMRSubsystem`의 마커 이벤트(`OnMarkerDetected`, `OnMarkerMoved`, `OnMarkerLost`, `CXMRSubsystem.h:163-165`)를 구독한다. 자기 `MarkerId`일 때만 반응한다.
- 액터 위치는 `MarkerToActor * 마커 월드 포즈`다(UE에서 A*B는 A를 먼저 적용). 이벤트 위치는 이미 월드 좌표다(`Docs/Varjo-Capabilities.md` §5-1).
- 추적 모드·timeout 설정은 마커가 감지된 뒤에만 성공한다(`CXMRSubsystem.h:167`, `Docs/Varjo-Capabilities.md` §5-2 3번).
  - BeginPlay 때 이미 감지된 마커면 바로 설정한다.
  - 아니면 첫 감지·이동 이벤트에서 설정한다. 실패하면 다음 이벤트에서 다시 시도한다.
  - PIE 중 Details에서 `TrackingMode`를 바꿔도 반영돼야 한다(`PostEditChangeProperty` 또는 같은 효과).
- EndPlay 때 그 마커의 모드를 되돌린다. 차량 보정 마커였으면 프로필의 `TrackingMode`, 아니면 Stationary다.
- `MarkerId`가 지금 차량 마커 프로필의 보정 마커와 같으면 경고 로그를 남긴다: "Marker <ID> is a calibration marker. Car calibration is less accurate while it is Dynamic." 동작은 막지 않는다.
- 레벨에 같은 `MarkerId`를 쓰는 액터가 둘 이상이면 경고하고 둘 다 따라가게 둔다. 모드는 나중에 설정한 쪽이 이긴다.

### 3. 문서

- `Docs/Operating-Guide.md`에 "마커를 따라가는 메시 만들기" 절. 아래 "만드는 법"을 그대로 쓴다.
- `Docs/Headset-Test-Checklist.md` §15-4를 아래 "시험 절차"로 바꾼다.

## 범위 밖

- 예시 BP·레벨 배치. 사용자가 시험 PC에서 직접 만든다. 바이너리 애셋을 리포에 더하지 않는다.
- 한 물체에 마커 여러 개, 면 지정, 떨림 줄이기
- 컨트롤 창 버튼. 고급 창의 "Dynamic tracking for all markers"는 control-window 명세에 그대로 있다.

## 지켜야 할 조건

- 차량 보정·저장·복원 동작은 그대로다. 이 액터는 Placement와 상관없다.
- 세션 전에 Varjo 함수를 부르지 않는다(AGENTS.md). 모드 설정은 감지 뒤에만 한다.
- 레벨에 이 액터가 없으면 아무 일도 없다.
- `/CXMR/`·`/Game/` 애셋과 레벨을 바꾸지 않는다.

## 근거

- 마커 이벤트·모드 함수: `CXMRSubsystem.h:21-22, 163-172`
- 마커 이벤트 규칙: 감지는 세션당 ID마다 한 번이고, 다시 잡으면 이동 이벤트로 온다. 놓침에는 위치가 없다(`Docs/Varjo-Capabilities.md` §5-2).
- 이벤트 구독 선례: `UCXMRMarkerDebugComponent`(`CXMRMarkerDebugComponent.cpp:56-68`)

## 구현자가 정할 것

- 클래스·함수 이름, 라벨 표시 방법(TextRender 등), 이벤트를 BP에 넘기는 방식

## 경계 상황

- `MarkerId` 0 → 아무것도 안 함, 경고 한 번.
- 마커가 한 번도 안 보임 → 레벨에 놓은 자리에 그대로 있다. 라벨 `Marker <ID> · waiting`.
- 헤드셋 세션 없음(이 PC의 PIE) → 위와 같다. Varjo 호출 없음.
- 놓침 → `WhenLost`대로, 라벨 `· lost`. 다시 이동 이벤트가 오면 복귀.
- `bFollow` 끔 → 그 자리에 멈춤. 다시 켜면 다음 이벤트 위치로 간다.
- 마커 추적이 꺼져 있음 → 로그 "Marker tracking is off. Press V or turn it on in the control window." 한 번.

## 완료 조건

1. 자동화 시험(합성 이벤트 — 서브시스템 델리게이트를 직접 Broadcast, 테스트 월드에 액터 스폰):
   - 감지·이동 → 위치·회전이 `MarkerToActor * 마커`와 일치
   - 다른 ID는 무시
   - 놓침 → Hold/Hide
   - 다시 이동 → 복귀
   - `bFollow` 끔 → 안 움직임
   - `MarkerId` 0 → 아무것도 안 함
   - 보정 마커 ID → 경고
2. 모드 설정을 감지 전에는 부르지 않는다는 것을 시험으로 확인한다. Varjo 호출을 가로챌 자리가 없으면 그 이유와 대신 확인한 방법을 보고 ③에 적는다.
3. 에디터에서 이 클래스를 부모로 BP를 만들 수 있고, Details에 위 값이 보인다. 에디터 MCP로 임시 BP를 만들어 확인만 하고, 저장하지 않고 지운다. 보고 ⑤에 스크린샷.
4. `-DisableAdaptiveUnity` 빌드 경고·오류 0, `CXMR.` 전체 통과.
5. 바이너리 애셋 변경 0. 문서 두 곳.

## 만드는 법 (Operating-Guide에 옮길 내용)

1. Content Browser에서 `/Game/` 아래 원하는 폴더에 Blueprint Class를 만들고, 부모로 **CXMR Marker Follower**를 고른다(예: `BP_Dynamic`).
2. BP를 열어 루트 아래에 Static Mesh를 붙인다. 실물 박스 치수에 맞춘다(엔진 큐브는 100 cm, 30 cm 박스면 Scale 0.3).
3. Class Defaults(또는 레벨에 놓은 뒤 Details)에서 `Marker Id`에 박스에 붙인 마커 번호를 넣는다. **차량 보정 마커와 다른 번호**를 쓴다.
4. `Tracking Mode`를 고른다. 움직이는 물체는 Dynamic이다.
5. 레벨에 놓는다. 위치는 상관없다. 마커가 보이면 그 자리로 간다.
6. 메시가 실물 박스와 어긋나면 `Marker To Actor`로 맞춘다. 마커가 박스 윗면 가운데에 있다면, 박스 중심은 마커에서 박스 높이의 절반만큼 면 안쪽이다. 어느 축이 면 바깥인지는 마커 축 표시로 확인한다.

## 시험 절차 (체크리스트 §15-4로 옮길 내용)

1. 위 "만드는 법"대로 `BP_Dynamic`을 만들어 `L_Main`에 놓는다. `Tracking Mode`는 Stationary로 시작한다.
2. VR Preview에서 박스를 보이게 든다. 메시가 박스로 오고, 라벨이 `Stationary`인지 본다.
3. 박스를 천천히, 그다음 빠르게 움직인다. 메시가 얼마나 늦게 따라오는지, 멈췄을 때 떨리는지, 손이 마커를 가리면 언제 `lost`가 되는지 본다.
4. 레벨에 놓은 `BP_Dynamic`을 고르고 Details에서 `Tracking Mode`를 Dynamic으로 바꾼다. PIE 중 바꿔도 된다. 라벨이 `Dynamic`이 되면 3을 반복한다.
5. 마커 축 표시(튜닝 창 "Show marker axes and labels")를 켜고, 빨강·초록·파랑 중 어느 축이 마커 면 바깥을 향하는지 사진 한 장을 남긴다.
6. 결과는 한두 줄이면 된다.

## 개정 이력

- 2026-10-14: 처음 작성 (Claude). 박스 따라가기 큰 기능(면·여러 마커)을 썼다가 지웠고, 콘솔 명령안도 사용자가 거절. Details에서 마커 번호를 지정하는 BP 부모 클래스로 정함
