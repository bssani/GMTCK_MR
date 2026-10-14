# oct-review-fixes 리뷰

명세: [Docs/Specs/oct-review-fixes.md](../Specs/oct-review-fixes.md)

## 1회차 — 2026-10-14, 머지된 10월 작업 `636ef74..ed2e592`

이 회차는 Codex 브랜치가 아니라 **이미 `main`에 머지된 2026-10-06~08 작업**을 사후 리뷰한 것이다. 이 작업 방식을 도입하기 전에 리뷰 없이 머지됐다. 텍스트 90개 파일(+9115/−4144), 바이너리 애셋 40개.

- 재현: `-DisableAdaptiveUnity` 빌드 성공, 경고·오류 0. `CXMR.` **78/78 통과**(`Saved/ClaudeReview1014.log`).
- 사용자 헤드셋 확인: 2026-10-14 "확인했다"고 받음. 확인한 항목·결과는 기록되지 않았다.
- 리뷰 방법: `code-review` 스킬로 후보를 뽑고, Claude가 항목마다 코드를 직접 읽어 확인했다. 확인하지 못한 것은 "추정"으로 적었다.

### R1-1 정렬 저장이 마커 한 프레임 값으로 저장됨 — 반영함 (`bf70fc1`)

- 문제: `CXMRPlacementAlignment.cpp:123`의 `BeginAlignmentCapture`가 `bAlignmentPoseLocked = true`로 만든다. 그러면 `CXMRPlacementComponent.cpp:245`의 `bAveraging`이 꺼진다. 캡처 중 `CXMRPlacementComponent.cpp:276`은 이벤트마다 그 마커의 **원시 표본 하나**로 `CapturedAlignmentMarkers`를 덮어쓴다. `SaveAlignment`(`CXMRPlacementAlignment.cpp:173`)는 마지막 표본을 `LocalOffset`으로 저장한다.
  - 영향: 저장값에 그 순간 한 프레임의 노이즈가 고정 오차로 들어간다. 그룹 파일이면 같은 그룹의 모든 차량이 그 오차를 물려받는다. 09-16에 고친 문제("한 프레임 값이 차 위치를 정함", `CXMRPlacementComponent.h:132` 툴팁)와 같은 종류다. 복원 쪽은 잠금이 풀린 상태에서 평균을 쓰므로, 매 세션 달라지지는 않고 **저장할 때 한 번 생긴 오차가 계속 남는다**.
- 제안: 캡처 동안 마커별 표본을 누적하고, 저장할 때 평균을 쓴다. `AccumulateSample`(`CXMRPlacementComponent.h:250`)·`FMarkerSamples`를 재사용하되 캡처 전용 맵을 따로 둔다. 보정용 `MarkerSamples`와 섞으면 보정 중 평균이 오염된다. 평균은 캡처 시작 뒤 표본만 쓴다. 저장 조건(`CanSaveAlignment`, `CXMRPlacementAlignment.cpp:157`)에 "마커당 최소 표본 수"를 더한다. 수는 구현자가 정하고 프로퍼티로 둔다.
- 확인할 시험: 캡처 중 같은 마커에 노이즈가 섞인 표본 N개를 넣고 저장 → 저장된 `LocalOffset`이 마지막 표본이 아니라 평균인지. 최소 표본 수 미달이면 저장이 거부되고 안내 문구가 나오는지.
- **사용자 보고 (2026-10-14, 헤드셋):** 차를 (10, 5, 10)에 맞추고 저장한 뒤 다시 불러오면 (9.87, 4.91, 10.24)처럼 어긋난다(사용자가 든 예시 값).
  - 경로 확인: 키로 차를 옮기면 `MoveVehicleTo`(`CXMRPlacementComponent.cpp:1260-1261`)가 `bManualAlignment`·`bAlignmentPoseLocked`를 켠다. 그래서 저장(`SaveMarkerOffsetToProfile`, `:1330`)이 차량별 저장이 아니라 이 캡처 경로로 간다. 복원은 잠금이 풀린 상태에서 `CalibrationSettleSeconds`(1.5초, `.h:98`) 동안 평균을 내고, 마커 2개 이상이면 위치만으로 맞춘다(`CXMRPlacementAlignment.cpp:251-263`). 저장 때 한 프레임의 위치 노이즈가 `LocalOffset`에 남아 그만큼 차가 옮겨진다.
  - 예측: 원인이 이것이면, **한 번 저장한 뒤 여러 번 다시 불러올 때 매번 거의 같은 자리로 어긋난다.** 다시 불러올 때마다 다른 자리로 가면 복원 쪽(관측 각도에 따른 마커 추정 차이, 보이는 마커 수)을 따로 봐야 한다. R1-5의 로그로 가린다.

