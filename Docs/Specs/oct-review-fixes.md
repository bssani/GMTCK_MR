# 10월 머지 작업 리뷰 반영 + 캘리브레이션 정확도

- 베이스 브랜치: `main`
- 상태: 머지함 (`61a1320`)

## 목적

1. 2026-10-06~08 작업(운전자 눈 정렬, 차량 그룹 공용 정렬, 디자인 옵션, USB 손 접촉, 새 컨트롤 창)은 리뷰 없이 `main`에 머지됐다. 그 사후 리뷰 [Docs/Reviews/oct-review-fixes.md](../Reviews/oct-review-fixes.md) 1회차의 `열림` 항목을 반영한다.
2. 사용자 헤드셋 보고(2026-10-14): "차를 (10, 5, 10)에 맞추고 저장한 뒤 다시 불러오면 (9.87, 4.91, 10.24)처럼 어긋난다." 저장·복원 정확도를 올린다. 사용자 결정 U-1·U-2를 함께 넣는다.

## 범위

리뷰 1회차 항목(문제·제안·확인할 시험은 리뷰 파일에 있다):

1. R1-1 정렬 저장을 캡처 동안의 평균으로 — **가장 급하다**
2. R1-2 `FindPlacement` 캐시
3. R1-3 차량별 저장 파일을 거부할 때 사유 안내
4. R1-4 캡처 취소 때 위치 잠금 되돌리기
5. R1-5 저장·복원 진단 로그(마커 월드 좌표 포함), `CXMR.DumpMarkers` 콘솔 명령 등록

사용자 결정(2026-10-14) — 아래 절에 요구가 있다:

6. U-1 정한 보정 마커가 **전부** 보여야 보정·저장·복원
7. U-2 트래킹 원점이 그대로면 저장한 차량 월드 위치를 그대로 재사용

에디터 작업은 없다.

### U-1 정한 마커가 전부 보여야 한다

"필요 마커"는 마커 프로필에서 `Role == Calibration`이고 `MarkerId > 0`인 마커 전부다(`GetRequiredAlignmentMarkerCount`, `CXMRPlacementAlignment.cpp:68`). 저장된 정렬을 복원할 때는 저장된 마커 집합(`SavedAlignmentMarkers`)이다. 지금은 파일을 읽을 때 프로필 집합과 같은지 검사한다(`CXMRPlacementPersistence.cpp:286-290`).

- **보정(처음)**: 필요 마커가 전부 지금 관측 중일 때만 계산·정착(`CalibrationSettleSeconds`)·완료한다. 정착이 끝날 때 다시 전부 관측 중이어야 완료한다. 지금은 `MinMarkersToCalibrate`(기본 2)라는 별도 숫자를 쓴다(`CXMRPlacementComponent.h:101`, `.cpp:408`). 이 프로퍼티는 없애고 필요 마커 수를 쓴다.
- **복원(저장된 정렬)**: 저장된 마커가 전부 관측 중일 때만 복원한다. 지금 있는 "1개만 보이면 놓고 확인 필요" 경로는 없앤다: `bRestoreNeedsConfirmation`(`.cpp:481`), `ConfirmRestoredAlignment`(`CXMRPlacementAlignment.cpp:269`), 그 패널 버튼과 문구.
- **다 보이기 전에는 차를 움직이지 않는다.** 지금은 마커 1개로 임시 배치한다(`.cpp:387-403`, `RecomputeCalibration`의 `ApplyPlacement`). 이 동작을 없앤다. 다만 키 조정·눈 정렬처럼 사용자가 직접 옮기는 것은 그대로다.
- **저장**: 정렬 파일을 쓰는 모든 경로가 필요 마커 전부를 요구한다. 캡처 저장(`SaveAlignment`)은 이미 전부를 요구한다. 차량별 저장(`SaveMarkerOffsetToProfile`, `.cpp:1350-1397`)은 지금 1개만 보여도 저장하고 안 보인 마커는 계산으로 옮겨 적는다. 이 "옮겨 적기"를 없애고 전부 보일 때만 저장한다. `LearnMarkerLayout`(`.cpp:773`)도 등록된 필요 마커가 전부 보일 때만 쓴다(미등록 마커 추가는 그대로).
  - **적용 범위 (Q3 답, 2026-10-15):** 이 조건은 **관측으로 정렬을 기록하는 쓰기**에만 적용한다. 캡처 저장, 차량별 조정 저장(`SaveMarkerOffsetToProfile`), `LearnMarkerLayout`이 해당한다. 원본 초기화(`TryResetCalibrationToAuthored`, `CXMRPlacementComponent.cpp:899-908`)는 관측 없이 원본 오프셋을 쓰는 것이라 제외하고 지금 동작을 그대로 둔다. 그 파일에는 U-2의 `worldPose`·`markerWorld`를 쓰지 않는다. 그래서 다음 복원은 마커 기준이다. 기존 공용 파일 fallback(`CXMRPlacementPersistence.cpp:207-209`)도 바꾸지 않는다.
