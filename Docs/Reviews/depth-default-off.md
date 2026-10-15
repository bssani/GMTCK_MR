# depth-default-off 리뷰

## 1회차 — 2026-10-15, `1c2802a`

재현(Claude): `-DisableAdaptiveUnity` 빌드 성공, 경고·오류 0. `CXMR.` 81/81 통과, `TEST COMPLETE. EXIT CODE: 0`. 로그는 `Saved/ClaudeReview1015e_Build.log`, `Saved/ClaudeReview1015e_Tests.log`. 시험 뒤 이 PC의 `Saved/CXMR/Tuning.json`이 시험 전과 바이트까지 같은 것을 확인함.

**통과. 열린 항목 없음.**

확인한 것:

- 기본값 두 곳(`CXMRSubsystem.h:45`, `CXMRTuningWindowComponent.cpp:220`)이 시안 `def: false`와 같다. `Depth.EnvEstimation`·범위 기본값은 그대로다.
- 문구 세 곳이 시안과 글자까지 같다. 도움말, 시작 상태줄, MR 설명이다. 라벨과 다른 행은 바뀌지 않았다.
- MR 시작 순서와 세션 가드(`CXMRSubsystem.cpp:255-270`)는 바뀌지 않았다.
- 새 시험 `CXMR.Panels.DepthDefaultOff`는 실제 레지스트리 등록으로 명세의 경계 상황을 모두 덮는다.
  - 저장값 없음 → 호출이 `MR, Depth.Test, Depth.Range, Diagnostics.MarkerTracking` 순서이고, 깊이·실세계 깊이 둘 다 꺼짐
  - 저장값 1 → `Depth.EnvEstimation`까지 적용
  - 변경 수 0↔1
  - Reset MR
  - `T`
- `/CXMR/`·`/Game/` 애셋에서 `SetDepthTest`·`ToggleDepthTest`·`SetDepthTestEnabled`를 부르는 곳이 없다. Blueprint가 시작할 때 다시 켜는 경로는 없다.
- 바이너리·Config 변경 0. 문서 두 곳이 명세대로다.

기록(조치 필요 없음):

- 새 시험은 실제 `Saved/CXMR/Tuning.json`을 바꿨다가 `ON_SCOPE_EXIT`로 되돌린다. 기존 `CXMRReviewExposureTests.cpp:20-59`와 같은 방식이다. 시험 도중 에디터가 죽으면 시험값이 남는다. 이 PC는 개발용이라 영향이 작다. 헤드셋 PC에서는 자동화 시험을 돌리지 않는다.
- 헤드셋 시험 상태는 **not-tested**다(명세 "로컬에서 확인할 것").

## 질문

없음.
