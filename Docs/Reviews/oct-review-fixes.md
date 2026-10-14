# oct-review-fixes 리뷰

명세: [Docs/Specs/oct-review-fixes.md](../Specs/oct-review-fixes.md)

## 1회차 — 2026-10-14, 머지된 10월 작업 `636ef74..ed2e592`

이 회차는 Codex 브랜치가 아니라 **이미 `main`에 머지된 2026-10-06~08 작업**을 사후 리뷰한 것이다. 이 작업 방식을 도입하기 전에 리뷰 없이 머지됐다. 텍스트 90개 파일(+9115/−4144), 바이너리 애셋 40개.

- 재현: `-DisableAdaptiveUnity` 빌드 성공, 경고·오류 0. `CXMR.` **78/78 통과**(`Saved/ClaudeReview1014.log`).
- 사용자 헤드셋 확인: 2026-10-14 "확인했다"고 받음. 확인한 항목·결과는 기록되지 않았다.
- 리뷰 방법: `code-review` 스킬로 후보를 뽑고, Claude가 항목마다 코드를 직접 읽어 확인했다. 확인하지 못한 것은 "추정"으로 적었다.

### R1-1 정렬 저장이 마커 한 프레임 값으로 저장됨 — 열림

- 문제: `CXMRPlacementAlignment.cpp:123`의 `BeginAlignmentCapture`가 `bAlignmentPoseLocked = true`로 만든다. 그러면 `CXMRPlacementComponent.cpp:245`의 `bAveraging`이 꺼진다. 캡처 중 `CXMRPlacementComponent.cpp:276`은 이벤트마다 그 마커의 **원시 표본 하나**로 `CapturedAlignmentMarkers`를 덮어쓴다. `SaveAlignment`(`CXMRPlacementAlignment.cpp:173`)는 마지막 표본을 `LocalOffset`으로 저장한다.
  - 영향: 저장값에 그 순간 한 프레임의 노이즈가 고정 오차로 들어간다. 그룹 파일이면 같은 그룹의 모든 차량이 그 오차를 물려받는다. 09-16에 고친 문제("한 프레임 값이 차 위치를 정함", `CXMRPlacementComponent.h:132` 툴팁)와 같은 종류다. 복원 쪽은 잠금이 풀린 상태에서 평균을 쓰므로, 매 세션 달라지지는 않고 **저장할 때 한 번 생긴 오차가 계속 남는다**.
- 제안: 캡처 동안 마커별 표본을 누적하고, 저장할 때 평균을 쓴다. `AccumulateSample`(`CXMRPlacementComponent.h:250`)·`FMarkerSamples`를 재사용하되 캡처 전용 맵을 따로 둔다. 보정용 `MarkerSamples`와 섞으면 보정 중 평균이 오염된다. 평균은 캡처 시작 뒤 표본만 쓴다. 저장 조건(`CanSaveAlignment`, `CXMRPlacementAlignment.cpp:157`)에 "마커당 최소 표본 수"를 더한다. 수는 구현자가 정하고 프로퍼티로 둔다.
- 확인할 시험: 캡처 중 같은 마커에 노이즈가 섞인 표본 N개를 넣고 저장 → 저장된 `LocalOffset`이 마지막 표본이 아니라 평균인지. 최소 표본 수 미달이면 저장이 거부되고 안내 문구가 나오는지.
- **사용자 보고 (2026-10-14, 헤드셋):** 차를 (10, 5, 10)에 맞추고 저장한 뒤 다시 불러오면 (9.87, 4.91, 10.24)처럼 어긋난다(사용자가 든 예시 값).
  - 경로 확인: 키로 차를 옮기면 `MoveVehicleTo`(`CXMRPlacementComponent.cpp:1260-1261`)가 `bManualAlignment`·`bAlignmentPoseLocked`를 켠다. 그래서 저장(`SaveMarkerOffsetToProfile`, `:1330`)이 차량별 저장이 아니라 이 캡처 경로로 간다. 복원은 잠금이 풀린 상태에서 `CalibrationSettleSeconds`(1.5초, `.h:98`) 동안 평균을 내고, 마커 2개 이상이면 위치만으로 맞춘다(`CXMRPlacementAlignment.cpp:251-263`). 저장 때 한 프레임의 위치 노이즈가 `LocalOffset`에 남아 그만큼 차가 옮겨진다.
  - 예측: 원인이 이것이면, **한 번 저장한 뒤 여러 번 다시 불러올 때 매번 거의 같은 자리로 어긋난다.** 다시 불러올 때마다 다른 자리로 가면 복원 쪽(관측 각도에 따른 마커 추정 차이, 보이는 마커 수)을 따로 봐야 한다. R1-5의 로그로 가린다.

### R1-2 컨트롤 창이 매 프레임 레벨 전체 액터를 수십 번 훑음 — 열림

