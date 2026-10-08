# Design review implementation validation

Verified locally on 2026-10-08, branch `feature/design-option-review`.

## Result

- `GMTCK_MREditor Win64 Development` build: succeeded.
- Full `CXMR.` automation suite: **70 passed, 0 failed**, process exit 0.
- Bundled Blueprint compilation and `L_Main` load/Root structure checks: **0 errors, 0 warnings**.
- Actual Slate screenshots inspected for Review, Alignment and Settings. Preview names and geometry are synthetic test fixtures.
- `git diff --check`: passed.
- Final label-only refinement: rebuilt and reran `CXMR.Panels.RenderPreview`; passed and screenshots refreshed.

Logs and previews are local under `Saved`: `DesignReviewTests.log`, `DesignReviewAssetMigration.log`, `DesignReviewAssetVerification.log`, `ControlPanelPreview_0.png`, `ControlPanelPreview_2.png`, `ControlPanelPreview.png`.

## Coverage

Actor/world tests cover same-slot replacement, independent slots, nonzero CAD offsets, fixed anchor preservation, failed class/model/transform/ID/mount validation, authored construction activity, hidden staging, ChildActor ownership, unattached owned USB cleanup, foreign actor preservation and vehicle unload. USB tests cover surface/bone contact, hysteresis, tracking loss, nearest-port arbitration, inactive options and settings registration after replacement. Panel tests cover mode navigation, separate exposure application, review asset lifetime across GC, calibration confirmation and window recovery. Existing marker/alignment persistence tests remain in the full suite.

The independent review exposed four integration defects that were corrected: constructed activity overwritten by staging, detached owned children omitted from cleanup, USB registry ownership lost during replacement, and weak-only menu references lost after GC. Actual bundled map loading also exposed the old Loader attachment reference; Root PostLoad and asset migration now resolve it to VehicleAnchor.

## Asset migration

`Scripts/MigrateDesignReviewAssets.py` removes retired input mappings, compiles bundled Blueprints, resaves affected vehicle/Pawn assets, migrates map attachment references and deletes eight retired InputAction assets only after loading packages to confirm no actual references. The first migration pass also resaved other bundled assets; those changes remain. Automatic approval review rejected a broad asset restore because it could discard unrelated user work; no restore was executed.

`Scripts/VerifyDesignReviewAssets.py` loads bundled assets and the actual template map and checks the minimal Root and Loader attachment without saving them.

## Remaining validation boundary

No headset, packaged build, physical cable depth visibility or subjective contact-feedback validation was performed. Real vehicle meshes are not automatically split or repositioned. Authors must configure named mounts and complete assemblies using [Operating-Guide.md](Operating-Guide.md), then validate on the physical rig using [Headset-Test-Checklist.md](Headset-Test-Checklist.md). Assembly construction must remain self-contained; unrelated global side effects are not transactional. Option selection is session state, separate from saved alignment.

Changes are uncommitted; no push or merge was performed.

## 2026-10-08 리뷰 반영

브랜치 `feature/design-option-review`, 시작 HEAD `23ec56ec9d6212954454dd83a2f032943605338f`를 유지했다. commit/push/merge/rebase, `.codex/` 수정, 바이너리 애셋 restore/save는 하지 않았다. 새 소스·문서·Scripts는 보존했다. 슬롯 기본 옵션 자동 선택은 보류 상태다.

### 반영 내용

1. 운영 문서 두 개를 `git show HEAD:<path>`로 복원한 뒤 제거된 기능 절만 정리했다. 키보드·넘버패드·NumLock, Keys move along/Keep Level, §2-1 depth range 깜빡임, MR 알파, 마커 추적·freeze·표본 평균, 마스킹·손 보정·관전 카메라·USB 손 접촉·차량 공용 정렬, 문서 표와 `/CXMR/`/`/Game/` 규칙을 남겼다. Review/Alignment/Settings, 노출 프리셋, 슬롯/조립체/옵션 작성과 마이그레이션 범위는 한국어로 추가했다.
2. 그룹 정렬 삭제는 컴포넌트가 같은 Placement/그룹에 대해 5초 동안 무장하며 두 번째 클릭에만 실행한다. 실시간을 사용해 일시정지·시간 배율에 영향을 받지 않는다. 다른 페이지 선택·창 닫기·대상 변경·시간 초과는 확인을 무효화한다. 버튼은 Advanced Placement 안에 있고 확인 라벨에 그룹 이름이 나온다. 실패 시 재확인 후 재시도한다.
3. `GetAlignmentStatus`가 차량·정렬 상태를 함께 만든다. 옵션 실패는 Review 하단에 남고 헤더의 정렬 상태를 가리지 않는다. 보정 오류가 있으면 상태 뒤에 붙인다.
4. BeginPlay에서 unbound PPV의 bias와 override를 캡처한다. MR/VR 프리셋 존재 여부를 각각 관리하고 없는 모드는 기준값을 표시·복원한다. `Display.Exposure`만 있고 새 키가 둘 다 없는 설정은 로드 시 두 새 키의 메모리 저장값으로 이관한다. 이후 일반 설정 저장 시 함께 디스크에 기록된다. 새 키가 하나라도 있으면 레거시 값으로 다른 모드를 채우지 않는다.
5. Review Loader는 weak cache가 유효할 때 재사용한다. Tick/Status/버튼은 같은 경로를 쓴다. 무효·파괴된 Loader는 다시 찾는다. Root PostLoad는 AttachTarget이 비어 있거나 액터 컴포넌트 집합 밖에 있을 때만 앵커로 이관한다. VehicleAnchor는 캘리브레이션만 움직인다는 설명을 복원했다.
6. 새 Slate 세 파일은 탭 들여쓰기와 한 줄 헬퍼 호출로 정리했다. MR/VR Subsystem 조회를 공통 헬퍼로 옮겼고 Alignment 중복 안내·바깥 surface를 제거했다. 빈 CoreRedirects 섹션을 삭제했다. 저장된 리포 애셋 44개에서 Setup API 이름의 ASCII/UTF-16 참조가 없었고, 소스 호출·미사용 API를 제거했다. Blueprint 로드·컴파일도 통과했다. VehicleRoot.h/.cpp와 Headset-Test-Checklist.md는 CRLF다.