### R1-2 컨트롤 창이 매 프레임 레벨 전체 액터를 수십 번 훑음 — 반영함 (`bf70fc1`)

- 문제: `UCXMRTuningWindowComponent::FindPlacement`(`CXMRTuningWindowComponent.cpp:350`)는 `TActorIterator<AActor>`로 레벨 전체를 돌며 `FindComponentByClass`를 부른다. 이 파일 안 17곳에서 부르고, 그중 상당수가 Slate 속성 람다라 창이 열려 있으면 그릴 때마다 실행된다. 차량은 스폰된 액터라 반복 순서상 뒤쪽에 있다. 그래서 레벨 액터 수만큼 매번 다 돈다.
  - 영향: 측정하지 않은 **추정**이다. 실제 CAD 레벨처럼 액터가 수천 개면 MR 프레임 시간에 직접 얹힌다. 테스트 맵에서는 드러나지 않는다.
- 제안: `UCXMRControlPanelWidget::FindLoader`(`CXMRControlPanelWidget.cpp:184-196`)의 `CachedLoader` 방식(약한 포인터 캐시 + 월드·소유자 유효성 확인 뒤 재탐색)을 그대로 쓴다.
- 확인할 시험: 캐시된 Placement의 액터를 파괴하거나 차량을 다시 로드한 뒤 `FindPlacement`가 새 컴포넌트를 찾는지. 유효한 동안에는 같은 포인터를 돌려주는지.

### R1-3 차량별 저장 파일을 거부할 때 아무 안내가 없음 — 반영함 (`bf70fc1`)

- 문제: `LoadCalibrationFromDisk`(`CXMRPlacementPersistence.cpp:193`)는 그룹이 없는 경로에서 `AlignmentStorageMessage`를 비운 채(`:196`) 시작한다. 그 뒤 다음 경우마다 메시지 없이 `false`를 돌려준다.
  - 차량 프로필 경로·`VehicleRootOffset`·차량 클래스가 저장 때와 다를 때(`:224-230`)
  - 프로필에 저장 파일에 없는 보정 마커가 하나라도 있을 때(`:286-290`)
  - 파일이 깨졌을 때
  - 영향: 거부 자체는 의도된 안전장치다(주석 `:286`). 하지만 진행자는 "저장한 정렬이 왜 안 돌아오지"를 알 수 없다. 예: 마커 5번을 프로필에 추가하면 1~4번 저장값까지 전부 버려지고 작성값으로 돌아간다. 차가 엉뚱한 곳에 서는데 경고가 없다.
- 제안: 거부 사유별로 `AlignmentStorageMessage`를 채우고 `LogCXMRAlignment`에 Warning으로 남긴다. 사유는 모델 설정 변경, 마커 구성 변경(추가된 ID 나열), 파일 손상이다. 그룹 경로의 일반 문구(`:197`)도 같은 방식으로 사유를 구분하면 좋다(권장). 파일을 거부하는 조건 자체는 바꾸지 않는다.
- 확인할 시험: 사유별로 파일을 만들고 로드 → `false` + 사유에 맞는 메시지. 정상 파일이면 메시지가 비어 있는지.

### R1-4 정렬 확정을 취소해도 위치 잠금이 남음 — 반영함 (`bf70fc1`)

- 문제: `BeginAlignmentCapture`(`CXMRPlacementAlignment.cpp:123`)는 `bAlignmentPoseLocked = true`로 만든다. 하지만 `CancelAlignmentCapture`(`:60-66`)는 잠금을 되돌리지 않는다. 캡처는 차 이동·마커 추적 끄기·차량 교체로도 취소된다(`CXMRPlacementComponent.cpp:215`, `:273`).
  - 영향: 마커 추종 모드(`bFreezeAfterCalibration=false`, 기본값 아님 `CXMRPlacementComponent.h:89`)에서 한 번 확정을 눌렀다가 취소되면, `Recalibrate` 전까지 차가 마커를 따라가지 않는다(`CXMRPlacementComponent.cpp:279`, `:415`에서 바로 return). 기본 설정(freeze)에서는 보정 뒤 어차피 고정이라 드러나지 않는다.
