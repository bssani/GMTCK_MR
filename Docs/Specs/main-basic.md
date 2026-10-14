# main에서 USB·디자인 옵션 빼기

- 베이스 브랜치: `main`
- 상태: 구현 대기 — [oct-review-fixes.md](oct-review-fixes.md)가 `main`에 머지된 뒤 시작한다

## 목적

사용자 결정(2026-10-14): **`main`에는 기본 템플릿만 둔다. USB 포트 표식, USB 손 접촉, 디자인 옵션(파트 교체)은 `project/pqdq-usb-access`에만 둔다.** 10-07 머지(`c576309`)가 PQDQ 전용 기능을 `main`에 함께 넣었고, 디자인 옵션은 10-08(`e1b5f77`)에 `main`에 들어왔다. 이 둘을 `main`에서 걷어낸다. 디자인 옵션은 처음엔 남기기로 했다가 같은 날 빼기로 바꿨다.

`main`에 남는 것(사용자 확인 2026-10-14): MR 표시(패스스루·깊이·View Offset·마스킹·VR 전용 오브젝트), 마커 보정·저장·복원·키 조정, 차량 로드(프로필·카탈로그), **운전자 눈 정렬·eye 스냅**, **차량 그룹 공용 정렬**, **손 추적 표시(H)**, **시선 점(G)·Foveated(I)**, **관전 카메라**, **마커 축·라벨 디버그**, 데스크톱 컨트롤 창.

## 범위

`main`에서 지울 것:

1. 파일: `Plugins/CXMR/Source/CXMR/Public/CXMRUsbPortTarget.h`, `Private/CXMRUsbPortTarget.cpp`, `Private/CXMRUsbHandContact.cpp`, `Private/Tests/CXMRUsbInteractionTests.cpp`
2. 다른 코드의 USB 참조:
   - `CXMRPartAssembly.cpp:2, 94, 122, 187` — 조립체가 USB 포트를 자식으로 다루는 부분
   - `CXMRVehicleLoaderComponent.cpp:12, 68, 93, 104` — 옵션 전환 때 포트 접촉을 켜고 끄는 부분
   - `CXMRTuningWindowComponent.h:28` — `ECXMRControlPage::USB`
   - `SCXMRControlPanel.cpp:187, 204, 220-224` — `Port.*` 행과 USB 페이지 필터
   - `CXMRTuningWindowComponent.cpp:585` — 문구의 "physical USB and console"을 USB 없는 문구로(예: "the physical rig")
3. USB만 쓰는 손 접촉 형상: `CXMRHands::GetContactShapes`, `GetSurfaceDistance`, `FContactShape`(`CXMRHandTracking.h:24-30`, `.cpp:133-175`). 지우기 전에 USB 밖에서 쓰는 곳이 없는지 grep으로 확인한다. 손 추적 표시(H)가 쓰는 것은 남긴다.
4. USB 관련 시험: `CXMRPartVariantTests.cpp`(USB 언급 44곳), `CXMRRegressionTests.cpp`(11곳), `CXMRDriverEyeAlignmentTests.cpp`의 USB 부분. 시험이 USB만 확인하면 지우고, 다른 것(조립체 소유권·정리, 옵션 전환)도 확인하면 USB 부분만 빼고 남긴다.
5. 문서: `Docs/Operating-Guide.md`의 USB 절, `Docs/Headset-Test-Checklist.md` §12. `main`에서는 지우고, 한 줄로 "USB 포트·손 접촉은 `project/pqdq-usb-access` 브랜치에 있다"고 남긴다. `Docs/Design-Review-Validation.md`·`Docs/superpowers/`는 작업 기록이라 그대로 둔다.
6. **디자인 옵션** — 10-08 `e1b5f77`이 더한 것 전부:
   - 파일: `CXMRDesignOption.h/.cpp`, `CXMRPartAssembly.h/.cpp`, `CXMRPartSlotComponent.h/.cpp`, `CXMRPartVariantComponent.h/.cpp`, `Tests/CXMRPartVariantTests.cpp`, `Tests/CXMRPartVariantTestTypes.h`
   - 참조: `CXMRVehicleProfile.h:44`(`DesignOptions`), `CXMRVehicleLoaderComponent`의 옵션 선택·스테이징·조립체 정리, `CXMRControlPanelWidget.cpp`, 컨트롤 창 Review 페이지의 옵션 부분, `CXMRModeTests.cpp`·`CXMRRegressionTests.cpp`·`CXMRDriverEyeAlignmentTests.cpp`·`CXMREditor/Private/Tests/CXMRPanelEditorTests.cpp`의 옵션 관련 부분
   - 스크립트: `Scripts/MigrateDesignReviewAssets.py`, `Scripts/VerifyDesignReviewAssets.py` 중 디자인 옵션만을 위한 것. Root·Loader 구조 확인처럼 옵션과 무관한 부분이 쓸모 있으면 남기고 보고한다.
   - 문서: `Docs/Operating-Guide.md`·`Docs/Headset-Test-Checklist.md` §6의 디자인 옵션 설명, `Docs/Design-Review-Asset-Inventory.md`. 한 줄로 "디자인 옵션은 `project/pqdq-usb-access` 브랜치에 있다"고 남긴다.
   - 디자인 옵션 클래스를 참조하는 애셋은 없다(2026-10-14 Claude가 전체 `.uasset`·`.umap`을 클래스 이름으로 확인). 차량 Data Asset에 저장된 `DesignOptions` 값은 프로퍼티가 없어지면 로드 때 무시된다. 그 경고가 로그에 나오면 보고 ④에 적는다.