- 문제: `UCXMRTuningWindowComponent::FindPlacement`(`CXMRTuningWindowComponent.cpp:350`)는 `TActorIterator<AActor>`로 레벨 전체를 돌며 `FindComponentByClass`를 부른다. 이 파일 안 17곳에서 부르고, 그중 상당수가 Slate 속성 람다라 창이 열려 있으면 그릴 때마다 실행된다. 차량은 스폰된 액터라 반복 순서상 뒤쪽에 있다. 그래서 레벨 액터 수만큼 매번 다 돈다.
  - 영향: 측정하지 않은 **추정**이다. 실제 CAD 레벨처럼 액터가 수천 개면 MR 프레임 시간에 직접 얹힌다. 테스트 맵에서는 드러나지 않는다.
- 제안: `UCXMRControlPanelWidget::FindLoader`(`CXMRControlPanelWidget.cpp:184-196`)의 `CachedLoader` 방식(약한 포인터 캐시 + 월드·소유자 유효성 확인 뒤 재탐색)을 그대로 쓴다.
- 확인할 시험: 캐시된 Placement의 액터를 파괴하거나 차량을 다시 로드한 뒤 `FindPlacement`가 새 컴포넌트를 찾는지. 유효한 동안에는 같은 포인터를 돌려주는지.

### R1-3 차량별 저장 파일을 거부할 때 아무 안내가 없음 — 열림

- 문제: `LoadCalibrationFromDisk`(`CXMRPlacementPersistence.cpp:193`)는 그룹이 없는 경로에서 `AlignmentStorageMessage`를 비운 채(`:196`) 시작한다. 그 뒤 다음 경우마다 메시지 없이 `false`를 돌려준다.
  - 차량 프로필 경로·`VehicleRootOffset`·차량 클래스가 저장 때와 다를 때(`:224-230`)
  - 프로필에 저장 파일에 없는 보정 마커가 하나라도 있을 때(`:286-290`)
  - 파일이 깨졌을 때
  - 영향: 거부 자체는 의도된 안전장치다(주석 `:286`). 하지만 진행자는 "저장한 정렬이 왜 안 돌아오지"를 알 수 없다. 예: 마커 5번을 프로필에 추가하면 1~4번 저장값까지 전부 버려지고 작성값으로 돌아간다. 차가 엉뚱한 곳에 서는데 경고가 없다.
- 제안: 거부 사유별로 `AlignmentStorageMessage`를 채우고 `LogCXMRAlignment`에 Warning으로 남긴다. 사유는 모델 설정 변경, 마커 구성 변경(추가된 ID 나열), 파일 손상이다. 그룹 경로의 일반 문구(`:197`)도 같은 방식으로 사유를 구분하면 좋다(권장). 파일을 거부하는 조건 자체는 바꾸지 않는다.
- 확인할 시험: 사유별로 파일을 만들고 로드 → `false` + 사유에 맞는 메시지. 정상 파일이면 메시지가 비어 있는지.

### R1-4 정렬 확정을 취소해도 위치 잠금이 남음 — 열림

- 문제: `BeginAlignmentCapture`(`CXMRPlacementAlignment.cpp:123`)는 `bAlignmentPoseLocked = true`로 만든다. 하지만 `CancelAlignmentCapture`(`:60-66`)는 잠금을 되돌리지 않는다. 캡처는 차 이동·마커 추적 끄기·차량 교체로도 취소된다(`CXMRPlacementComponent.cpp:215`, `:273`).
  - 영향: 마커 추종 모드(`bFreezeAfterCalibration=false`, 기본값 아님 `CXMRPlacementComponent.h:89`)에서 한 번 확정을 눌렀다가 취소되면, `Recalibrate` 전까지 차가 마커를 따라가지 않는다(`CXMRPlacementComponent.cpp:279`, `:415`에서 바로 return). 기본 설정(freeze)에서는 보정 뒤 어차피 고정이라 드러나지 않는다.
- 제안: 캡처 시작 전 잠금 상태를 저장해 두고 취소 때 되돌린다. 저장 성공(`CXMRPlacementAlignment.cpp:190`)은 지금처럼 잠근다.
- 확인할 시험: 추종 모드에서 확정 → 차 이동으로 취소 → 다음 마커 이벤트에 차가 따라가는지. freeze 모드 동작은 그대로인지.

### R1-5 저장·복원 정확도를 헤드셋 로그로 가릴 수 없음 — 열림

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

- **Q1 (사용자에게):** `c576309`(10-07 "Merge CXMR USB interaction…")이 `project/pqdq-usb-access`의 PQDQ 전용 커밋 10개(가상 손, USB 포트 표식 등)를 템플릿인 `main`에 넣었다. 지금 두 브랜치는 같다. 의도한 것인가? 의도라면 "USB 포트·가상 손은 템플릿 기능"으로 AGENTS.md에 적는다. 아니라면 템플릿에서 떼어 낼 범위를 별도 명세로 정한다.