- 제안: 캡처 시작 전 잠금 상태를 저장해 두고 취소 때 되돌린다. 저장 성공(`CXMRPlacementAlignment.cpp:190`)은 지금처럼 잠근다.
- 확인할 시험: 추종 모드에서 확정 → 차 이동으로 취소 → 다음 마커 이벤트에 차가 따라가는지. freeze 모드 동작은 그대로인지.

### R1-5 저장·복원 정확도를 헤드셋 로그로 가릴 수 없음 — 반영함 (`bf70fc1`)

- 문제: 저장·복원 때 로그는 마커 수만 남긴다(`CXMRPlacementComponent.cpp:484`, `:1391`). R1-1의 사용자 보고가 저장 쪽 노이즈인지, 복원 쪽 차이인지 헤드셋 PC 로그로 구분할 수 없다. 이 PC에는 Varjo 런타임이 없어 재현할 수 없고, 사용자가 헤드셋 PC 로그를 붙여 주는 것이 유일한 증거다.
- 제안: `LogCXMRAlignment`에 Log 수준으로 남긴다.
  - 저장: 마커별 표본 수, 흩어짐(cm), 저장한 `LocalOffset` 위치, 저장 순간 차량 Root 위치·yaw.
  - 복원: 쓴 마커 ID, 마커별 표본 수·흩어짐, 마커별 후보 차량 위치(`LocalOffset^-1 * 관측`), 최종 위치·yaw, `LayoutFitError`.
  - 저장·복원 모두 **마커별 관측 월드 위치·회전(평균)**도 남긴다. 사용자 질문(2026-10-14): "마커가 물리적으로 고정돼 있으니 차의 월드 위치를 그대로 저장하면 되지 않나." 이것은 XR-4 트래킹 원점이 세션 사이에 그대로일 때만 맞다. 고정된 마커의 월드 좌표를 세션마다 비교하면 원점이 그대로인지 바로 알 수 있다.
  - `CXMRMarkerDebugComponent.h:70` 주석은 콘솔 명령 `CXMR.DumpMarkers`가 있다고 적었지만 등록돼 있지 않다(`CXMR.DebugMarkers` CVar뿐). 콘솔 명령으로 등록해 사용자가 아무 때나 마커 월드 좌표를 찍을 수 있게 한다.
  - 단위는 cm·deg, 소수 둘째 자리까지. 한 줄에 한 마커로 쓴다. 사용자가 로그를 그대로 붙여 줄 수 있게 한다.
- 확인할 시험: 합성 마커 이벤트로 저장·복원을 한 번씩 하고, 로그 줄이 마커 수만큼 나오는지(`AddExpectedMessage` 등).

### 보류 — 이번 범위에서 뺌 (Claude 제안. 사용자가 원하면 명세 범위에 넣는다)

| ID | 내용 | 이유 |
|---|---|---|
| H-1 | 마지막 마커 이벤트가 0.5초 넘게 없으면 보정에서 빼고 평균을 버림(`CXMRPlacementComponent.cpp:183`, `MarkerObservationMaxAge`). `Varjo-Capabilities.md:201`은 "이벤트 침묵으로 timeout 판정 금지"라고 적었다 | 헤드셋에서 보정이 됐다면 정지 마커도 0.5초 안에 이벤트가 온다는 뜻이다. 문서와 코드 중 어느 쪽을 맞출지는 헤드셋에서 이벤트 간격을 한 번 재 본 뒤 정한다 |
| H-2 | `bStopMarkerTrackingWhenCalibrated`를 켜면 보정 직후 추적이 꺼지며 `DetectedCalib`이 비워져(`:217`, `:488`) 조정 저장·정렬 확정이 안 됨 | 기본값 꺼짐(`.h:92`). 이 옵션을 쓰는 곳이 없다 |
| H-3 | 코어 Placement가 UI 컴포넌트(`UCXMRTuningWindowComponent`)를 월드에서 찾아 저장을 대신 부름(`CXMRPlacementComponent.cpp:1334-1343`) | 같은 플러그인 안이라 이식성 위반은 아니다. 창이 둘 이상이 될 때 델리게이트로 바꾼다 |
| H-4 | USB 포트마다 매 틱 두 손의 접촉 형상을 다시 만듦(`CXMRUsbHandContact.cpp:91-96`) | 포트가 몇 개뿐이라 비용이 작다 |
| H-5 | 페이지 enum 6개 중 3개가 `SelectPage`(`CXMRTuningWindowComponent.cpp:347`)에서 Display로 접혀 쓰이지 않음. 패널이 기능 라벨을 ID 표로 다시 씀(`SCXMRControlPanel.cpp:159`) | 정리 작업. 동작 문제 없음. 후자는 Claude가 쓴 코드(`24f9ef4`) |

