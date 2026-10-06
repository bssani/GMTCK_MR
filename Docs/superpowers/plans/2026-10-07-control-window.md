# CXMR Unified Control Implementation Plan

> Inline execution in the current project branch; preserve existing placement and USB changes. Commit, push and merge were authorized after implementation.

**Goal:** 기존 두 데스크톱 패널을 사용/설정 모드와 작업별 메뉴를 갖춘 하나의 창으로 통합한다.

**Architecture:** UCXMRTuningWindowComponent가 창과 탐색/보정 상태를 소유한다. SCXMRControlPanel은 표시를 담당하며 기존 ControlPanelWidget의 차량/시점 바인딩과 Tuning 레지스트리를 사용한다. DesktopPanel API와 Editor 버튼은 이 창으로 연결한다.

**Tech Stack:** UE 5.7 C++, Slate, UMG, Unreal Automation.

## Task 1: 탐색과 창 수명

Files: CXMRTuningWindowComponent.h/.cpp, CXMRDesktopPanelComponent.cpp, CXMRPanelEditorTests.cpp.

- [x] 테스트: 두 열기 버튼이 같은 창에 연결됨, X로 닫기/재열기/EndPlay 정리, 사용 모드의 고급 화면 차단.
- [x] 현재 코드에서 테스트 실패를 확인한다.
- [x] 창을 Tuning 컴포넌트 하나로 통합하고 Desktop API를 위임한다. SelectPage/SetSetupMode/GetActivePage를 추가한다.
- [x] 기존 버튼 ID는 유지하고 레이블은 Control/Settings로 변경한다.

## Task 2: 작업 화면과 보정 안내

Files: new Private/SCXMRControlPanel.h/.cpp, CXMRControlPanelWidget.h, CXMRPlacementComponent.h/.cpp.

- [x] 좌측 메뉴, 사용/설정 전환, 실시간 상태, 화면별 그룹을 구현한다.
- [x] 차량/시점은 기존 바인딩을 재사용한다. 새 UI를 위해 기존 클래스의 BuildViewerPage/BuildDisplayPage를 공개한다.
- [x] 보정은 시작→실제 완료 상태 확인→사용자 확인→저장 순서로 처리한다. 저장 성공을 실제 SaveCalibrationToDisk 결과로 기록한다.
- [x] 기능 등록/해제 시 열려 있는 화면을 갱신하고 존재하지 않는 기능에는 상태 설명을 표시한다.

## Task 3: 조작과 검증

Files: CXMRPanelUI.cpp/.h, CXMRTuningSubsystem.h, Docs/Editor-Followup.md.

- [x] Float 슬라이더+직접 입력, Choice 목록, 도움말, 행 여백을 적용한다.
- [x] GMTCK_MREditor Win64 Development 빌드, CXMR.Panels. 테스트, CXMR. 전체 테스트를 실행한다.
- [x] 가능한 렌더 실행에서 창 재열기와 중복 창 방지를 확인한다.
- [x] 운영 안내와 검증 범위를 기록한다. 커밋/푸시는 하지 않는다.

## 검증 기록

- GMTCK_MREditor Win64 Development 빌드 성공.
- 최종 `CXMR.` 25개 모두 통과, 실패 0, 종료 코드 0.
  `Saved/UnifiedControlAccepted.log`에서 결과 확인.
- 렌더 가능한 실행에서 두 툴바 버튼의 단일 창 공유, X 닫기/재열기,
  설정 모드 전환과 여섯 작업 화면의 Slate 렌더를 확인함.
  이미지: `Saved/ControlPanelPreview.png`, `ControlPanelPreview_0/1/2/4/5.png`.
- 저장 전 확인, 확인 후 이동, 위치 복귀, 파일 쓰기 실패, 같은 마커 프로필을 쓰는 차량 교체,
  이전 차량 복귀, 차량 삭제에 대한 보정 상태 회귀 검사를 통과함.
- 최초 중복 자동 열기 검사와 확인 상태의 새 회귀 사례가 수정 전 실패하는 것을 확인함.
- 요청한 코드 리뷰 스킬의 별도 정적 검토에서 공유 마커 프로필 차량 교체 문제를 발견하고 수정함.
- `git diff --check` 통과. 기존 배치/USB 변경 유지, 커밋·푸시 없음.
- 헤드셋 사용성/물리 정합 검증은 수행하지 않음. 실물 케이블·금속 플러그 가림 문제는 남아 있음.
## 언어·주석 정리

- 화면·상태 문구·도움말은 영어로 통일함.
- 수정한 코드 파일의 주석은 짧은 한국어로 정리함. 저작권과 인자 이름 표시는 유지함.
- 에디터 툴팁은 영어 메타데이터로 분리함. enum 주석은 UHT의 자동 툴팁에서 제외함.
- 소유 C++ 소스에 한국어 문자열 없음. 빌드와 전체 25개 검사 통과함.
- 영어 화면 렌더 확인함. 로그: `Saved/EnglishControlVerified.log`.