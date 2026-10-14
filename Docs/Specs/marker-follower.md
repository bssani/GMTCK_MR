# 마커를 따라가는 박스 — 콘솔 명령 하나로

- 베이스 브랜치: `main`
- 상태: 구현 대기 — **급함.** 사용자가 2026-10-15에 다른 PC에서 git으로 받은 빌드로 시험한다. 다른 작업보다 먼저 한다.

## 목적

사용자 요청(2026-10-14): 실물 박스에 Varjo 마커를 붙이고 움직이면 **가상 메시가 박스를 잘 따라가는지** 헤드셋에서 확인한다. Stationary와 Dynamic 추적을 비교한다.

- CXMR에는 마커를 따라 움직이는 메시가 없다. 마커 축·라벨 디버그(`CXMRMarkerDebugComponent`)는 선만 그린다.
- Varjo 예제 맵(`BP_MarkerControls`)에는 이 기능이 있지만 리포에 없고(`.gitignore`), 사용자는 예제를 쓰지 않는다.
- 레벨·애셋을 고치지 않고 **콘솔 명령으로 띄우는** 진단 기능으로 만든다. 시험하는 PC에서 git pull + 빌드만 하면 바로 쓸 수 있어야 한다.

## 범위

1. 콘솔 명령 `CXMR.FollowMarker`:
   - `CXMR.FollowMarker <ID>` — 그 마커 ID를 따라가는 박스를 띄운다. 추적 모드는 Dynamic이다.
   - `CXMR.FollowMarker <ID> stationary` / `dynamic` — 모드를 지정한다. 이미 띄운 ID에 다시 쓰면 모드만 바꾼다.
   - `CXMR.FollowMarker <ID> <모드> <크기cm>` — 박스 한 변 길이. 생략하면 마커 이벤트의 `Size` 너비다.
   - `CXMR.FollowMarker off` — 모두 지운다. `CXMR.FollowMarker off <ID>` — 그 ID만 지운다.
   - 잘못 쓰면 사용법 한 줄을 로그에 남긴다.
2. 따라가는 박스(새 액터 클래스, 이름은 구현자가 정함):
   - 메시: 엔진 기본 큐브(`/Engine/BasicShapes/Cube`)를 크기에 맞춰 늘린다. 색은 MR에서 잘 보이는 단색이다. 플러그인 `/CXMR/Core/Materials/M_CXMRUnlitColor`가 맞으면 쓰고, 아니면 엔진 기본 재질을 쓴다. 새 애셋은 만들지 않는다.
   - **박스 중심을 마커 위치에 둔다.** 마커의 어느 축이 면 바깥을 향하는지 아직 모르기 때문이다(`Docs/Headset-Test-Checklist.md` §15-4에서 확인). 실물 박스 안으로 반쯤 들어가 보여도 따라가는지 판단하는 데는 지장이 없다.
   - 박스 위에 라벨: `Marker <ID> · Dynamic`, `Marker <ID> · Stationary`. 모드는 런타임이 실제로 보낸 값(`UCXMRSubsystem::GetMarkerTrackingMode`)이다. 요청한 값이 아니다.
   - 마커 감지·이동 이벤트(`OnMarkerDetected`, `OnMarkerMoved`, `CXMRSubsystem.h:163-164`)마다 그 포즈로 옮긴다. 이벤트 위치는 이미 월드 좌표다(`Docs/Varjo-Capabilities.md` §5-1).
   - 놓침(`OnMarkerLost`): 마지막 자리에 두고 색을 흐리게, 라벨 끝에 ` · lost`를 붙인다. 다시 이동 이벤트가 오면 원래대로.
3. 추적 모드 적용:
   - Varjo의 모드·timeout 설정은 마커가 감지된 뒤에만 성공한다(`CXMRSubsystem.h:167`, `Docs/Varjo-Capabilities.md` §5-2 3번).
   - 명령을 받은 시점에 이미 감지된 마커면 바로 설정한다. 아니면 첫 감지·이동 이벤트에서 설정한다. 실패하면 다음 이벤트에서 다시 시도한다.
   - 모드를 바꾸면 그 사실을 로그에 남긴다.
4. 지울 때 그 마커의 모드를 되돌린다. 차량 보정 마커였으면 프로필의 `TrackingMode`로, 아니면 Stationary로.
5. 따라갈 ID가 지금 차량 마커 프로필의 보정 마커이면 경고를 남긴다: "Marker <ID> is a calibration marker. Car calibration is less accurate while it is Dynamic." 동작은 막지 않는다.
6. 문서:
   - `Docs/Headset-Test-Checklist.md` §15-4를 이 명령으로 하는 절차로 바꾼다. 아래 "시험 절차"를 그대로 옮긴다.
   - `Docs/Operating-Guide.md`의 콘솔 명령 목록에 한 줄을 더한다.