- **저장은 한 번 누르기로 한다.** 지금은 "확정 → 마커 관측 → 다시 눌러 저장" 두 단계다(`CXMRPlacementAlignment.cpp:206-221`). 한 번 누르면 캡처를 시작하고, 마커마다 평균(R1-1 최소 표본)이 끝나면 자동으로 저장한다. 그 사이 필요 마커가 하나라도 빠지면 **저장 요청 전체를 취소**한다. 빠짐은 Lost 이벤트와 0.5초 관측 만료를 똑같이 본다. 캡처 표본을 비우고, 잠금은 R1-4대로 시작 전 상태로 돌린다. 빠진 마커 ID를 안내한다. 다시 저장하려면 다시 누른다(Q3 답, 2026-10-15). **캡처 시간 제한:** 정해진 시간 안에 모든 필요 마커가 최소 표본을 채우지 못하면 같은 방식으로 취소하고, 표본이 모자란 마커 ID를 안내한다. 다음 명세 [control-window.md](control-window.md)의 Save alignment 버튼이 이 동작을 전제로 한다. 지금 패널의 확정·저장 버튼은 같은 동작을 하게만 바꾸고 배치는 그대로 둔다.
- **Marker Follower가 쓰는 마커는 차량 보정에서 뺀다.** `ACXMRMarkerFollower`(0번 작업, `marker-follower` 리뷰 1회차)가 레벨에 있으면, 그 `MarkerId`를 다음에서 뺀다: `LearnMarkerLayout`이 미등록 마커를 보정 마커로 더하는 것(`CXMRPlacementComponent.cpp:799-830`), "N in view" 관측 수(`CurrentMarkerCount`, `:186-193`), U-1 필요 마커. 지금은 박스가 보이는 채로 Learn을 누르면 움직이는 마커가 차 배치에 들어간다.
- **필요 마커가 1개인 프로필**은 그대로 허용한다. 그 1개가 보이면 지금처럼 마커 방향까지 써서 놓는다.
- 안내: 기다리는 동안과 저장이 거부될 때 **빠진 마커 ID**를 문구에 넣는다(예: `Waiting for markers: 2 of 3 in view (missing 7)`). 패널 배치는 바꾸지 않는다. 기존 문구 자리(상태·메시지)에 넣는다.

### U-2 원점이 그대로면 저장한 월드 위치를 재사용

마커가 물리적으로 고정돼 있어도, 차의 월드 좌표가 같은 실제 자리를 가리키는지는 헤드셋 트래킹 원점에 달려 있다. 원점이 세션 사이에 그대로면 저장한 월드 위치가 가장 정확하다(평균·계산 오차가 없다). 원점이 움직였으면 지금처럼 마커 기준으로 계산한다.

- **저장**: 정렬 파일을 쓸 때 다음 두 필드를 더 쓴다.
  - `worldPose`: 저장 순간 차량 Root의 월드 위치·회전(`x y z pitch yaw roll`, cm·deg).
  - `markerWorld`: 필요 마커마다 `id`와 저장에 쓴 **평균** 월드 위치(`x y z`). R1-1 캡처 평균과 같은 표본을 쓴다.
- **복원**: 필요 마커가 전부 관측되고 정착 평균이 끝나면, 마커마다 지금 평균 위치와 `markerWorld` 위치의 거리를 잰다.
  - 최대 거리가 `OriginReuseTolerance`(cm, 새 프로퍼티) 이하이면 원점이 그대로라고 보고 **`worldPose`를 그대로** 차 위치로 쓴다.
  - 넘으면 지금처럼 마커 기준으로 계산한다.
  - 둘 중 무엇을 썼는지와 최대 거리를 R1-5 로그와 상태 문구에 남긴다. 예: `Restored saved world pose (origin unchanged, max 0.12 cm)`, `Restored from markers (origin moved 1.40 cm)`.
- 필요 마커가 1개인 프로필은 위치 거리에 더해 yaw 차이도 `OriginReuseYawTolerance`(deg, 새 프로퍼티) 이하일 때만 재사용한다. 마커 하나로는 그 마커를 중심으로 원점이 돈 경우를 위치만으로 알 수 없기 때문이다.
- 새 필드가 없는 기존 파일은 지금처럼 마커 기준으로 복원한다.
- 같은 세션 안 차량 교체(`bKeepSharedPose`, `.cpp:695-707`)는 지금 동작 그대로다.

## 범위 밖