### 테스트와 증거

동작 2·3·5는 테스트를 먼저 추가하고 수정 전 실제 실패를 확인했다. 상태 조합은 기존 동작 그대로 컴포넌트로 추출해 테스트한 뒤 실패 우선순위를 고쳤다.

- RED: `DesignReviewFixRed.log`의 새 테스트 5개 모두 실패, 프로세스 exit 1 (엔진 기록 -1). 노출 1.5→0 덮어쓰기, 레거시 0.7 누락, VR 기준값 복원 누락, 정렬 상태 가림, 확인 만료 누락을 재현했다.
- RED: `DesignReviewFixResetRed.log`의 `CXMR.Alignment.Group.ResetScope` 실패, 프로세스 exit 1 (엔진 기록 -1). 첫 클릭이 실제 그룹 저장과 복원 정보를 지우는 것을 재현했다.
- 기존 Group.ResetScope/ResetFailure는 두 클릭, 첫 클릭 보존·재시도 검증으로 보강했다. 새 ResetExpiry는 만료·그룹 라벨·다시 무장 후 실행을 확인한다. FailureAlignmentStatus는 실제 잘못된 SelectDesignOption 뒤에도 차량/Aligned를 확인한다.
- 새 ExposureBaseline은 저장값 없는 PPV bias 1.5와 override true/false를 모두 확인한다. ExposureLegacy는 옛 키 0.7의 MR/VR 이관을 확인한다. ExposurePartial은 MR 2.2와 옛 키가 함께 있어도 VR은 기준 1.5/override false로 돌아가고 사용자 조작 후에는 VR 프리셋을 쓰는 것을 확인한다.
- ReviewAssetLifetime에는 파괴된 cached Loader의 재발견을 보강했다. RenderPreview는 기본 Alignment·Advanced Placement·무장 상태를 실제 Slate로 렌더링한다. 이름과 차량 지오메트리는 합성 fixture다.
- 수정 후 첫 전체 실행은 74 통과/1 실패였다. 게임 시간 delta 제한으로 만료 검증이 실패해 실시간 기준으로 수정했다.
- **최종 전체 `CXMR.`: 75 통과, 0 실패, 프로세스/엔진 exit 0.** `Saved/DesignReviewFixTests.log`.
- **GMTCK_MREditor Win64 Development 빌드 성공, exit 0.** `Saved/DesignReviewFixBuild.log`. 캡처 위치만 보정한 최종 테스트 코드 빌드도 exit 0 (`DesignReviewFixPreviewBuild.log`). 빌드 전 실행 중 에디터가 없는지 확인했고 빌드는 직렬 실행했다.
- 읽기 전용 번들 Blueprint 컴파일·L_Main/Root/Loader 구조 검증: **오류 0, 경고 0, exit 0**. `Saved/DesignReviewFixAssetVerification.log`. 이전/이후 SHA-256 비교로 기존 40개 바이너리 변경·삭제 상태가 유지됐음을 확인했다.

- `CXMR.Panels.RenderPreview` 단독 실행: 1 통과/0 실패, exit 0 (`Saved/DesignReviewFixPreview.log`). 이후 그룹 범위 안내·접힘 제목 대비를 보정해 최종 전체 테스트에서 캡처를 다시 생성했다.
- 실제 Slate 이미지: `Saved/ControlPanelPreview_0.png`(Review), `ControlPanelPreview_2.png`(Alignment), `ControlPanelPreview.png`(Settings), `ControlPanelPreview_AlignmentAdvanced.png`, `ControlPanelPreview_AlignmentArmed.png`. 무장 라벨·Advanced Placement 내부 위치·중복 안내 제거·그룹 범위 안내를 직접 확인했다.
- 최종 `git diff --check`: exit 0. 탭 들여쓰기·멀티라인 람다 없음과 요청한 세 파일의 CRLF를 확인했다.

