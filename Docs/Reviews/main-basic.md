# main-basic 리뷰

명세: [Docs/Specs/main-basic.md](../Specs/main-basic.md)

## 1회차 — 2026-10-15, `8e1b303` (`codex/oct-review-fixes...codex/main-basic`)

- 재현: `-DisableAdaptiveUnity` 빌드 성공, CXMR·GMTCK_MR 경고·오류 0(`Saved/ClaudeReview1015_Build2.log`). `CXMR.` **73/73 통과**(`Saved/ClaudeReview1015_Test2.log`). 1번의 91개에서 보고한 18개를 뺀 수와 같다.
- 완료 조건 1: `Plugins/CXMR/Source`에서 `git grep -i usb`와 `git grep -E "DesignOption|PartAssembly|PartSlot|PartVariant"` 결과 0을 확인했다.
- 바이너리 애셋·Config 변경 0. 커밋은 제거만 담았다. 새 파일 `Tests/CXMRVehicleTestTypes.h`는 지운 옵션 fixture에서 남는 시험이 쓰던 메시 2개와 활성 상태만 옮긴 것이라 제거 작업의 일부로 본다.
- 제품 코드는 명세 범위 2·3·6의 참조만 지웠고 남는 기능의 동작은 그대로다(로더의 활동 상태 저장·복원, 패널 페이지 필터, 손 표시용 `GetBones`·`GetJointRadius`).

### M1-1 디자인 옵션 주석이 빈 채로 남음 — 반영함 (`9bbf878`)

- 문제: `CXMRVehicleProfile.h:42`의 `/** 고정 부품 장착점에서 비교할 설계 옵션. */`이 지운 `DesignOptions` 프로퍼티 자리에 남았다. 이제 아무것도 설명하지 않고, 바로 아래 `Ergonomics` 주석과 겹친다. 한국어라 완료 조건 1의 검색에 걸리지 않았다.
- 제안: 그 한 줄을 지운다. 이 브랜치는 제거만 담아야 하므로 이것도 제거 커밋이다.
- 확인할 시험: 없음(주석).

### 기록 (조치 불필요)

- 1번 브랜치에 리뷰 반영 커밋(R2-1·R2-2)이 생기면 이 브랜치에 `git merge codex/oct-review-fixes`로 받는다. 프로젝트 브랜치는 1번 머지를 평소처럼 받은 뒤 이 브랜치의 머지만 `-s ours`로 기록하므로, 이 머지 커밋이 섞여도 1번 내용은 버려지지 않는다([Agent-Workflow §2.4](../Agent-Workflow.md#24-프로젝트-브랜치에-템플릿-변경-넣기)).
- 명세의 "로컬에서 확인할 것"(PIE에서 컨트롤 창 행·차량 교체·로드 경고)은 3번 컨트롤 창이 패널을 새로 만든 뒤 한 번에 본다. 지금 패널은 3번에서 대체된다.

## Codex 1회차 반영 — 2026-10-15

- 1번 반영 f47a30c를 git merge로 받음. M1-1은 고아 주석과 빈 줄만 제거함. 제품 변경은 제거뿐임.
- 완료 조건 1: Plugins/CXMR/Source의 usb(대소문자 무시), DesignOption|PartAssembly|PartSlot|PartVariant 검색 0.
- 완료 조건 2·3: -DisableAdaptiveUnity 빌드 성공, CXMR·GMTCK_MR 경고·오류 0. CXMR. 74/74 성공·종료 코드 0(1번 신규 AutomaticAlignmentSave 포함). 시험 추가 제거·수정 없음.
- 완료 조건 4·5: 바이너리 변경 0, 기존 문서 정리 유지. 로그 Saved/Codex1015_Basic_Build.log, Saved/Codex1015_Basic_Test.log.
- 새 Varjo·엔진 API 없음. 로컬 PIE 행·차량 교체·로드 경고 확인은 리뷰 기록대로 3번에서 Claude가 확인할 일로 유지함. 헤드셋 필요 없음. 질문 없음.

## 2회차 — 2026-10-15, `9bbf878` — 통과

- 재현: `-DisableAdaptiveUnity` 빌드 성공, CXMR·GMTCK_MR 경고·오류 0(`Saved/ClaudeReview1015b_Build2.log`). `CXMR.` **74/74 통과**(`Saved/ClaudeReview1015b_Test2.log`, 1번의 새 시험 하나 포함). USB·디자인 옵션 검색 0. 바이너리 애셋 변경 0.
- M1-1 반영 확인. 1번 반영은 `bb76d51` merge로 받았고, 이 브랜치의 Codex 커밋은 여전히 제거만 담는다.
- 열린 항목 없음. 머지는 사용자 승인 뒤, 1번 다음에 한다. 프로젝트 브랜치에는 `-s ours`로 기록만 한다(명세 "머지 방법").