- 리뷰의 보류 H-1 ~ H-5. 단 H-1(관측 0.5초 만료)은 U-1의 "지금 관측 중" 판정에 그대로 쓴다. 판정 방식은 바꾸지 않는다.
- `main`에서 USB 기능을 빼는 일 — 별도 명세 [main-basic.md](main-basic.md)
- 컨트롤 창 레이아웃·디자인 변경 — 사용자와 시안을 정한 뒤 별도 명세로 한다
- 애셋(`.uasset`·`.umap`) 변경

## 지켜야 할 조건

- **저장 파일의 기존 필드와 경로는 바꾸지 않는다.** 기존 필드: `alignmentVersion` 1, `alignmentMarkers`, `primaryMarker`, `markers[]`의 `id`·`x`…`roll`, `vehicleProfile`·`vehicleOffset`·`vehicleClass`, 그룹 식별 정보. U-2의 `worldPose`·`markerWorld`는 **선택 필드로 더한다.** 지금 있는 저장 파일이 수정 뒤에도 마커 기준 복원으로 읽혀야 한다.
- 파일을 거부하는 조건(R1-3)은 바꾸지 않는다. 안내만 더한다.
- 보정 중 평균(`MarkerSamples`, `bAverageMarkerSamples`)의 방식은 그대로다. R1-1은 캡처 전용으로 따로 쌓는다.
- freeze 모드(`bFreezeAfterCalibration=true`, 기본값)의 동작은 그대로다.
- 조작 키는 그대로다. UI 문구는 영어로 쓴다. 패널 배치는 바꾸지 않는다. U-1에서 없애는 복원 확인 버튼만 뺀다.
- 기존 `CXMR.` 시험 78개는 통과해야 한다. U-1 때문에 기대값이 바뀌는 시험(1개 마커 임시 배치, 복원 확인, `MinMarkersToCalibrate`)은 고치고 보고 ③에 목록을 적는다.

## 근거

리뷰 파일의 항목별 경로:줄. 주요 위치:

- 평균: `CXMRPlacementComponent.h:132`(`bAverageMarkerSamples` 툴팁 — 한 프레임 노이즈), `:233-250`(`FMarkerSamples`, `AccumulateSample`), `CXMRPlacementComponent.cpp:245`, `:318-351`
- 보정 계산: `CXMRPlacementComponent.cpp:363-490`(`ComputeCalibrationPose`, `RecomputeCalibration`, `FinishCalibration`), 저장 정렬 복원 `CXMRPlacementAlignment.cpp:223-267`
- 캡처·저장: `CXMRPlacementAlignment.cpp:60-221`, 차량별 저장 `CXMRPlacementComponent.cpp:1328-1397`
- 파일 쓰기·읽기: `CXMRPlacementPersistence.cpp`(쓰기 `:78` 근처 `WriteCalibrationData`, 읽기 `:193-306`)
- 캐시 선례: `CXMRControlPanelWidget.cpp:184-196`
- 마커 이벤트 좌표: 플러그인이 이미 월드 좌표로 준다(`Docs/Varjo-Capabilities.md` §5-1, `VarjoMarkersPlugin.cpp:86-98`). `GetTrackingToWorldTransform`에 pawn 위치가 들어가므로 pawn은 움직이지 않는다는 전제다(§5-2 5번).

## 구현자가 정할 것

- R1-1: 마커당 최소 표본 수의 기본값과 프로퍼티 이름. 캡처 시간 제한의 기본값과 프로퍼티 이름(예: `CalibrationSettleSeconds`의 몇 배). 보고 ③에 고른 값과 이유를 적는다.
- R1-2: 캐시 무효화 조건의 세부.
- R1-3: 안내 문구와 사유 구분 방식.
- U-2: `OriginReuseTolerance` 기본값은 **0.3 cm**, `OriginReuseYawTolerance`는 **0.3 deg**로 시작한다. 사용자가 R1-5 로그를 보고 다시 정한다. 프로퍼티 이름과 JSON 필드 안의 세부 구조는 구현자가 정한다.

## 경계 상황

