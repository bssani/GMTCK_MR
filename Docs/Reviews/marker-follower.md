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

---

# Claude 리뷰

## 1회차 — 2026-10-14, `7400a5d`

- 재현(Claude, 별도 worktree, 새로 받은 상태):
  - `-DisableAdaptiveUnity` 빌드 성공. CXMR·GMTCK_MR 경고·오류 0.
  - 경고 5건은 Varjo 플러그인 소스(`VarjoHandInteraction.cpp` C5038·C4996)다. 이 PC의 작업 폴더는 이미 빌드돼 있어 안 보였을 뿐이고, git으로 새로 받은 시험 PC에서도 똑같이 보인다. 무시해도 된다.
  - `CXMR.` 82/82 통과.
- 확인한 것:
  - 감지 전 모드 요청 금지: 정상. `UCXMRSubsystem::GetMarkerTrackingMode`는 이벤트로 받은 ID(`PluginMarkerIds`)만 Varjo에 묻는다(`CXMRSubsystem.cpp:590-593`). 그래서 BeginPlay의 시도는 세션 전 Varjo를 부르지 않는다. 설정은 `IsXRSessionRunning()`으로 막혀 있다(`:587`). 보고 ③의 대체 확인에 동의한다.
  - 라벨 방향: X축을 머리 쪽으로 돌린다. 마커 디버그와 같은 규칙이다(`CXMRMarkerDebugComponent.cpp:235`).
  - 명세 범위 밖 변경 없음, 애셋 변경 0.
- **2026-10-15 시험을 막는 문제는 없다.**

### R1-1 레벨에 놓은 액터의 크기가 첫 마커 이벤트에 1로 돌아감 — 반영함 (5930e58)

- 문제: `HandleMarkerPose`가 `SetActorTransform(MarkerToActor * FTransform(Rotation, Position))`로 옮긴다(`CXMRMarkerFollower.cpp:141`). 마커 포즈의 크기는 1이라서, 사용자가 레벨에 놓은 `BP_Dynamic` 액터를 Scale 0.3으로 줄여 두면 첫 감지에서 1로 커진다. 안내 문서는 "BP 안의 메시 크기를 맞춘다"고 했지만, 레벨에서 액터 크기를 바꾸는 사람이 흔하다.
- 제안: 위치·회전만 마커에서 받고 액터의 지금 크기는 유지한다. 예: 합성 결과에 `SetScale3D(GetActorScale3D())` 뒤 `SetActorTransform`, 또는 `SetActorLocationAndRotation`.
- 확인할 시험: 액터 크기를 (0.3, 0.3, 0.3)으로 두고 감지·이동 → 위치·회전은 마커를 따르고 크기는 그대로.

### R1-2 시작할 때마다 "Marker tracking is off" 경고가 찍힘 — 반영함 (5930e58)

- 문제: BeginPlay가 `HandleTrackingChanged(Subsystem->IsMarkerTrackingOn())`을 부른다(`CXMRMarkerFollower.cpp:52`). 그런데 마커 추적은 Placement가 세션이 뜬 뒤 스스로 켠다(`CXMRPlacementComponent.cpp:492-519`). 그래서 시작 시점에는 거의 항상 꺼져 있고, 매번 "Press V or turn it on" 경고가 남는다. 몇 초 뒤 추적이 저절로 켜지므로 시험하는 사람이 잘못 따라 할 수 있다.
- 제안: BeginPlay에서는 상태만 맞추고 경고하지 않는다. 경고는 "추적이 켜져 있다가 꺼졌을 때"만 한다. 또는 세션이 뜬 뒤에도 일정 시간(마커 추적 시작 재시도와 같은 20초) 꺼져 있을 때만 한다.
- 확인할 시험: 추적 꺼진 상태로 BeginPlay → 경고 없음. 켜졌다 꺼짐 → 경고 한 번.

### 1번 작업으로 넘기는 것 (이 브랜치에서 고치지 않음)

- **Learn marker layout이 박스 마커를 차량 보정 마커로 등록한다.**
  - `LearnMarkerLayout`은 지금 보이는 마커 중 프로필에 없는 것을 전부 보정 마커로 더한다(`CXMRPlacementComponent.cpp:799-830`). Placement는 모든 마커를 `SeenMarkers`에 담는다(`:247-251`).
  - 박스가 보이는 채로 누르면 움직이는 마커가 차 배치에 들어간다. 컨트롤 창의 "N in view" 수(`CurrentMarkerCount`, `:186-193`)에도 박스 마커가 섞인다.
  - Placement 코드라 0번 범위 밖이다. 1번(oct-review-fixes U-1)에 "Marker Follower가 쓰는 ID는 학습·관측 수에서 뺀다"를 더한다.
  - 2026-10-15 시험에서는 **박스가 보이는 상태로 Learn marker layout을 누르지 않는다**(체크리스트 §15-4에 적음).