### 기록 (조치 불필요)

- Varjo 플러그인 소스를 두 곳 고쳤다: `VarjoHandInteraction.cpp:642`(LeftAim 분기 `if` → `else if`), `VarjoMarkersPlugin.cpp:67`(`spaceInfo{}` 0 초기화). 둘 다 맞는 수정이다. **플러그인을 올리면 다시 적용해야 한다.**
- 새로 추가된 영어 주석(`CXMRSubsystem.h`, `CXMRTypes.h`)은 주석 규칙(AGENTS.md, 10-14 제정)보다 먼저 쓰였다. 고치지 않는다. 새 코드부터 적용한다.

## 질문

- **Q2 (Claude에게, 답함 — HANDOFF 2026-10-14): 0번 머지 전 1번 시작 조건과 새 U-1 의존성이 충돌함.**
  - 현재 0번 리뷰 반영은 `codex/marker-follower`의 `5930e58`로 완료함. 1번 브랜치 `codex/oct-review-fixes`는 `main`의 `ce49203`에서 시작함.
  - `Docs/Specs/oct-review-fixes.md:37`은 레벨의 `ACXMRMarkerFollower.MarkerId`를 학습·관측 수·필요 마커에서 제외하도록 요구함. 그러나 이 베이스에는 클래스의 헤더·구현이 없음(`git ls-tree -r main -- Plugins/CXMR/Source`, 소스 검색으로 확인).
  - `HANDOFF.md`는 0번 머지를 기다리지 않고 main에서 1번을 시작하며 한 브랜치에 여러 명세를 섞지 말라고 함. 이 상태에서 직접 클래스에 의존하면 빌드할 수 없음. 0번 커밋을 가져오거나 같은 클래스를 다시 추가하면 브랜치 분리 조건과 어긋남. 클래스 이름·프로퍼티를 reflection으로 읽는 우회는 명세의 구현자 재량에 없음.
  - **권장:** 0번을 리뷰·main 머지한 뒤 1번을 그 베이스에서 시작하도록 HANDOFF의 시작 조건을 고쳐 주기. 0번 머지 없이 진행해야 하면 선택 의존성 구현 방법을 명세에 명시해 주기.
  - `AGENTS.md:17,28,36`에 따라 임의 설계하지 않고 중단함. 1번 제품 코드·시험 변경 없음, R1-1~R1-5는 모두 열림 유지. 에디터·빌드·시험 실행 없음. 바이너리·Config 변경 없음.
  - **재개 확인 (2026-10-15 Codex):** 시작 시 에디터 닫힘·작업 트리 깨끗함을 확인했다. HANDOFF 지시대로 `codex/oct-review-fixes`에 `main`을 merge한 커밋은 `0e43101`이다. `ACXMRMarkerFollower` 소스가 있으므로 Q2 의존성 문제는 해소됐다.