## 범위 밖

- 박스 크기·면·여러 마커 평균, 실물 박스와 정확히 겹치기
- 레벨 배치, 컨트롤 창 버튼(고급 창의 "Dynamic tracking for all markers"는 control-window 명세에 그대로 있다)
- 저장. 앱을 다시 켜면 없다.

## 지켜야 할 조건

- 차량 보정·저장·복원 동작은 그대로다. 따라가는 박스는 Placement와 상관없다.
- 세션 전에 Varjo 함수를 부르지 않는다(AGENTS.md). 모드 설정은 감지 뒤에만 한다.
- `/CXMR/`·`/Game/` 애셋과 레벨을 바꾸지 않는다.
- 명령이 없으면 아무것도 생기지 않는다. 지금 동작과 성능에 영향이 없다.

## 근거

- 마커 이벤트·모드 함수: `CXMRSubsystem.h:21-22, 163-172`
- 콘솔 명령 선례: `CXMRPlacementComponent.cpp:47-70`(`FAutoConsoleCommandWithWorld`)
- 마커 이벤트 규칙: 감지는 세션당 ID마다 한 번, 다시 잡으면 이동으로 온다. 놓침에는 위치가 없다(`Docs/Varjo-Capabilities.md` §5-2).

## 구현자가 정할 것

- 액터 클래스 이름, 라벨 표시 방법(TextRender 등), 놓쳤을 때 흐리게 하는 방법

## 경계 상황

- 같은 ID로 두 번 명령 → 박스는 하나이고 모드·크기만 바뀐다.
- 명령을 쓴 뒤 마커가 한 번도 안 보임 → 박스는 숨긴 채 기다린다. 처음 보이면 그 자리에 나타난다.
- 헤드셋 세션 없음(이 PC의 PIE) → 명령은 받되 아무것도 보이지 않는다. 로그에 "Waiting for marker <ID>"를 남긴다.
- PIE 종료·레벨 이동 → 박스는 사라진다(월드 액터).
- 마커 추적이 꺼져 있음 → 로그에 "Marker tracking is off. Press V or turn it on in the control window."를 남긴다.

## 완료 조건

1. 자동화 시험(합성 이벤트 — 서브시스템 델리게이트를 직접 Broadcast):
   - 명령 → 박스 생성
   - 감지·이동 → 위치·회전 일치
   - 놓침 → 자리 유지 + 라벨 ` · lost`
   - 다시 이동 → 복귀
   - `off` → 삭제
   - 같은 ID 두 번 → 하나
   - 보정 마커 ID → 경고
2. 모드 설정을 감지 전에는 부르지 않는다는 것을 시험으로 확인한다. Varjo 호출을 가로챌 자리가 없으면 그 이유와 대신 확인한 방법을 보고 ③에 적는다.
3. `-DisableAdaptiveUnity` 빌드 경고·오류 0, `CXMR.` 전체 통과.
4. 바이너리 애셋 변경 0.
5. 문서 두 곳.

## 시험 절차 (체크리스트 §15-4로 옮길 내용)

1. 박스에 붙일 마커는 **차량 보정 마커와 다른 ID**로 한다.
2. PIE(VR Preview) 중 콘솔(`~`)에서 `CXMR.FollowMarker <ID> stationary`를 친다. 박스를 보이게 들면 그 자리에 색 박스가 뜬다. 라벨은 `Stationary`다.
3. 박스를 천천히, 그다음 빠르게 움직인다. 색 박스가 얼마나 늦게 따라오는지, 멈췄을 때 떨리는지, 손이 마커를 가리면 언제 `lost`가 되는지 본다.
4. `CXMR.FollowMarker <ID> dynamic`을 친다. 라벨이 `Dynamic`으로 바뀌는지 보고 3을 반복한다.
5. 끝나면 `CXMR.FollowMarker off`.
6. 결과는 한두 줄이면 된다.

## 개정 이력

- 2026-10-14: 처음 작성 (Claude). 박스 따라가기 큰 기능(면·여러 마커)을 썼다가 지운 뒤, 사용자 의도("메시가 박스를 따라가는지 확인")에 맞춰 진단 명령으로 줄임