## 질문

없음.

# Codex 리뷰 반영 결과 — 2026-10-14

## ① 바뀐 파일

- `Private/CXMRMarkerFollower.cpp`, `Public/CXMRMarkerFollower.h`: 레벨 액터 크기 유지, 시작 상태는 조용히 읽고 켜짐→꺼짐 때만 경고 한 번.
- `Private/Tests/CXMRMarkerFollowerTests.cpp`: 기존 4개 시험의 시작 경고 기대 제거, PreservePlacedScale·TrackingOffWarningAfterEnabled 추가.
- `Docs/Headset-Test-Checklist.md`: §15-4의 두 기존 결함 안내를 수정 뒤 확인 절차로 교체.
- `Docs/Reviews/marker-follower.md`: R1-1·R1-2 상태와 이 보고.

## ② 완료 조건·리뷰 상태

- R1-1·R1-2 반영. 0.3 배율 액터의 감지·이동 위치/회전과 배율 유지, 시작 추적 꺼짐·중복 off 무경고, on→off 경고 한 번 확인.
- 명세 1·2: 합성 시험 통과. 런타임 모드 호출의 기존 대체 확인 한계는 그대로.
- 명세 3: 공개 BP 프로퍼티 변경 없음. 이전 임시 BP 확인 증거를 유지하며 이번에는 에디터를 열지 않음.
- 명세 4: -DisableAdaptiveUnity 빌드 성공, 경고·오류 0. CXMR. 발견/완료/성공 84, 실패 0, 종료 0.
- 명세 5: 바이너리·Config 변경 0. 만드는 법은 그대로, 결함 안내만 갱신.

## ③ 명세와 다르게 한 것

- 리뷰 제안의 켜짐→꺼짐 조건 선택. 시작부터 꺼져 있을 때 경고하던 기존 경계 동작을 R1-2 요청에 따라 수정함. 새 타이머 없음.
- 모드 요청·차량 보정·학습은 변경하지 않음. Learn의 박스 마커 제외는 1번 작업으로 넘김.
- 전체 빌드·시험은 변경 끝에 각 1회 실행. 임시 BP 확인 재실행 없음(공개 BP 표면 변경 없음).

## ④ API 소스 확인

- `C:/Program Files/Epic Games/UE_5.7/Engine/Source/Runtime/Core/Public/Math/TransformVectorized.h:947`: SetScale3D.
- `C:/Program Files/Epic Games/UE_5.7/Engine/Source/Runtime/Engine/Classes/GameFramework/Actor.h:1713`: GetActorScale3D. SetActorTransform은 기존 경로 유지.
- `Plugins/CXMR/Source/CXMR/Public/CXMRSubsystem.h:158`: IsMarkerTrackingOn은 캐시 getter. `Private/CXMRSubsystem.cpp:565-568`: 실제 상태 변경 때 델리게이트. 새 Varjo API 호출 없음.

## ⑤ 로컬에서 확인할 것

- 빌드 로그: `Saved/MarkerFollowerReviewBuild1014.log`.
- 전체 시험 로그: `Saved/MarkerFollowerReviewTests1014.log`, 84/84, 종료 0. 자동화 초기 엔진 리소스 경고·nohmd OpenXR 메시지는 빌드 경고와 구분함.
- 에디터: 사용자 `/Game/` BP 액터 Scale 0.3으로 두고 감지·이동 뒤 크기 확인. 기존 Details 확인은 `Saved/MarkerFollowerDetails1014.png`.
- 헤드셋: not-tested. 체크리스트 §15-1·15-2 준비, §15-4 크기 유지·시작/추적 중단 경고 및 Stationary/Dynamic 비교, §15-5 기록.
- 질문 없음. push·머지 없음.

## 2회차 — 2026-10-14, `5930e58`

- 재현(Claude, worktree): `-DisableAdaptiveUnity` 빌드 성공, CXMR·GMTCK_MR 경고·오류 0. `CXMR.` 84/84(새 시험 2개 포함).
- R1-1 → **반영 확인 (`5930e58`).** 마커 포즈에 액터의 지금 크기를 넣어 옮긴다(`CXMRMarkerFollower.cpp:142-145`). 시험 `CXMR.MarkerFollower.PreservePlacedScale`는 감지·이동 두 번 모두 크기 0.3 유지를 확인한다.
- R1-2 → **반영 확인 (`5930e58`).** 시작할 때는 상태만 맞추고(`:52-53`), 켜져 있다가 꺼질 때 한 번만 경고한다(`:164`). 시험 `CXMR.MarkerFollower.TrackingOffWarningAfterEnabled`는 처음 꺼짐에는 경고가 없고, 켜졌다 꺼지면 한 번 경고하는 것을 확인한다.
- **열린 항목 없음 — 리뷰 통과.** 헤드셋 시험 상태는 not-tested다(체크리스트 §15-4).