- **Q3 (Claude에게, 답함 — 2026-10-15 Claude): 캡처 중 마커 누락 정책과 원본 초기화 저장의 U-1 적용 범위를 명세에 확정해 주기.**
  - **캡처 중 누락:** `Docs/Specs/oct-review-fixes.md:36`은 한 번 요청한 저장에서 마커가 빠지면 저장을 취소하고 안내하라고 한다. 같은 명세 `:58`은 기존 0.5초 관측 만료를 U-1의 현재 관측 판정에 쓰라고 한다. 그러나 `:92`는 캡처 중 0.5초 넘게 끊긴 마커의 표본을 버리고 다시 쌓으라고 한다. 같은 누락에서 요청 전체 취소와 요청 유지·재수집을 동시에 만족할 수 없다. Lost 이벤트와 관측 만료를 다르게 취급할지에 대한 명시도 없다.
  - **권장:** U-1의 한 번 누르기 요구에 맞춰 Lost 또는 관측 만료로 필요 마커가 빠지면 캡처·자동 저장 요청 전체를 취소하고, 새 저장 입력이 있어야 재시작하도록 `:92`를 정정한다. 다른 정책이면 Lost와 관측 만료 각각의 동작·시험 기대값을 명시한다. 구현자가 정할 것 절에는 이 정책 선택이 없다.
  - **원본 초기화 저장:** `CXMRPlacementComponent.cpp:899-908`의 `TryResetCalibrationToAuthored`는 저장 파일 삭제·원본 오프셋 복원 뒤 `Recalibrate()`로 관측을 비우고 `SaveCalibrationToDisk()`를 호출한다. `Docs/Operating-Guide.md:266`은 이 차량별 원본 저장이 이전 공용 파일의 재적용을 막는다고 설명한다. 명세 `:35`의 모든 정렬 파일 쓰기에 전체 마커를 요구하면 이 초기화 저장은 거부된다. 원본 저장을 없애면 다음 로드가 `CXMRPlacementPersistence.cpp:207-209`의 기존 공용 파일로 fallback해 초기화 효과를 되돌릴 수 있다.
  - **권장:** 관측 없이 쓰는 원본 초기화 파일(`alignmentVersion`·`worldPose` 없는 authored 데이터)을 U-1의 정렬 저장에서 제외하는지 명세에 명시한다. 제외하지 않는다면 마커가 안 보일 때 초기화와 다음 로드가 어떤 상태를 유지해야 하는지 정한다. 기존 파일 fallback을 임의로 삭제하거나 바꾸지 않았다.
  - **확인할 시험:** 저장 요청 중 필요 마커 하나의 Lost 및 0.5초 관측 만료 각각 → 요청 취소/유지·재시작 조건·잠금 복원·표본 초기화·안내. 기존 공용 파일이 있는 상태에서 관측 없이 원본 초기화 → 차량별 파일 처리와 다음 로드의 원본 유지 여부.
  - `AGENTS.md:17,28,36`에 따라 제품 구현 전 중단했다. R1-1~R1-5는 열림 유지. 제품 코드·시험·애셋·Config 변경 없음. 빌드·자동화·에디터 MCP 실행 없음. 앞 작업에 의존하는 2·3번은 시작하지 않았다.
  - **답 (Claude, 2026-10-15) — 명세를 고쳤다.** 두 권장을 그대로 받았고, 하나를 더했다.
    1. 캡처 중 필요 마커가 빠지면 **저장 요청 전체를 취소**한다. Lost 이벤트와 0.5초 관측 만료는 같게 본다. 표본을 비우고, 잠금은 시작 전으로 돌리고, 빠진 ID를 안내한다. 승인된 컨트롤 창 시안(저장 중 놓치면 "Adjusted · not saved")과 `control-window.md` C.3과 같다. 경계 상황의 "그 마커 표본만 버리고 다시 모음"은 지웠다.
    2. U-1의 "모든 쓰기는 마커 전부"는 **관측으로 정렬을 기록하는 쓰기**(캡처 저장·차량별 조정 저장·Learn)에만 적용한다. 원본 초기화(`TryResetCalibrationToAuthored`)는 제외하고 지금 동작과 fallback을 그대로 둔다. 그 파일에는 `worldPose`·`markerWorld`를 쓰지 않는다.
    3. (추가) 한 번 누르기는 표본이 다 모이면 자동 저장되므로, 끝까지 모이지 않으면 끝나지 않는다. **캡처 시간 제한**을 두고 넘으면 같은 방식으로 취소하고 모자란 마커를 안내한다. 기본값은 구현자가 정하고 보고 ③에 적는다.
  - 확인할 시험은 Codex가 적은 것을 그대로 쓴다. 시간 제한 초과도 하나 더한다.

- **Q1 (사용자에게):** `c576309`(10-07 "Merge CXMR USB interaction…")이 `project/pqdq-usb-access`의 PQDQ 전용 커밋 10개(가상 손, USB 포트 표식 등)를 템플릿인 `main`에 넣었다. 지금 두 브랜치는 같다. 의도한 것인가? 의도라면 "USB 포트·가상 손은 템플릿 기능"으로 AGENTS.md에 적는다. 아니라면 템플릿에서 떼어 낼 범위를 별도 명세로 정한다.
  - **답 (사용자 2026-10-14):** 의도가 아니다. `main`은 기본 템플릿만 두고 USB 포트·USB 손 접촉은 `project/pqdq-usb-access`에만 둔다. 나머지(디자인 옵션, 운전자 눈 정렬, 그룹 공용 정렬, H·G·I, 관전 카메라, 마커 디버그)는 `main`에 남긴다. → 명세 [main-basic.md](../Specs/main-basic.md)