- R1-1: 캡처 시간 제한 안에 한 마커만 표본이 모자람 → 저장 요청 취소 + 그 마커 ID와 모자란 표본 수 안내. 캡처 중 필요 마커가 Lost 또는 0.5초 넘게 끊김 → 저장 요청 전체 취소(U-1 "저장은 한 번 누르기", Q3 답 2026-10-15). 그 마커 표본만 버리고 계속 모으지 않는다. 캡처 중 차량 교체·차 이동 → 지금처럼 취소되고 표본도 비워짐.
- R1-2: PIE를 다시 시작함, 차량 액터를 다시 로드함, Placement 액터가 파괴됨 → 캐시가 무효화되고 다시 찾음.
- R1-3: 사유가 겹침(모델 변경 + 마커 추가) → 하나 이상 보이면 된다. 저장 파일이 없음 → 지금처럼 메시지 없음(차량별), 그룹이면 기존 `NoGroupSave` 그대로.
- R1-4: 캡처가 저장으로 끝남 → 잠금 유지. 캡처가 취소됨 → 시작 전 상태로. 시작 전부터 잠겨 있었음 → 취소해도 잠금 유지.
- U-1: 정착 시간 동안 마커 하나가 끊김 → 정착 타이머를 멈추고 다시 전부 보이면 처음부터. 필요 마커 3개 중 2개만 계속 보임 → 차가 움직이지 않고 빠진 ID 안내. 프로필에 필요 마커가 0개 → 지금처럼 보정 안 함, 안내.
- U-2: 원점이 그대로지만 마커 하나만 허용 오차를 살짝 넘음 → 마커 기준으로 계산(재사용 안 함). `worldPose`는 있는데 `markerWorld`가 없거나 마커 집합이 다름 → 재사용 안 함. 재사용 뒤 사용자가 키로 옮김 → 지금처럼 수동 정렬, 다시 저장해야 함. 그룹 파일 → 그룹의 Root 위치 하나를 재사용(차량별 `VehicleRootOffset`은 그대로 적용됨).

## 완료 조건

1. R1-1 ~ R1-5가 리뷰 파일의 "문제"를 해결하고, 각 항목 상태가 `반영함 (<커밋 제목>)`이다.
2. 리뷰 파일의 "확인할 시험"이 자동화 시험으로 들어가 있고 통과한다.
3. U-1 시험: 필요 마커 2개 중 1개만 관측 → 차가 안 움직이고 보정·저장·복원 모두 안 됨, 문구에 빠진 ID. 2개 관측 → 보정·저장·복원 됨. 필요 마커 1개 프로필 → 1개로 됨.
4. U-2 시험: 저장 → 같은 마커 월드 위치로 복원 → 차 위치가 저장한 `worldPose`와 **정확히** 같다(허용 오차 0.001 cm 수준). 마커 월드 위치를 허용 오차보다 크게 옮겨 복원 → 마커 기준 계산. 새 필드 없는 기존 파일 → 마커 기준 계산.
5. `-DisableAdaptiveUnity` 빌드 성공, CXMR·GMTCK_MR 모듈 경고·오류 0.
6. `CXMR.` 전체 통과.
7. 바이너리 애셋 변경 0(`git status`에 `.uasset`·`.umap` 없음).
8. `Docs/Operating-Guide.md`와 `Docs/Headset-Test-Checklist.md`의 보정·저장·복원 설명을 U-1·U-2에 맞게 고친다(1개 마커 임시 배치·복원 확인 설명 삭제, 원점 재사용 설명 추가).

## 로컬에서 확인할 것

- 빌드·시험: 위 5·6.
- 헤드셋(사용자, 헤드셋 PC): [Headset-Test-Checklist](../Headset-Test-Checklist.md) §3, §5 정렬 확정·저장, §13 차량 공용 정렬.
  1. 차를 맞추고 저장한다.
  2. 껐다 켜기를 세 번 한다. 매번 `CXMR.DumpMarkers`와 복원 로그를 붙여 받는다.
  3. 로그가 "saved world pose"면 저장한 자리와 정확히 같은지, "from markers"면 원점이 얼마나 움직였는지 본다.
  4. 마커 하나를 가려 차가 움직이지 않고 빠진 ID가 표시되는지 본다.

## 개정 이력

- 2026-10-14: 처음 작성 (Claude)
- 2026-10-14: R1-5 추가, R1-1 우선 — 사용자 헤드셋 보고(저장 뒤 복원 위치가 어긋남)
- 2026-10-14: R1-5에 마커 월드 좌표 로그와 `CXMR.DumpMarkers` 추가 — 세션 간 트래킹 원점이 그대로인지 재기 위해
- 2026-10-14: U-1(정한 마커 전부 필요, 다 보이기 전엔 차 안 움직임)·U-2(원점이 그대로면 월드 위치 재사용) 추가 — 사용자 결정
- 2026-10-14: U-1에 "저장은 한 번 누르기" 추가 — 승인된 컨트롤 창 시안이 전제로 함
- 2026-10-14: U-1에 Marker Follower 마커 제외 추가 — marker-follower 리뷰 1회차
- 2026-10-15: Q3 답 — 저장 중 마커 빠짐은 요청 전체 취소(Lost·관측 만료 같음), 캡처 시간 제한 추가, 원본 초기화 저장은 U-1 저장 조건에서 제외