### 바이너리 애셋과 남은 범위

[전체 애셋 분류 표](Design-Review-Asset-Inventory.md)에 HEAD 대비 40개를 각각 적었다. 필요한 변경 14개(수정 6, 삭제 8), 첫 패스 덤 재저장 26개다. 덤 재저장의 의미적 무변경까지 증명한 것은 아니며 되돌릴지는 사용자 결정이다. 광범위한 restore를 재시도하지 않았다.

헤드셋·패키징·실제 케이블 depth·주관적 사용성은 검증하지 않았다. 실제 CAD 분해/옵션 제작과 슬롯 기본 옵션 선택도 수행하지 않았다. 여러 unbound PPV 중 첫 번째를 쓰는 기존 선택 규칙은 유지한다. 조립체의 외부 부작용은 원래처럼 트랜잭션 범위 밖이다.

## Git 게시 승인 (2026-10-08)

검증 후 사용자가 현재 변경의 커밋과 `origin/feature/design-option-review` 게시를 승인했다. 위의 미커밋·미푸시 문구는 이 승인 전 점검 시점의 기록이다. `.codex/`는 게시에서 제외하며 기존 바이너리 변경과 새 소스·문서·Scripts는 포함한다. 테스트 이후 실행 코드 변경은 없다.

## 후속 상태 오류 수정 (2026-10-08, 기준 6332833)

현재 `project/pqdq-usb-access` 작업 트리에서 다음 두 리뷰 항목을 수정했다.

- **P1 — 그룹 리셋 후 잘못된 Aligned 상태:** 파일 삭제가 성공한 뒤 원본 오프셋을 복원하고 현재 앵커를 기준으로 임시 조정값을 비운다. 기존 `Recalibrate()`로 보정 완료·수동/복원 잠금·캡처·관측 시각·표본 평균·마커 설정 이력·fit 오차·조정 방향·안정화 타이머를 초기화한다. 과거 관측으로 즉시 재계산하지 않으므로 새 관측 전에는 위치를 유지하고 Alignment pending을 표시한다. 삭제 실패 경로는 이전 상태를 유지한다.
- **P2 — 중첩 PartAssembly의 비활성 상태 고착:** 후보 계층에 또 다른 PartAssembly가 있으면 교체 전 `PrepareForReview()`에서 영어 오류로 거부한다. 기존 옵션과 USB 접촉을 유지하고 거부한 후보·자식 조립체·USB를 정리한다. 하위 모델은 일반 Actor/ChildActor 또는 메시 컴포넌트로 구성한다. 운영 가이드에 이 제한을 추가했다.

새 회귀 테스트는 `CXMR.ReviewState.ResetFreshObservations`, `ResetPendingSettle`, `NestedAssemblyRejected`다. 보정된 그룹을 리셋한 뒤 헤더·앵커·기본 freeze에서의 새 관측, 이전 표본 제외·안정화 시간 재시작, 실제 ChildActor 조립체 계층의 교체 거부·기존 옵션 보존·후보 정리를 검증한다. 기존 `CXMR.Alignment.Group.ResetFailure`에 삭제 실패 후 보정 완료 상태 보존도 추가했다.

- **RED:** 수정 전 새 테스트 3개 모두 실패. `Saved/ReviewStateRedTests.log`, 프로세스 exit **255**, 엔진 기록 **-1**. 이전 안정화 타이머가 새 보정을 일찍 완료하는 실패까지 확인했다.
- **GREEN 빌드:** `GMTCK_MREditor Win64 Development` 성공, exit **0**. `Saved/ReviewStateGreenBuild.log`.
- **GREEN 전체 CXMR:** **78 통과 / 0 실패**, 프로세스·엔진 exit **0**. `Saved/ReviewStateGreenTests.log`. `CXMR.Panels.RenderPreview`도 포함한다.
- 빌드 전 에디터가 실행 중이지 않음을 확인했고 빌드는 직렬로 수행했다. `git diff --check` 통과. 바이너리 애셋과 `.codex/`는 수정하지 않았다.

이번 검증은 헤드셋 없이 실제 Unreal 자동화 월드에서 수행했다. 실제 헤드셋 마커 재관측·패키징은 검증하지 않았다. 중첩 PartAssembly 지원을 구현한 것은 아니며 기존 콘텐츠가 이 구조를 사용하면 일반 자식 Actor로 고쳐야 한다. 이번 후속 수정은 아직 커밋·푸시하지 않았다.

검증 후 사용자가 후속 수정의 커밋과 main 머지를 승인했다. 위 미커밋·미푸시 문구는 승인 전 검증 시점의 기록이며, 이번 승인에 따라 USB 브랜치와 main에 게시한다. 검증 이후 실행 코드 변경은 없다.