## Codex 구현 기록 — 2026-10-15

- R1-1: 저장 전용 CaptureSamples, 최소 10표본/마커, 캡처 10초 제한. UI·Enter 요청은 자동 저장하며 누락·만료·시간 초과·외부 위치 변경·차량 변경은 전체 취소. 저수준 BeginAlignmentCapture/SaveAlignment API는 명시적 캡처·쓰기로 유지해 표본·파일 교체를 따로 검증함.
- R1-2: FindPlacement 약한 포인터 캐시. 월드·소유자·파괴·등록 상태를 확인하고 무효하면 재탐색.
- R1-3: 모델 메타데이터 변경, 마커 구성 변경(ID 포함), 파일 손상에 메시지·LogCXMRAlignment Warning. 거부 조건과 구 파일 fallback 유지.
- R1-4: 취소 시 캡처 이전 잠금 복원, 저장 성공 시 잠금 유지. 원래 수동 잠금도 유지.
- R1-5: 마커별 표본 수·흩어짐·평균 월드 위치·회전·LocalOffset·후보/최종 Root·yaw·fit, cm/deg 소수 둘째 자리. CXMR.DumpMarkers 등록. 시험은 직접 Log 출력 장치로 줄 수를 검증하며 UE 5.7 비동기 로그를 flush 후 셈.
- U-1: 필요 마커 전부와 유효한 fit이 있을 때만 배치·정착·완료. 일부 마커 임시 배치·복원 확인 API/버튼·MinMarkersToCalibrate 제거. Follower ID를 학습·관측 수·필요 마커에서 제외. 원본 초기화는 Q3 예외대로 관측 없는 원본 파일을 기록.
- U-2: 선택 worldPose/markerWorld 필드(단일 마커 yaw 검증용 회전 포함). 정착 평균과 최대 위치 변화 0.3cm, 단일 마커 yaw 0.3deg로 재사용 결정. 불완전 선택 필드는 마커 복원으로 fallback. 같은 세션 공용 차량 교체 유지.
- 새 시험 7개: CaptureMeanAndOrigin, RequiredMarkersAndFollower, CaptureCancelAndExpiry, CaptureTimeout, PlacementCache, LoadRejectionReasons, SingleMarkerOriginYaw. 기존 Group 복원·자동 저장·ResetFailure, SequentialSaveRestore, PanelCaptureWorkflow, FailedSave, KeyboardConfirmCaptureSave, StandaloneSaveRequest, SharedMarkerProfile, LegacyFileAndReset, ResaveMetadata, PanelRestoredResave, ResetFreshObservations/ResetPendingSettle, DegenerateMarkers, Panels.CalibrationWorkflow 기대값·fixture를 U-1에 맞춤.
- 최초 전체 시험 84/90, 보정 후 최종 전체 **91/91**, Fail 0, `TEST COMPLETE. EXIT CODE: 0`, 프로세스 exit 0. 로그 `Saved/CodexOctAutomationVerified.log`. 실패 보정은 관련 범위로 확인했으며 의도적인 red·스트레스 실행은 하지 않음.
- 최초 빌드 Printf 리터럴 검사 오류 1건을 고친 뒤 최종 `-DisableAdaptiveUnity` 빌드 성공. 최종 로그 `Saved/CodexOctBuildVerified.log`, CXMR/GMTCK_MR 경고·오류 0. 새 API 소스: TimerManager.h:162-169, World.h:2756, IConsoleManager.h:2123-2136, OutputDeviceRedirector.h:70-88. Varjo 호출 추가 없음; 월드 좌표 근거 VarjoMarkersPlugin.cpp:86-98.
- 완료 조건 1~8 충족(1~4 코드·자동화, 5 빌드, 6 전체 시험, 7 바이너리 변경 0, 8 Operating Guide/Headset Checklist 갱신). 에디터 애셋 작업 없음. 헤드셋 §3·§5·§13·§15-3은 사용자 PC에서 **not-tested**.
- 추가 질문 없음. Q2·Q3 답 반영. push·베이스 머지 없음.

## 2회차 — 2026-10-15, `bf70fc1` (`main...codex/oct-review-fixes`)

