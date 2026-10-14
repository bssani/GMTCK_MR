# marker-follower — 구현 보고

명세: [marker-follower.md](../Specs/marker-follower.md). 2026-10-14 Codex.
브랜치 `codex/marker-follower`, 베이스 `main` (`7292f3d`).
Claude 리뷰 회차는 아직 없다. 시작 시 기존 리뷰 파일·열린 항목 없음.

## ① 바뀐 파일

- `Plugins/CXMR/Source/CXMR/Public/CXMRMarkerFollower.h`: Blueprint 부모, 여섯 프로퍼티, 추종·모드 함수, found/lost 이벤트.
- `Plugins/CXMR/Source/CXMR/Private/CXMRMarkerFollower.cpp`: 서브시스템 이벤트 구독, 월드 포즈 합성, loss/reacquisition, 런타임 모드 라벨, 모드 복원·경고.
- `Plugins/CXMR/Source/CXMR/Private/Tests/CXMRMarkerFollowerTests.cpp`: 합성 이벤트 자동화 4개.
- `Docs/Operating-Guide.md`: §3-2 만드는 법, 기존 DynamicObject 설명 수정.
- `Docs/Headset-Test-Checklist.md`: §15-4 명세 시험 절차.
- `Docs/Reviews/marker-follower.md`: 이 보고. `HANDOFF.md` 결과는 git 무시.

Placement·Subsystem·Varjo 플러그인·설정·기존 애셋은 수정하지 않았다.

## ② 완료 조건

| 명세 조건 | 결과와 근거 |
|---|---|
| 1. 합성 이벤트 | 충족. `CXMR.MarkerFollower` 4/4. 감지·이동의 위치·회전/오프셋 합성, 다른 ID, Hold/Hide, Moved 재획득, follow 정지·재개, ID 0, 보정 마커 경고, 중복 ID 동시 추종, EndPlay 구독 해제 확인 |
| 2. 감지 전 모드 요청 금지 | 명세가 허용한 대체 확인. 가로채기 지점이 없어 소스 호출 경로와 WaitingAndZeroId 시험으로 확인(③). 실제 런타임 호출 횟수·성공/실패 주입은 미측정 |
| 3. BP 부모·Details | 충족. UnrealAgent MCP로 임시 BP 생성·컴파일. 부모 표시, 여섯 값·기본값, SceneComponent 루트, 세 함수 노출 조회. 화면 확인 `Saved/MarkerFollowerDetails1014.png`. 임시 BP 미저장 삭제 성공·에디터 종료 |
| 4. 빌드·전체 시험 | 충족. 최종 `-DisableAdaptiveUnity` 빌드 성공, 경고·오류 0. `CXMR.` 발견 82, 성공 82, 실패 0, 종료 0 |
| 5. 바이너리·문서 | 충족. `.uasset`/`.umap` 변경·추가 0, 문서 두 곳 갱신 |

헤드셋 시험은 **not-tested**다. 이 PC에 Varjo 런타임이 없다.

## ③ 명세와 다르게 한 것·검증 한계

- 제품 동작의 명세 이탈 없음. timeout 값은 명세 프로퍼티에 없으므로 바꾸지 않는다.
- 모드 요청을 가로채는 기존 지점이 없다. `UCXMRSubsystem::SetMarkerTrackingMode`는 non-virtual(`Public/CXMRSubsystem.h:169`), Varjo 진입점은 static(`Plugins/VarjoPlugin/Source/VarjoOpenXR/Public/VarjoOpenXR.h:154`)이다. 이 시험을 위해 기존 서브시스템을 수정하지 않았다.
- 대체 확인: `CXMRMarkerFollower.cpp:118-128`에서 관측 전에는 캐시 getter가 이미 감지된 ID를 확인해야 setter에 도달한다. getter는 `CXMRSubsystem.cpp:592-595`에서 미등록 ID를 거절한다. 감지/이동 수신 후에만 `bCanConfigure`가 켜진다. WaitingAndZeroId는 BeginPlay·BP 함수·PIE 속성 변경 경로에서 관측 전 waiting·원래 포즈를 유지하고, 합성 관측 후에는 런타임 모드를 꾸며내지 않는 것을 확인했다. setter는 세션 running 검사로 보호된다(`CXMRSubsystem.cpp:587`). 실제 헤드셋에서 이미 감지된 마커의 BeginPlay 설정, 요청 실패 후 재시도, EndPlay 복원은 별도 확인해야 한다.
- 제공된 `mcp__unreal_vbor` 도구는 3000 포트를 가리켰다. 리포 `.codex/config.toml`에 적힌 `http://localhost:55559/mcp`의 tools/list → execute_python/call_tool로 BP 확인·스크린샷을 수행했다. 구성 파일은 바꾸지 않았다.
- 임시 BP `/Game/__CXMRMarkerFollowerCheck/BP_MarkerFollowerCheck`만 생성·컴파일·열고 저장 없이 삭제했다(명세 완료 조건 3). 기존 `/Game/`·`/CXMR/` 애셋·레벨에는 쓰지 않았다. `.uasset` 파일이 없음을 전후 확인했다.
- 최초 빌드의 C4701 경고 1건은 라벨 조회 변수 초기화로 수정했다. 최초 전체 실행은 기존 78/78 통과, 신규 0/4 실패였다. 원인은 테스트 월드가 액터 초기화 전이라 AActor::ProcessEvent와 EndPlay가 차단된 것. InitializeActorsForPlay를 추가한 뒤 최종 전체 82/82 통과했다. 의도적 red 실행·반복/스트레스 시험은 하지 않았다.
- 자동화 실행의 OpenXR loader API 1.1/1.0 메시지는 `-nohmd` 환경 로그다. 최종 자동화 종료 0과 실제 헤드셋 결과를 구분한다.