USB를 참조하는 애셋은 없다(2026-10-14 Claude가 전체 `.uasset`·`.umap`을 `CXMRUsbPortTarget` 문자열로 확인). 에디터 작업은 없다.

## 범위 밖

- `project/pqdq-usb-access` 브랜치. 이 브랜치는 건드리지 않는다. 머지 뒤 Claude가 처리한다(아래 "머지 방법").
- 리뷰 보류 H-5(페이지 enum 정리) 중 USB 외의 부분. `View`·`Diagnostics` 페이지 값은 그대로 둔다.
- 컨트롤 창 레이아웃·디자인 변경
- 다른 기능의 동작 변경

## 지켜야 할 조건

- **이 브랜치의 커밋은 USB·디자인 옵션 제거만 담는다.** 다른 수정·정리를 섞지 않는다. 머지 방법 때문이다.
- 남는 기능의 동작은 그대로다. 차량 로드·교체·정렬 그룹 공유는 디자인 옵션이 없어도 같은 결과를 내야 한다.
- `/CXMR/`·`/Game/` 애셋을 바꾸지 않는다.
- 저장 파일 형식, 조작 키, 다른 패널 문구는 그대로다.

## 머지 방법 (Claude가 한다)

`main`이 USB를 지운 뒤 그 머지를 평소처럼 `project/pqdq-usb-access`에 merge하면, 프로젝트 브랜치에서도 USB가 지워진다. 그래서 이 머지 하나만 프로젝트 브랜치에 **`git merge -s ours <USB 제거 머지 커밋>`**으로 기록한다. 내용은 받지 않고 "받았다"는 기록만 남긴다. 그 전에 프로젝트 브랜치가 `main`의 그 직전까지를 이미 받아 두어야 한다. 그래야 다른 템플릿 변경이 함께 버려지지 않는다. 이후 `main` 변경은 평소처럼 merge하고, 컨트롤 창처럼 USB 행이 얽힌 파일이 충돌하면 프로젝트 쪽 USB 행을 살리는 쪽으로 푼다.

## 근거

- USB 참조 위치: 위 범위의 경로:줄(2026-10-14 `main` `7c18e3c` 기준).
- 사용자 결정: 2026-10-14 대화 — "usb branch에는 그대로 두고 main에는 usb 이런 거 빼 버려. main에는 정말 basic한 거 넣을 거야."

## 구현자가 정할 것

- 2·4에서 USB를 뺀 뒤 남는 코드의 정리 방식(빈 분기 제거 등). 동작이 바뀌지 않는 범위에서.

## 경계 상황

- 디자인 옵션의 조립체 안에 예전에 USB 포트 액터를 넣어 둔 차량 BP가 있으면 → 일반 액터로 취급돼야 한다. 애셋 확인 결과 지금은 없다.
- `Port.*` 튜닝 값이 저장된 `Tuning.json`이 있으면 → 모르는 키는 무시돼야 한다. 기존 레지스트리 동작을 확인하고 보고 ④에 적는다.

## 완료 조건

1. `main`의 `Plugins/CXMR/Source`에서 `grep -ri usb`와 `grep -rE "DesignOption|PartAssembly|PartSlot|PartVariant"` 결과가 0이다(주석·문구 포함).
2. 남는 시험이 통과하고, 지운·고친 시험 목록을 보고 ③에 적는다.
3. `-DisableAdaptiveUnity` 빌드 성공, CXMR·GMTCK_MR 모듈 경고·오류 0.
4. 바이너리 애셋 변경 0.
5. 범위 5·6의 문서가 고쳐져 있다.

## 로컬에서 확인할 것

- 빌드·시험: 위 2·3.
- 에디터(사용자 또는 Claude가 MCP로): `/Game/Map/L_Main` PIE → 컨트롤 창에 USB 행과 디자인 옵션이 없고, 차량 교체가 그대로 되는지. 로그에 클래스·프로퍼티를 못 찾는 오류가 없는지.
- 헤드셋: 필요 없음(기능 제거만).

## 개정 이력

- 2026-10-14: 처음 작성 (Claude)
- 2026-10-14: 디자인 옵션도 `main`에서 빼기로 — 사용자 결정