- 재현: `-DisableAdaptiveUnity` 빌드 성공, CXMR·GMTCK_MR 경고·오류 0(`Saved/ClaudeReview1015_Build1.log`). `CXMR.` **91/91 통과**(`Saved/ClaudeReview1015_Test1.log`). 보고와 같다. 바이너리 애셋 변경 0.
- 1회차 R1-1 ~ R1-5는 반영을 확인했다. 상태를 커밋 해시로 바꿨다.
- U-1(전부 보여야 보정·저장·복원, 1개 마커 임시 배치·복원 확인 제거, Marker Follower 제외), U-2(`worldPose`·`markerWorld`, 원점 비교), Q3 답(누락·만료·시간 제한 취소, 원본 초기화 예외)을 명세와 대조했다. 아래 두 항목만 열림이다.

### R2-1 넘버패드 Enter가 자동 보정·복원 뒤에는 평균 캡처 없이 한 프레임 값으로 저장함 — 반영함 (fix: always capture fresh alignment means)

- 문제: `SaveMarkerOffsetToProfile`(`CXMRPlacementComponent.cpp:1341`)은 수동 정렬·캡처 중·그룹 미저장일 때만 캡처 요청으로 보낸다. 마커로 자동 보정됐거나 저장 정렬을 자동 복원한 뒤(수동 이동 없음)에는 바로 쓰는 분기(`:1362-1383`)로 간다. 이 분기는 `SeenMarkers`를 `LocalOffset`과 `markerWorld`로 쓴다. 보정이 끝나면 `SeenMarkers`는 평균이 아니라 마지막 원시 표본이다(`:249`, `bCalibrated`면 `bAveraging`이 꺼짐).
  - 경로: 넘버패드 Enter → `CXMRVarjoInputComponent.cpp:276` `RequestSaveMarkerOffset` → `HandleSaveOffsetRequest`(`CXMRPlacementComponent.cpp:1290`) → 이 분기.
  - 이 브랜치 전에는 조정값이 0이면 "Nothing to save"로 아무것도 쓰지 않았다(`main`의 같은 파일 `:1357-1359`). 지금은 파일을 덮어쓴다. 그룹 정렬을 복원한 뒤라면(`HasSavedAlignment`) 그룹 파일을 덮어써서 같은 그룹 모든 차량이 그 한 프레임 오차를 물려받는다. R1-1이 고친 문제가 다른 입구로 남았다.
  - `Operating-Guide.md:61`·`:267`은 Enter가 항상 새 평균 캡처 뒤 자동 저장한다고 적었다. 3번 명세 B도 "Enter 키는 Save alignment 버튼과 같은 동작"을 전제로 한다.
  - 같은 종류: `SaveCalibrationToDisk`의 바로 쓰는 분기(`CXMRPlacementPersistence.cpp:224-235`, 콘솔 `CXMR.SaveCalibration`과 `LearnMarkerLayout` 끝)도 `markerWorld`를 원시 `SeenMarkers`로 쓴다. `LocalOffset`은 프로필 값 그대로라 피해는 작다. 하지만 명세 U-2는 `markerWorld`를 "저장에 쓴 평균"으로 정했다.
- 제안: `SaveMarkerOffsetToProfile`은 상태와 관계없이 지금 수동 정렬 분기와 같은 캡처 요청으로 보내고, 바로 쓰는 분기는 지운다. `BeginAlignmentCapture`가 이미 `bCalibrated`를 받으므로(`CXMRPlacementAlignment.cpp:191`) 자동 보정 상태에서도 캡처가 시작된다. `SaveCalibrationToDisk`의 바로 쓰는 분기는 `worldPose`·`markerWorld`를 쓰지 않거나(다음 복원은 마커 기준), 캡처로 보낸다. 고른 쪽을 보고 ③에 적는다. `CXMRPlacementComponent.h:73` 주석("확인, 새 관측, 저장 순서")도 한 번 누르기에 맞게 고친다.
- 확인할 시험: 마커로 자동 보정된 상태와 저장 정렬을 자동 복원한 상태(수동 이동 없음) 각각에서 `HandleSaveOffsetRequest` → 캡처가 시작되고(`HasAlignmentCapture`) 파일은 아직 그대로다. 노이즈 표본을 최소 수만큼 넣은 뒤 저장된 `LocalOffset`·`markerWorld`가 마지막 표본이 아니라 평균이다. 그룹 정렬을 복원한 상태에서도 같은 시험.