## ④ API 소스 확인

아래 경로는 소스로 직접 확인했으며 API 동작을 추정해서 구현하지 않았다.

- `Plugins/VarjoPlugin/Source/VarjoOpenXR/Private/VarjoMarkersPlugin.cpp:83`: 이벤트가 보고한 prediction으로 실제 모드 갱신. `:89-101`: TrackingToWorld 적용 후 월드 위치·회전 broadcast. `:107`: loss는 ID만 broadcast.
- 같은 파일 `:161-188`: 모드 setter의 등록 ID 조건·런타임 요청·캐시 갱신, getter의 캐시 조회. getter 자체는 런타임을 호출하지 않는다.
- `Plugins/CXMR/Source/CXMR/Private/CXMRSubsystem.cpp:42-49,583-602`: running 검사, enum 변환, 알려진 ID만 조회. `Public/CXMRSubsystem.h:163-170`: 이벤트 시그니처·모드 함수.
- `C:/Program Files/Epic Games/UE_5.7/Engine/Source/Runtime/Core/Public/Math/TransformVectorized.h:54-55`: A*B는 A 먼저 적용.
- `C:/Program Files/Epic Games/UE_5.7/Engine/Source/Runtime/Engine/Classes/GameFramework/Actor.h:1803,2387`: SetActorTransform·PostEditChangeProperty.
- `C:/Program Files/Epic Games/UE_5.7/Engine/Source/Runtime/Engine/Classes/Components/TextRenderComponent.h:104,120,148`: 텍스트·정렬·월드 크기.
- `C:/Program Files/Epic Games/UE_5.7/Engine/Source/Runtime/Engine/Private/Actor.cpp:1471-1477,3194-3203`: 초기화되지 않은 월드의 ProcessEvent·액터 EndPlay 제한. `Classes/Engine/World.h:3842`: InitializeActorsForPlay.

## ⑤ 로컬에서 확인할 것

- 빌드: `Saved/MarkerFollowerBuild1014.log`, `-DisableAdaptiveUnity`, 성공·경고/오류 0. 사용자 시험 PC에서도 같은 옵션으로 빌드한다.
- 자동화: `Saved/MarkerFollowerTests1014.log`, `CXMR.` 82/82, 종료 0. 새 접두어 `CXMR.MarkerFollower`.
- 에디터: `Saved/MarkerFollowerBlueprint1014.json`의 기본값 조회, `Saved/MarkerFollowerDetails1014.png`의 부모·Details, `Saved/MarkerFollowerCleanup1014.json`의 임시 BP 삭제 성공. 에디터 닫힘. 사용자는 `/Game/` 아래 자식 BP·메시·레벨 배치를 직접 만든다(Operating-Guide §3-2).
- 헤드셋: 체크리스트 §15-1/15-2 준비·Varjo OpenXR 런타임 확인 후 §15-4 Stationary/Dynamic 박스 추적 비교. 실제 지연·떨림·축·loss/reacquisition·PIE 중 모드 변경은 not-tested. follow 토글, Hold/Hide, 이미 감지된 ID로 시작, 종료 후 프로필/Stationary 복원도 헤드셋에서 확인한다. §15-5 환경과 관찰 기록.

## 질문

없음. Claude가 `main...codex/marker-follower`를 리뷰하면 된다. push·머지 없음.