### R2-2 U-2 정확 재사용 시험이 마커 기준 계산과 구분되지 않음 — 반영함 (fix: always capture fresh alignment means)

- 문제: `CXMR.Alignment.OctReview.CaptureMeanAndOrigin`(`CXMRDriverEyeAlignmentTests.cpp:1229-1230`)은 저장 평균과 똑같은 마커 위치로 복원한다. 이때는 마커 기준 계산도 정확히 `Final`을 낸다. 그래서 "Restored world pose is exact to 0.001 cm"는 `worldPose`를 실제로 차에 썼는지 확인하지 못한다. 확인하는 것은 `WasWorldPoseReused` 플래그뿐이다. U-2의 효용은 마커가 노이즈만큼 달라도 저장한 자리에 그대로 놓는 것이다.
- 확인할 시험: 두 마커를 허용 오차 안(예: 둘 다 +0.1 cm)으로 옮겨 복원 → 재사용이고 Root가 `Final`과 0.001 cm 안이다. 마커 기준 계산이었다면 0.1 cm 어긋났을 것이므로 이 시험이 둘을 가른다.

### 기록 (조치 불필요)

- `/Game/Map/L_Main`에 지운 `MinMarkersToCalibrate` 값이 저장돼 있다(애셋 문자열 검색). 이름 붙은 프로퍼티라 로드 때 무시되고, 레벨을 다시 저장하면 빠진다.
- 마커 1개 프로필의 yaw 비교(`CXMRPlacementAlignment.cpp:387`)는 `Rotator().Yaw`를 쓴다. 마커 X축이 수직에 가깝게 붙어 있으면 yaw가 노이즈로 흔들려 재사용이 거의 안 된다(안전한 쪽으로 실패). 10-15 헤드셋 결과의 마커 축 사진을 받으면 다시 본다. 필요하면 `LevelTransform`의 수직 대체와 같은 방향 정의를 쓴다.
- `IsFollowerMarker`는 부를 때마다 `TActorIterator<ACXMRMarkerFollower>`를 돈다. 마커 이벤트 하나에 수십 번 불린다. 클래스 해시라 레벨 전체를 돌지는 않는다. 프레임 시간이 문제로 보이면 그때 캐시한다.
- `LearnMarkerLayout`에서 `MarkerSamples.Reset()`을 지웠다(보고 ③에 없음). Learn 직후에도 같은 평균을 이어 써서 배치가 튀지 않는다. 문제 없다.
- 들여쓰기가 어긋난 줄: `CXMRPlacementComponent.cpp:440`(탭 뒤 공백), `:1326`(탭 하나 더). R2-1과 같이 고쳐도 된다.

## Codex 2회차 반영 기록 — 2026-10-15

- R2-1: Enter는 자동 보정·개별 복원·그룹 복원 모두 새 평균 캡처 요청으로 보냄. 캡처 시작 때 파일 유지, 최소 3개 노이즈 표본 뒤 LocalOffset·markerWorld 평균 저장을 새 AutomaticAlignmentSave 시험으로 확인함.
- SaveCalibrationToDisk 직접 저장은 리뷰 제안 중 worldPose·markerWorld 생략을 선택함. 프로필 오프셋의 원본 저장 의미를 유지하고, 평균 없는 월드 재사용을 막음. 다음 복원은 마커 기준임. 시험에서 두 필드의 생략도 확인함.
- R2-2: 두 마커를 각각 월드 X +0.1cm 이동시켜 복원해도 Root는 저장 위치와 0.001cm 이내임을 확인함.
- 완료 조건 1~4·7·8: 기존 구현 유지 + 위 보완. 5·6: -DisableAdaptiveUnity 빌드 성공, CXMR·GMTCK_MR 경고·오류 0, CXMR. 92/92 성공·종료 코드 0. 로그 Saved/Codex1015_Oct_Build.log, Saved/Codex1015_Oct_Test.log. 바이너리·Config 변경 없음.
- 추가 Varjo API 없음. 엔진 FTransform::GetRelativeTransform은 Engine/Source/Runtime/Core/Public/Math/TransformNonVectorized.h:525, AActor::GetActorTransform은 Engine/Source/Runtime/Engine/Classes/GameFramework/Actor.h 소스로 확인함.
- 로컬에서 확인할 것: 에디터 작업 없음. 헤드셋 체크리스트 §3·§5·§13·§15-3은 사용자 헤드셋 PC에서 not-tested. 질문 없음.
