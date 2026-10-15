# CXMR 사용법

이 프로젝트를 **어떻게 작동시키는가**. 마커 번호를 바꾸고, 차량을 갈아끼우고, 세션을 돌리는 실무 절차.

| 문서 | 용도 |
|---|---|
| **이 문서** | 사용법 — 뭘 어디서 바꾸고 어떻게 돌리는가 |
| `Headset-Test-Checklist.md` | 헤드셋 실측 순서와 "정상인데 안 되는 것처럼 보이는" 경우 |
| `Varjo-Capabilities.md` | Varjo가 실제로 뭘 지원하는가 (사실관계 원전) |
| `Passthrough-Black-Screen.md` | **MR을 켰는데 검은 화면일 때** — 콘솔만으로 원인 가리기 |
| `MR-Environment.md` | **방을 어떻게 꾸미는가** — 검정 천·조명·바닥, 그리고 전후를 수치로 재는 법 |
| `Editor-Followup.md` | 에디터가 열려 있어야 하는 미완 작업 (PP_MR 알파, WBP 행, 마커 ID) |

**애셋 경로 규칙**: `/CXMR/...` = 플러그인(템플릿 공통, 건드리면 모든 프로그램에 영향).
`/Game/...` = 이 프로젝트(프로그램별 데이터). **프로그램마다 다른 값은 전부 `/Game/`에 둔다.**

---

## 1. 5분 시작

1. `GMTCK_MR.uproject` 열기
2. `/Game/Map/L_Main` 열기 → **Play**
3. 보여야 하는 것: 앞쪽 2.5m에 흰 큐브 차량과 모니터의 **CXMR Control 통합 창**(진행자용, §7).
   착용자는 실물 차량·부품을 보고, 진행자는 데스크톱 창을 조작한다(§7)

GameMode(`BP_CXMRGameMode`)가 `BP_CXMRPawn`을 자동 스폰한다. 레벨에 pawn을 배치할 필요 없다.

---

## 2. 조작 전체

### 키보드 (데스크톱 테스트용)

| 키 | 기능 |
|---|---|
| `M` | Mixed Reality (패스스루) on/off |
| `B` | VR 배경(하늘·바닥·안개) 보이기/숨기기 |
| `N` | Masking — 마스크 메시 모양대로 실제 세계 뚫기 |
| `K` | View Offset — 눈 위치 ↔ 패스스루 카메라 위치. **시작 값은 헤드셋 기본을 따른다(VR = 눈, MR = 카메라).** `K`나 튜닝 창으로 한 번 고르면 MR을 켜고 꺼도 그 값을 유지한다. `K`는 **0.5초에 걸쳐 부드럽게** 옮겨 간다(§7 Varjo 예제 기능) |
| `T` | Depth Test |
| `U` | Environment Depth Estimation |
| `Y` | **Depth Test Range on/off** (§2-1) |
| `←` `→` | **Depth Range NearZ 감소/증가** |
| `↓` `↑` | **Depth Range FarZ 감소/증가** |
| `V` | 마커 추적 on/off (MarkerAnchor 차량은 시작할 때 자동으로 켜진다) |
| `H` | 손 추적 시각화 |
| `R` | 재캘리브레이션 |
| `E` | 차량을 내 앞에 배치 |

### 넘버패드 — 차량 위치 미세조정 (§3-1)

**처음 조정을 시작할 때 내가 보고 있던 방향 기준**이다(2026-09-14 시점 기준으로 변경, **09-21 고정**).
차가 어느 쪽을 향해 있든 같은 키는 같은 방향으로 움직이고, **중간에 고개를 돌려도 축이 따라 돌지 않는다.**

| 키 | 차가 움직이는 방향 |
|---|---|
| `NumPad 7` / `NumPad 9` | 내게서 멀어짐 / 가까워짐 (머리 방향을 수평으로 편 기준) |
| `NumPad 1` / `NumPad 3` | 왼쪽 / 오른쪽 |
| `NumPad 0` / `NumPad .` | 위 / 아래 |
| `NumPad 4` / `NumPad 6` | 왼쪽으로 돌기 / 오른쪽으로 돌기 (위에서 볼 때 반시계 / 시계) |
| **저장** `NumPad Enter` | 한 번 누르면 새 평균 캡처 후 자동 저장. 필요 마커 전부를 계속 보이게 유지 |
| **초기화** `NumPad *` | 조정값을 0으로 |

### ★ 앞뒤·좌우가 꼬일 때 — `Keys move along` (2026-09-21)

**증상**: 키를 눌러도 원하는 대로 안 가고, `7`로 밀었다가 `9`로 당기면 제자리로 안 돌아온다.

**원인**: 축을 **누르는 순간의 머리 방향**에서 매번 새로 계산했다. 차를 맞추려면 A필러·콘솔·문턱을 번갈아 보게
되는데, 그때마다 "앞"이 달라졌다. 키를 **누른 채로** 고개를 돌리면 차가 곡선을 그리며 끌려갔다.
실측: 정면에서 10cm 밀고, 고개를 90° 돌린 채 10cm 당기면 **14.1cm 어긋난 채로 남는다**(되돌아오지 않는다).

**고침**: 축을 **처음 한 번 잡아서 고정**한다. 튜닝 창 `Vehicle placement → Keys move along`:

| 값 | 뜻 |
|---|---|
| **`Where I first faced`** (기본) | **처음 조정한 순간의 내 방향으로 고정.** 고개를 돌려도 축이 그대로다 → `7` 다음 `9`는 정확히 제자리 |
| `Where I look now` | 지금 보는 쪽 (09-21 이전 동작). 비교용으로만 |
| `The car's own axes` | 차의 앞/오른쪽(수평으로 편 것). 내가 어디를 보든 "멀어짐" = 차 앞쪽 |

- `"Away from me" is` 줄이 **지금 축이 내 시선에서 몇 도 틀어져 있는지** 알려 준다. 키가 옆으로 가는 느낌이면 이 숫자를 본다
- 축이 비뚤게 잡혔으면 **`Use the way I face now`**를 눌러 지금 보는 방향으로 다시 잡는다
- **마커를 다시 읽거나(`Re-read markers`) 차량을 바꾸면 축도 풀린다** — 새 정렬은 새로 잡는다

**회전 중심**은 `VehicleRoot → Placement → Nudge Pivot`이다. MarkerAnchor에서 Driver Eye Reference가 유효하면 그 눈 기준을 우선한다. 눈 기준이 없을 때 기본 `Markers`는 보이는 캘리브레이션 마커들의 중심이라,
마커 옆에서 먼저 맞춰 둔 부분이 돌려도 제자리에 남는다. `Viewer`(내 머리), `VehicleOrigin`(차 원점)도 고를 수 있다.

고급 창 Calibration 탭은 **차의 월드 위치**를 보여 준다(예전 패널의 X/Y/Z/Yaw는 차 기준 내부값이라 키 방향과 부호가 맞지 않았다).

⚠️ **NumLock이 꺼져 있으면 하나도 안 먹는다.** 꺼진 상태에서는 NumPad 7이 Home, 9가 PageUp으로
전달되어 매핑이 통째로 빗나간다. 아무 반응이 없으면 NumLock부터 확인한다.

이전에는 **차 기준 축 + 반대 방향**이라, 차가 돌아가 있거나 기울어 있으면 "왼쪽으로 돌기가 위로 움직이는" 식으로
축이 꼬였다.

### 마커가 비스듬히 붙어 있으면 차도 기우나 — 아니다

**`Placement → Keep Level`(기본 켬)이면 마커의 기울기는 버리고 방향(yaw)만 쓴다.** 마커를 벽이나 경사면에
비스듬히 붙여도 **차는 항상 수평**으로 놓인다.

- 왜: 마커 2개 이상일 때 쓰는 해법이 원래 수평을 가정한다. 1개일 때만 기울기를 통과시키면 **두 번째 마커가 보이는
  순간 차가 튄다.** 게다가 기울어진 차는 **조정 키의 축까지 기울여** 놓는다
- 끄면(= 마커 기울기를 그대로 따름) **차가 진짜로 기울어 서 있는 경우에만** 쓴다. 튜닝 창 `Keep level`
- ⚠️ **yaw는 따라간다.** 마커를 수평면에서 돌려 붙이면 차도 그만큼 돌아간다 — 기울기(pitch/roll)만 버린다

조정 속도는 `BP_CXMRPawn → VarjoInput → Offset Adjust Speed`(cm·deg/초, 기본 10).

### 아무 반응이 없는 키 — 제거된 매핑

`G` 시선 점, `I` foveation 오버레이, `F` 손목 클릭, `C` 전체 마커 Dynamic 전환은 매핑에서 제거했다.
턴테이블·트림 입력도 제거됐다. 키가 고장 난 것이 아니며, 마커별 Tracking Mode는 프로필에서 정한다.
`Y`와 방향키는 깊이 범위에 배선돼 있다. 넘버패드는 **NumLock**부터 확인한다.

### 2-1. Depth Test Range ★ flickering의 원인

**Depth Test를 켰는데 가상 물체가 심하게 깜빡인다면 거의 항상 range 문제다.**

Varjo 플러그인은 range가 **비활성이면 컴포지터에 `farZ = HUGE_VALF`를 넘긴다**
(`DepthPlugin.cpp:58-59`). 즉 **방 전체를 무한 거리까지** depth test 대상으로 삼는다.
추정 depth는 반사면·검은 표면·원거리에서 불안정하므로(§Varjo-Capabilities §3), 경계가 매 프레임
요동치며 이것이 flickering으로 보인다.

CXMR은 이제 range를 **기본으로 켜고 0.0–0.75m로 시작**한다. `Y`로 끄고 켤 수 있으며,
방향키로 경계를 실시간 조절한다. 패널에 현재 값이 표시된다(range가 꺼져 있으면 `unbounded`).

⚠️ **range 밖은 실세계가 통째로 사라진다.** 좁힐수록 안전한 게 아니라 **구멍이 커진다.**
손만 보이면 되면 0.75m, 대시보드까지면 1.2m 정도부터 시험한다.

조절 속도는 `BP_CXMRPawn → VarjoInput → Depth Range Adjust Speed`(m/s, 기본 0.5).

### 컨트롤러 (Varjo XR-4)

| 입력 | 기능 |
|---|---|
| 왼쪽 `A` / `B` | 좌/우 연속 회전 토글 |
| 오른쪽 스틱 상/하 | 차량 순환 |
| (미배정) | 착좌 위치 순환 — `CycleManikinAction`에 IA를 지정하면 동작 (§6) |

⚠️ 스틱 순환은 히스테리시스가 걸려 있다(0.6 진입 / 0.3 복귀). 한 번 튕기면 한 칸만 넘어간다.

---

## 3. 마커 설정 바꾸기 ★ 가장 자주 하는 작업

**편집 위치**: `/Game/Vehicle/Example/DA_MarkerProfile_TestCar`
(프로그램마다 새로 만든다 — `UCXMRMarkerProfile` 타입의 Data Asset)

`Markers` 배열에 엔트리를 추가한다. 엔트리 하나 = 물리 마커 하나.

| 필드 | 의미 |
|---|---|
| **Marker Id** | 마커에 인쇄된 번호. **이것이 안 맞으면 그 마커는 통째로 무시된다** |
| **Role** | `Calibration`(차량 정렬) / `DynamicObject`(움직이는 실물 추종, 미구현) / `Reserved` |
| **Local Offset** | **마커가 차량 기준 어디에 있는가** — 아래 참조 |
| **Tracking Mode** | `Stationary`(고정물, 필터 강함) / `Dynamic`(움직이는 것, 예측 on) |
| **Timeout** | 마커를 놓친 뒤 유지할 시간(초) |
| **Label** | 사람이 읽을 이름 (에디터 목록 표시용) |
| **Target Tag** | DynamicObject가 따라갈 액터 태그 (지금은 미사용) |

### Local Offset — 여기서 대부분 틀린다

**Local Offset은 "마커가 차량 좌표계에서 어디에 있는가"다.** 차량의 위치가 아니다.

계산은 이렇게 돈다:

```
차량월드 = Local Offset⁻¹ × 마커월드
```

즉 *"이 마커는 차 원점에서 앞으로 120cm, 위로 80cm에 붙어 있다"*고 알려주면, 시스템이 마커를
찾았을 때 **차를 거꾸로 계산해서** 그 자리에 놓는다.

**실측 절차** (설계 단계에서 부착 지점을 정할 수 있을 때):
1. 차량 3D 모델에서 마커를 붙일 지점의 좌표를 읽는다 (차 원점 기준, cm)
2. 실물에도 **같은 자리에** 마커를 붙인다
3. 그 좌표를 Local Offset의 Translation에 넣는다
4. 마커가 기울어져 붙는다면 Rotation도 넣는다 (대시보드 경사 등)

**Local Offset을 0으로 두면** 차량 원점이 마커 위에 정확히 얹힌다. 테스트할 땐 이게 편하다.

현장에서 마커를 손으로 붙이는 경우는 이 실측이 불가능하다 → **§3-1**로.

---

## 3-1. 현장 캘리브레이션 ★ 마커 위치를 모를 때

clay 모형에 마커를 손으로 붙이면 **붙인 자리를 차량 좌표계로 잴 방법이 없다.** authored
Local Offset은 그때 의미가 없다. 그래서 반대로 한다 — **눈으로 맞춘 뒤, 그 상태에서 마커가
어디 있었는지를 기록**한다.

### 절차

1. 마커를 잘 보이는 고정 부위에 붙이고, 사용할 번호를 차량 Marker Profile에 `Calibration`으로 등록한다.
2. 차량 Data Asset의 `Driver Eye Reference`를 설정하고 §7의 `Initial Alignment`로 대략 맞춘다.
   기존 `E` 수동 배치도 사용할 수 있다. 마커 추적은 XR 세션 준비 뒤 자동으로 시작한다.
3. 넘버패드나 Placement의 `− / +`로 실물 기준과 맞춘다.
4. 차량과 마커를 고정한 채 등록된 보정 마커 전부를 시야에 넣는다. Marker Follower가 쓰는 ID는 보정에서 제외한다.
5. `Confirm fit`, `Save alignment` 또는 넘버패드 `Enter`를 **한 번** 누른다. 이전 관측은 저장 표본에 쓰지 않는다.
6. 마커당 새 표본 10개가 모이면 평균으로 자동 저장한다. 저장 중 Lost·0.5초 관측 만료·차량 이동·교체가 생기면 요청 전체를 취소한다. 10초 안에 표본을 채우지 못해도 취소한다. 빠진 ID·부족한 표본 수·실패 이유가 표시되며, 다시 누르면 새 캡처를 시작한다.

다음 실행은 필요한 마커 **전부**가 관측되고 정착이 끝나야 복원한다. 하나라도 빠지면 차를 움직이지 않고 빠진 ID를 표시한다.
필요 마커가 한 개인 프로필은 그 한 개로 보정·복원할 수 있다. 미세조정·눈 정렬처럼 사용자가 직접 차를 옮기는 기능은 그대로다.
`CXMR.LearnMarkers`도 등록된 필요 마커 전부가 관측 중일 때만 학습한다. 미등록 번호 추가는 유지하되 Marker Follower 번호는 추가하지 않는다.
원본 초기화는 관측 없이 원본 오프셋 파일을 쓰므로 이 저장 조건에서 제외한다.

### 왜 마커가 필요한가 — 첫 정렬은 어차피 수동인데

**마커는 첫 정렬을 자동화하는 물건이 아니라, 한 번 맞춘 것을 세션을 넘어 재현하는 물건이다.**

앱이나 헤드셋을 다시 켰을 때 **트래킹 원점이 유지되는지는 추적 환경에 달려 있다.** 저장은 차량 Root 월드 포즈(`worldPose`)와 같은 캡처 평균의 마커 월드 좌표(`markerWorld`)도 기록한다.
복원 시 모든 마커 평균이 저장 위치에서 0.3cm 이내이면 저장한 차량 월드 포즈를 그대로 쓴다. 단일 마커는 yaw 차이도 0.3deg 이내여야 한다.
넘으면 저장된 마커↔차량 관계로 다시 계산한다. 새 필드가 없는 기존 파일과 원본 초기화 파일도 마커 기준으로 복원한다.
상태와 `LogCXMRAlignment`에 `Restored saved world pose` 또는 `Restored from markers`와 최대 원점 이동 거리(cm)가 표시된다.
`CXMR.DumpMarkers`로 현재 마커 월드 위치·회전(cm·deg)을 로그에 남긴다. 헤드셋 검증은 별도이며 아직 `not-tested`다.

| | 마커 없음 | 마커 + 학습 |
|---|---|---|
| 설치 (1회) | 눈으로 맞춤 | 눈으로 맞춤 + 학습 |
| 앱 재시작 | **다시 맞춤** | 자동 |
| 헤드셋 재부팅 | **다시 맞춤** | 자동 |
| 착용자 교체 | **다시 맞춤** | 자동 |
| 시간 경과 드리프트 | 밀린 채로 | 마커 보일 때마다 보정 |

1~2주 운영이면 재시작이 수십 번이다. **설치 1회의 수동 정렬 대 수십 번의 수동 정렬**이 차이다.

### 저장 위치

```
Saved/CXMR/MarkerCalib_<id>_<vehicle-key>.json          ← 현재 값
Saved/CXMR/MarkerCalib_<id>_<vehicle-key>.startup.json  ← 세션 시작 시 백업
```

`<id>`는 마커 프로필의 **Calibration Id**다. `<vehicle-key>`는 차량 Data Asset 경로의 해시다.
같은 마커 프로필을 쓰는 차량도 별도 파일을 사용한다. 기존 마커 전용 파일은 차량별 파일이 없을 때 읽는다.

`.startup`은 **되돌리기용**이다. 하루 종일 만지다 망쳐도 아침 상태로 갈 수 있다 —
`.startup.json`을 `.json`으로 복사하고 다시 로드하면 된다. 실수는 보통 연속으로 일어나므로
"직전 값"보다 "오늘 시작 전"이 실제로 필요한 지점이다.

내용은 **앵커 기준 마커 포즈**, 검증한 마커 ID, 기준 ID와 차량 식별 정보다. 새 정렬 저장은 차량 월드 포즈와 저장에 쓴 평균 마커 월드 포즈도 포함한다. 원점 비교 후 그대로 재사용하거나 마커 기준으로 계산한다.

기존 파일 형식 예시다. 새 정렬 저장에는 `alignmentVersion`, `alignmentMarkers`, `primaryMarker`와 차량 식별 정보도 들어간다.

```json
{
  "profile": "DA_MarkerProfile_TestCar",
  "units": "cm, degrees; marker pose in anchor-local space",
  "markers": [
    { "id": 5, "label": "A", "x": 210.4, "y": -85.2, "z": 12.0, "pitch": 0, "yaw": 90.0, "roll": 0 }
  ]
}
```

**텍스트로 둔 이유**: 원격 지원에서 "그 파일 열어서 숫자 불러주세요"가 되고, 현장 실측값을
메일로 되받아 다음 납품에 반영할 수 있다.

⚠️ **쿠킹된 빌드는 자기 데이터 애셋을 저장하지 못한다.** `MarkPackageDirty()`는 에디터에서만
의미가 있다. 이 JSON이 없으면 **앱을 끄는 순간 캘리브레이션이 사라진다.**

### 콘솔 명령

| 명령 | 하는 일 |
|---|---|
| `CXMR.LearnMarkers` | 지금 차량 포즈 기준으로 마커 배치를 학습하고 저장 |
| `CXMR.SaveCalibration` | 현재 값을 복원 정보와 함께 재저장. 수동 조정 후에는 확인·캡처 절차를 따름 |
| `CXMR.ResetCalibration` | 원본 마커 값으로 복원. 차량별 원본 저장으로 이전 공용 파일 재적용 방지 |

`NumPad Enter` 한 번으로 새 캡처·자동 저장을 요청한다. 모든 필요 마커를 유지하고 완료를 기다린다.

### 마커를 어디에 붙일 것인가

| 원칙 | 이유 |
|---|---|
| **움직이는 부품은 금지** | 문·후드·트렁크에 붙이면 **열리는 순간 캘리브레이션이 깨진다** |
| 3개 이상 | 하나 가려져도 버틴다 |
| **서로 최대한 멀리** | 마커 사이 기준선이 길수록 yaw가 안정된다 |
| 높이를 다르게 | 전부 한 평면이면 그 평면 수직 방향 추정이 약해진다 |
| 사람이 서는 위치에서 보이게 | 리뷰 중에도 계속 보정된다 |

차 외부가 실내보다 낫다 — 실내는 좁아 시야에 안 들어오는 각도가 많고 어둡다.
좌우 펜더에 하나씩(기준선 확보) + 루프나 후드에 하나(높이 차) 정도가 좋은 분포다.

clay 표면 보호도 감안한다. 직접 붙이면 자국이 남으므로 저점착 테이프나 별도 스탠드를 쓴다.
**받침대에 붙인다면 clay와 받침대가 절대 움직이지 않아야 한다** — 청소하려고 살짝만 밀어도
전부 틀어진다.

### 한계

- 학습 시점에 **등록된 필요 마커 전부가 관측 중이어야** 한다. 부족하면 학습·저장을 거부하고 빠진 ID를 표시한다
- 마커가 물리적으로 밀리면 저장값이 거짓이 된다 → 다시 Learn
- id 0은 Varjo가 무효 값으로 쓰므로 학습하지 않는다
- 마커가 기울어 붙어 있어도(수직면 등) `Keep Level`이 켜져 있으면 차는 수평으로 놓인다

### 마커 몇 개를 쓸 것인가

`VehicleRoot` 액터 → `Placement` 컴포넌트에서 설정한다.

| 설정 | 의미 |
|---|---|
| **Mode** | `MarkerAnchor`(마커 정렬) / `PawnRelative`(내 앞에 수동 배치) |
| **필요 마커** | 프로필의 유효한 Calibration ID 전부. Marker Follower ID 제외. 별도 최소 개수 설정은 제거됨 |
| **Freeze After Calibration** | 완료 후 이 컴포넌트가 차량 재배치를 멈춤 |
| **Stop Marker Tracking When Calibrated** | ⚠️ **기본 off로 둘 것.** 켜면 헤드셋의 마커 추적이 **전역으로** 꺼져서 DynamicObject 마커까지 죽는다 |
| **Start Marker Tracking On Begin Play** | 기본 on. 시작할 때 마커 추적을 켠다(세션이 올라올 때까지 20초 재시도) |
| **Calibration Settle Seconds** | 기본 1.5. 마커가 충분히 보인 뒤 이 시간 동안 갱신을 받고 고정한다 — 첫 샘플의 노이즈를 굳히지 않게 |
| **Average Marker Samples** | 기본 on. 정착 시간 동안 받은 마커 표본을 평균낸다. 흔들림(mm)과 표본 수는 고급 창 Calibration의 Marker steadiness에서 확인 |

**단일 마커는 각도 노이즈가 증폭된다**(레버암). 마커 하나의 방향이 1° 틀어지면 3m 떨어진 차 뒤쪽은
5cm 밀린다. **마커 2개 이상을 쓰면** 시스템이 마커 *위치*들 사이의 기준선으로 yaw를 계산하고
roll/pitch는 0으로 고정한다(차는 바닥에 평평히 놓인다는 가정) — 훨씬 안정적이다.

### 지금 프로젝트 상태

`DA_MarkerProfile_TestCar`의 작성값은 이 작업에서 변경하지 않았다. ID 0은 무효이며 보정에 쓰지 않는다.
→ 실물 마커로는 **Learn을 한 번 해야** 레이아웃이 생긴다. 이후 `Saved/CXMR/MarkerCalib_DA_MarkerProfile_TestCar.json`에 남는다.

---

## 3-2. 마커를 따라가는 메시 만들기

1. Content Browser에서 `/Game/` 아래 원하는 폴더에 Blueprint Class를 만들고, 부모로 **CXMR Marker Follower**를 고른다(예: `BP_Dynamic`).
2. BP를 열어 루트 아래에 Static Mesh를 붙인다. 실물 박스 치수에 맞춘다(엔진 큐브는 100 cm, 30 cm 박스면 Scale 0.3).
3. Class Defaults(또는 레벨에 놓은 뒤 Details)에서 `Marker Id`에 박스에 붙인 마커 번호를 넣는다. **차량 보정 마커와 다른 번호**를 쓴다.
4. `Tracking Mode`를 고른다. 움직이는 물체는 Dynamic이다.
5. 레벨에 놓는다. 위치는 상관없다. 마커가 보이면 그 자리로 간다.
6. 메시가 실물 박스와 어긋나면 `Marker To Actor`로 맞춘다. 마커가 박스 윗면 가운데에 있다면, 박스 중심은 마커에서 박스 높이의 절반만큼 면 안쪽이다. 어느 축이 면 바깥인지는 마커 축 표시로 확인한다.

값은 `CXMR → Marker Follower`에 있다. `Follow`를 끄면 현재 위치에서 멈추고, 켜면 다음 마커 이벤트부터 따라간다.
`When Lost` 기본은 Hold(마지막 자리 유지), Hide면 숨겼다가 다시 관측되면 나타난다.
라벨은 `waiting`(아직 관측 안 됨), `lost`(놓침), 런타임이 보고한 `Dynamic`/`Stationary`다. 모드를 읽을 수 없으면 `mode ?`다.
`Marker Id` 0은 동작하지 않는다. 마커 추적이 꺼져 있으면 `V` 또는 컨트롤 창으로 켠다.
키·버튼은 BP의 `Set Follow`/`Set Tracking Mode`에 연결할 수 있으며, `Is Marker Tracked`와 `On Marker Found`/`On Marker Lost`로 색 등을 바꿀 수 있다.
같은 번호의 follower가 둘이면 둘 다 따라가며 마지막 모드 요청이 이긴다. 종료하면 보정 마커는 프로필 모드, 그 밖의 마커는 Stationary로 되돌린다.

이 C++ 부모는 플러그인 코드다. 자식 BP·메시·레벨 배치는 프로젝트의 `/Game/` 데이터로 만들고 `/CXMR/` 공통 애셋은 바꾸지 않는다.
헤드셋 시험은 [체크리스트 §15-4](Headset-Test-Checklist.md#15-4-박스에-붙인-마커를-메시가-잘-따라가는가)로 한다. 로컬 자동화는 실제 추적 지연·떨림을 확인하지 않는다.

---

## 4. 차량 추가 / 교체

### 차량 프로파일 (`UCXMRVehicleProfile`)

`/Game/Vehicle/[프로그램]/DA_Vehicle_XXX`

| 필드 | 의미 |
|---|---|
| **Display Name** | 패널에 표시될 이름 |
| **Vehicle Actor** | 차량 지오메트리를 담은 Actor 클래스(BP) |
| **Marker Profile** | 이 차의 캘리브 설정. **차를 바꾸면 마커 설정도 따라온다** |
| **Vehicle Root Offset** | 모델 피벗이 마커 오프셋 기준과 어긋날 때 보정 |
| **Ergonomics** | 선택적으로 추가한 Ergonomics 컴포넌트의 퍼센타일 착좌 데이터 |

### 차량 목록 (`UCXMRVehicleCatalog`)

`Vehicles` 배열에 프로파일들을 넣으면 차량 순환이 동작한다.
**카탈로그를 지정하지 않으면 차량 순환만 조용히 무시된다**.

### 배선

`VehicleRoot` 액터 → `Loader` 컴포넌트 → `Profile`(시작 차량) / `Catalog`(순환 목록)

⚠️ **차량 액터는 반드시 `VehicleRoot` 아래로 스폰된다.** 직접 레벨에 배치하지 말 것 —
캘리브레이션은 `VehicleRoot`의 앵커를 움직이고, 차량은 그 자식이라 따라온다.
**차를 바꿔도 재캘리브가 필요 없는 이유가 이 구조다.**

---

## 5. 배경과 마스크 오브젝트

액터에 **`CXMR Scene Object` 컴포넌트**를 붙이고 Role만 고르면 된다. 중앙 목록은 없다 —
**역할이 없는 액터는 항상 보인다.**

| Role | 동작 | 쓰는 곳 |
|---|---|---|
| **VR Only** | VR 배경을 끄면 **화면에서만** 빠진다(조명·반사·스카이라이트에는 남음) | 하늘, 바닥, 벽, 안개, 구름 — **MR에서 실제 세계를 가리는 모든 것** |
| **Mask Mesh** | CustomDepth에만 그려진다(평소 안 보임). 마스킹을 켜면 그 모양대로 패스스루가 뚫린다 | 실물 스티어링 휠·시트 자리에 놓는 프록시 |

⚠️ **새 레벨을 만들면 배경 오브젝트에 VROnly를 붙이는 것을 잊기 쉽다.** 안 붙이면 MR을 켜도
가상 하늘이 남아 "MR이 안 된다"처럼 보인다. `L_Main`에는 SkyAtmosphere / HeightFog /
VolumetricCloud / SkySphere / Floor에 붙어 있다.

**Mask Mesh는 코드가 렌더 플래그 7개를 강제한다**(CustomDepth on, Main·Depth·Shadow·Reflection·
Sky·RayTracing·Decal off). 수동 설정 불필요.

### L_Main 조명 (2026-09-15 조정)

실시간 조명은 그대로 두고, 흰 clay·차 표면의 형태가 보이게 맞췄다(Varjo 예제 맵은 구운 조명이다). 새 레벨도 이 값에서 출발한다.

| 항목 | 값 | 이유 |
|---|---|---|
| 태양 (Directional Light) | **2.5 lux**, pitch −40 / yaw 40 | 6 lux에서는 흰 면이 포화됐고, 보는 사람 뒤에서 비추면 두 면이 같은 밝기라 형태가 사라졌다 |
| Sky Light | 실시간 캡처, **아래 반구 검정**, 강도 **1.5** | 바닥 반사로 그림자 면이 뜨지 않게, 대신 전체 채움을 조금 올림 |
| 반사 캡처 | 차 주변 Sphere Reflection Capture(반경 15m), 빌드 데이터 `L_Main_BuiltData` | 반사 캡처가 없었다 |

PIE 흰 큐브(노출 0): 밝은 면 228 / 그림자 면 108. VR 배경을 숨겨도(MR) 같은 값이다.

---

## 6. 착좌 위치 (Human Factors)

`UCXMRErgonomicsProfile`의 `Positions[]`: `{Name, Eye Point, bHasHipPoint, Hip Point}`
Eye Point는 **차량 로컬 좌표**다(차와 함께 움직인다).

**조작**: `VarjoInput → Cycle Manikin Action`에 Axis1D IA를 지정하면 스틱 flick으로 순환한다
(차량 순환과 같은 히스테리시스).

컨트롤 창 **Ergonomics 컴포넌트의 RequestErgonomicsStep**으로도 순환한다(2026-09-15부터. 예전 WBP 패널에는 이 행이 없었다).

⚠️ **차량 프로파일에 ergonomics 애셋이 연결돼 있어야 동작한다.** 안 걸려 있으면
`ResolveProfile()`이 null을 반환해 눌러도 조용히 아무 일도 일어나지 않는다.

동작이 모드에 따라 **정반대**다:
- **VR**: 세계가 가상이므로 **내 시점을 옮긴다**
- **MR**: 실제 몸은 못 옮기므로 **차를 옮긴다**
- **MR + MarkerAnchor**: **아무것도 안 옮긴다.** eye point가 이미 물리적으로 실재하므로
  (실물 시트) 차를 옮기면 캘리브레이션과 싸운다. 로그에 이유가 찍힌다

⚠️ Hip Point는 저장만 하고 아직 이동에 쓰지 않는다. VehicleRoot에는 Ergonomics가 기본으로 없으므로 필요한 차량에 명시적으로 추가한다.

---

## 7. 컨트롤 패널 조정

### CXMR Control과 Advanced settings

기본 창 **CXMR Control**(480×820)은 MR/VR, 헤드셋과 정렬 상태를 항상 위에 표시한다. 탭은 **Calibration / Vehicle**이다. 내용이 길면 탭 안만 스크롤한다. 두 창 모두 크기를 바꿀 수 있다.

앱은 기본적으로 MR을 요청하고, OpenXR 세션이 실행되면 **MR → 깊이 테스트 → 깊이 범위 → 실세계 깊이 추정** 순서로 적용한다. 세션 전에는 Varjo 런타임을 호출하지 않는다. 20초 동안은 시작 대기, 이후에는 `Headset not running`이다. 늦게 세션이 시작돼도 요청을 적용한다. `Advanced settings… → MR → Start in MR`을 끄면 다음 실행은 VR로 시작한다.

**Calibration**의 Markers는 프로필의 필요 마커와 관측 여부다. 저장 없이 처음 맞췄거나 조정한 상태에서 모든 마커가 보이면 **Save alignment**를 한 번 누른다. 새 표본을 평균내 자동 저장한다. Enter도 같은 저장 요청이다. 저장 중 마커가 빠지면 취소하고 다시 요청해야 한다. 파일 교체 성공 뒤에만 저장 완료다. 표본 수·시간 제한은 §3-1과 같다.

**Fine-adjust position**은 이동 간격 0.1 / 1 / 5 cm와 Away / Toward / Left / Right / Up / Down / Turn 버튼을 제공한다. 넘버패드와 같은 조정 경로이며 `Change since last save`는 저장·Discard changes·Recalibrate 뒤의 누적 조정량(cm, °)이다. **Discard changes**는 임시 조정만 버린다. **Recalibrate**는 마커를 다시 읽는다.

**처음 목업을 맞출 때**:
1. 차량 Data Asset에서 `Has Driver Eye Reference`를 켜고, 원본 차량 액터 로컬 좌표로 눈 위치와 정면(+X)을 넣는다. `VehicleRootOffset`은 별도로 유지한다.
2. `/Game/`의 Marker Profile에 고정 보정 마커 ID와 `Calibration` 역할을 설정한다. Marker Follower가 쓰는 ID는 제외한다.
3. 운전자석에 자연스럽게 앉아 정면을 보고 **First-time setup for a new rig → Align to my eyes**를 누른다. 3초 뒤 유효한 HMD 위치·yaw로 한 번 배치한다. 다시 누르면 취소한다.
4. 실물과 비교해 미세조정하고 모든 마커가 보이는 동안 **Save alignment**를 한 번 누른다.

다음 실행에는 모든 필요 마커의 정착 평균 뒤 자동 복원한다. 원점이 유지됐으면 저장 월드 포즈, 움직였거나 구 파일이면 마커로 재계산한다. 이미 완료한 정렬은 가림·재관측으로 움직이지 않는다. 실물 마커·모델 오프셋·좌석이 달라지면 다시 맞춰 저장한다.

**Vehicle**은 Loader 카탈로그의 실제 차량을 선택한다. `Alignment group`은 그룹이 있는 경우만 보인다. 같은 목업의 차량은 같은 그룹과 같은 마커 구성을 쓰고 모델 원점 차이는 `VehicleRootOffset`으로 맞춘다. 그룹 이름을 바꾼 뒤에는 차량을 다시 로드한다. 그룹 저장은 `Saved/CXMR/AlignmentGroup_*.json`, 그룹이 `None`이면 차량별 `MarkerCalib_*.json`이다. 다른 마커 구성은 그룹 저장을 적용·덮어쓰지 않는다.

**Advanced settings…**는 별도 창(560×760)을 연다. 다시 누르면 기존 창이 앞으로 온다. X로 숨긴 뒤 다시 열면 같은 위치·탭이며, 기본 창을 닫거나 PIE를 끝내면 둘 다 닫힌다. 에디터 `CXMR Settings` 버튼도 이 창을 연다. `CXMR Control` 버튼과 콘솔 `CXMR.Tuning`은 기본 창을 연다.

| 고급 탭 | 내용 |
|---|---|
| MR | 시작 모드, 깊이·범위, MR 가상 노출, Render from, Scene masks |
| VR | VR 가상 노출, Virtual background |
| Calibration | 관측 수치, 마커 평균·원점 허용 오차, 조정 방향·회전 중심, 레이아웃 학습·공용 저장 삭제 |
| Diagnostics | 손·마커 오버레이, 손 정렬, 모니터 카메라, 마커 추적·Dynamic·로그 |

행의 문구·도움말은 창에 보인다. MR/VR 행은 **그 모드에서 적용할 값**이며 다른 모드에서도 바꿀 수 있다. MR의 깊이 값과 VR의 배경 값은 모드 전환으로 바뀌지 않는다. `B`는 VR에서는 배경 설정을 바꾸고, MR에서는 실제 배경만 임시로 토글한다. 마커 추적을 끄면 자동 시작도 하지 않는다.

아래 변경 수, 탭의 `●`, 행의 `Changed · default …`는 **고급 창에 행이 있는 설정 중 Default와 다른 값**이다. 저장 여부와 무관하다. H 키로 켠 손 표시나 세션용 Dynamic도 센다. 탭의 `Reset … to defaults`는 그 탭 설정만 되돌린다. Snap으로 저장하는 손 보정 결과는 창에 행이 없으므로 세지도 초기화하지도 않는다.

영속화 대상은 조작을 마칠 때 `Saved/CXMR/Tuning.json`에 저장한다. 깊이 테스트·실세계 깊이 추정은 저장된다. 마커 추적·Dynamic·오버레이·배경은 세션 설정이다. `Calibration → Delete saved alignment for <group>`은 **Delete → Click again to delete**를 5초 안에 눌러야 삭제된다. 손 보정은 `Diagnostics → Snap hand to marker`를 다시 실행한다.

헤드셋·패키지 검증은 별도다. 이 PC의 합성 Slate 미리보기는 실제 패스스루·마커·손 추적 검증이 아니다. 게임 단축키는 게임 화면에 포커스를 둔다. 기본 창에 포커스가 있어도 Enter는 저장 요청이다.

### MR/VR 노출 프리셋

**MR - Real environment**는 실제 방, **VR - Virtual environment**는 가상 장면이다.
헤드셋 세션이 없으면 하드웨어 설정은 사용할 수 없으며, MR 전용 깊이·마스킹 설정은 VR에서 비활성화된다.
렌더러는 같은 구성을 유지한다. 실제 foveated rendering은 남아 있고 디버그 오버레이만 제거했다.

`MR virtual exposure`와 `VR virtual exposure`는 각각 저장한다. 패스스루 카메라 밝기가 아니라
unbound Post Process Volume의 가상 장면 노출(EV)이다. 카메라 노출은 Varjo Base에서 정한다.
저장값도 사용자 조작도 없는 모드는 레벨의 원래 override 여부와 bias를 유지한다.
한 모드만 조정했다면 다른 모드로 전환할 때 레벨 기준값으로 돌아간다.
기존 `Tuning.json`의 `Display.Exposure`만 있고 새 두 키가 모두 없으면 그 값을 MR·VR 양쪽 초기 프리셋으로 쓴다.

디자인 옵션은 `project/pqdq-usb-access` 브랜치에 있다.

### 마이그레이션과 범위

VehicleRoot는 VehicleAnchor + Placement + Loader를 중심으로 쓴다. VehicleAnchor는 캘리브레이션만 움직인다.
턴테이블·트림/CMF·손목 클릭·핀치 잡기·박스 맞추기·플러그 끝 추정·시선 점·foveation 디버그 표시는 제거했다.
마커 진단·level-fit·실제 생성 손 접촉은 남아 있다. Ergonomics는 필요할 때 명시적으로 추가하는 선택 API다.

번들 입력/Pawn/Root 이관은 `Scripts/MigrateDesignReviewAssets.py`, 읽기 검증은 `Scripts/VerifyDesignReviewAssets.py`다.
프로그램별 Blueprint에 옛 컴포넌트·입력·함수 호출이 남았다면 제거 후 컴파일한다.
옛 트림 설정이 조립체로 자동 변환되거나 실제 CAD 차량이 자동 분해되지는 않는다.
조립체 Construction/BeginPlay가 외부 액터·파일·전역 상태를 바꾸면 실패한 교체가 그 부작용까지 되돌릴 수 없다.
실물 케이블 깊이·주관적 피드백·설계 우열은 현장에서 확인한다. 템플릿의 손 접촉은 삽입 성공이 아니다.

**프로젝트 브랜치에서 항목 추가하기** — 창은 기능을 모르고, 기능이 스스로 등록한다. 템플릿을 고칠 필요가 없다.

```cpp
// BeginPlay
FCXMRTunable T;
T.Id = "Hands.RadiusScale";            // Tuning.json 키 — 바꾸면 저장값을 잃는다
T.Category = INVTEXT("Virtual hands");
T.Label = INVTEXT("Finger thickness");
T.Kind = ECXMRTunableKind::Float;      // Bool / Float / Choice / Stepper / Action / Readout
T.Min = 0.5f; T.Max = 2.0f; T.Delta = 0.05f; T.Default = 1.0f; T.bPersist = true;
T.Get = [this] { return RadiusScale; };
T.Set = [this](float V) { RadiusScale = V; };
T.Owner = this;
GetWorld()->GetGameInstance()->GetSubsystem<UCXMRTuningSubsystem>()->Register(MoveTemp(T));

// EndPlay
Tuning->UnregisterOwner(this);
```

BP·Python에서는 `Set Tunable Value` / `Get Tunable Value` / `Invoke Tunable` / `Get Tunable Text`로 같은 항목을 다룬다.

### Varjo 예제에서 가져온 기능 (2026-09-15)

Varjo 예제 맵(`/Game/VRTemplate/Maps/VRTemplateMap`)에 있고 CXMR에 없던 것들이다. 코드는 전부 CXMR 플러그인에 있고,
애셋은 `/CXMR/`에만 있다(`/Game/` 참조 없음). **헤드셋 실측 전이다** — PIE에는 헤드셋이 없어 실제 foveation·손 추적·
모니터 스펙테이터 화면 자체는 확인하지 못했다. 확인 순서는 `Headset-Test-Checklist.md` §8·§11·§14.

| 기능 | 켜는 법 | 보여 주는 것 |
|---|---|---|
| **View Offset 부드러운 전환** | `K`, 컨트롤 창 `Render from` | 눈 ↔ 카메라 위치를 0.5초에 걸쳐 옮긴다. 한 번에 바꾸면 가상 장면 전체가 툭 튄다 |
| **마커 라벨** | 고급 창 Diagnostics → `Marker axes and labels`, 또는 `CXMR.DebugMarkers 1` | 마커마다 `ID 12  Stationary` 글자(놓치면 빨강 `LOST`)와 반투명 판. 레벨 안 물체라 **헤드셋에서도** 마커 위에 보인다 |
| **모니터 화면** | 고급 창 Diagnostics의 Monitor | 헤드셋 미러 대신 ① 착용자 시점을 흔들림 줄이고 수평 유지 ② 차 주위를 천천히 도는 화면 |

**View Offset 전환** — `K`와 컨트롤 창 버튼은 전환, 튜닝 창 `View offset` 숫자는 즉시 바뀐다. 전환 시간은 튜닝 창
`View offset glide`(기본 0.5초, 0 = 즉시, 저장됨). 도중에 런타임이 값을 받지 않으면 그 자리에서 멈추고 로그에
`View offset glide stopped at`이 찍힌다 — 헤드셋이 없는 PIE에서는 항상 이렇게 멈춘다.

**마커 라벨** — 모드는 플러그인이 **지금** 그 마커에 걸고 있는 값이다. 캘리브레이션 마커는 처음 보일 때 마커 프로파일
항목의 `Tracking Mode`가 걸리므로, 라벨이 그 값과 다르면 설정이 안 먹은 것이다. 헤드셋이 보고한 적 없는 마커는 `mode ?`로 나온다.
(움직이는 실물에 메시를 겹칠 때는 §3-2의 **CXMR Marker Follower**를 쓴다. 프로필의 DynamicObject 역할만 지정해서는 메시가 따라가지 않는다.)
예전 화면 글자(`DrawDebugString`)는 라벨로 바꿨다.

**모니터 화면** — 두 번째 카메라가 장면을 한 번 더 그리므로(성능 비용) 기본은 꺼짐(`Headset mirror`)이다. 고른 값은 PC별로
저장된다. 해상도는 `BP_CXMRPawn → Spectator → Resolution`(기본 1920×1080).
⚠️ **MR에서는 이 화면에 실제 방이 나오지 않는다.** 패스스루 영상은 헤드셋 런타임 안에서 합성되어 엔진에 오지 않는다.
헤드셋이 없으면(PIE) 띄울 스펙테이터 화면이 없어 텍스처에만 그린다(튜닝 창 `Monitor picture`가 알려 준다).

---

## 8. 새 프로그램용으로 세팅하기

이 템플릿은 **프로그램마다 복제해서** 쓴다. 복제 후 바꿀 것은 전부 `/Game/` 안이다.

1. `/Game/Vehicle/[프로그램]/` 폴더 생성
2. **마커 프로파일** 생성 → 실물 마커 ID와 실측 오프셋 입력 (§3)
3. **차량 프로파일** 생성 → 차량 BP, 마커 프로파일 (§4)
4. 여러 대면 **카탈로그** 생성
5. `ACXMRVehicleRoot`의 **BP 서브클래스**를 만들어 레벨에 배치 → `Loader`에 프로파일·카탈로그 지정
6. 레벨의 배경 오브젝트에 **VROnly** 부착 (§5)
7. 실물을 가릴 자리에 **Mask Mesh** 배치 (§5)

**`/CXMR/` 안은 건드리지 않는다.** 거기를 고치면 모든 프로그램에 영향이 간다.

---

## 9. 증상 → 원인

| 증상 | 먼저 볼 것 |
|---|---|
| 차량 순환이 안 된다 | `Loader → Catalog` 미지정 |
| 차량이 아예 안 나온다 | `Loader → Profile`이 비었거나, 프로파일의 `Vehicle Actor`가 비었다. 로그에 경고가 찍힌다 |
| **Depth Test를 켜면 가상 물체가 심하게 깜빡인다** | **Depth Range가 꺼져 있다 → `Y`로 켜고 방향키로 좁힌다 (§2-1)** |
| **MR을 켜도 패스스루가 안 뜨고 검정 화면이다** | 알파가 0이어야 실세계가 보인다. PPV의 `PP_MR`이 마스크 바깥까지 불투명하게 칠하고 있는지 의심 — PPV blendables에서 `PP_MR`을 빼고 재현되는지 본다 |
| 마커를 인식해도 차가 안 움직인다 | 마커 ID가 프로파일에 없다 / Role이 `Calibration`이 아니다 / 이미 freeze됐다(`R`로 재캘리브) |
| **로그에 `contains id 0` 경고** | 프로파일이 ID 0으로 authoring돼 있다. 0은 Varjo가 무효값으로 거부한다 — 실물에 인쇄된 번호를 넣는다 (§3) |
| 착좌 순환이 반응 없다 | Settings의 Authored eye position `[-][+]`을 쓰거나 `Cycle Manikin Action`에 IA를 지정한다. 차량 프로파일에 ergonomics 애셋이 있어야 한다 (§6) |
| 차가 마커에서 어긋난 곳에 놓인다 | `Local Offset`이 틀렸다 (§3) |
| MR을 켜도 실제 세계가 안 보인다 | 배경 오브젝트에 VROnly가 없다 → `B`로 확인 |
| 패널의 Mixed Reality가 안 켜진다 | CVar를 못 찾은 것. 로그에 `LogCXMR: Warning: Mixed reality toggle ignored` |
| 손이 허공에 얼어붙어 있다 | 그럴 수 없다 — 추적이 끊기면 그리지 않는다. 보인다면 실제로 추적 중이다 |
| `H`를 눌러도 손이 안 나온다 | 로그 `LogCXMRHands: Hand tracker present: NO`면 `OpenXRHandTracking` 플러그인. `present: yes`인데 `hand data received: left no, right no`이고 5초 뒤 `No hand data from the OpenXR runtime` 경고가 뜨면 **앱이 아니라 런타임이 손을 한 번도 보내지 않은 것**이다 → Varjo Base > Settings > System > Experimental > Hand tracking(Varjo / Ultraleap, Ultraleap이면 추적 서비스도)을 확인하고 플레이를 다시 시작한다 |
| 캘리브 후 다른 마커가 안 잡힌다 | `Stop Marker Tracking When Calibrated`가 켜져 있다 → 끈다 |
| **넘버패드를 눌러도 차가 안 움직인다** | ① **NumLock 확인** ② 로그에 `Marker offset adjusted`가 찍히는지 본다. 안 찍히면 입력이 도달하지 않은 것이고, 찍히는데 안 움직이면 배치 계산 쪽이다 |
| 재시작하면 캘리브레이션이 사라진다 | `Saved/CXMR/MarkerCalib_*.json`이 있는지 확인. 없으면 저장이 안 된 것 — `CXMR.SaveCalibration`을 치면 경로가 로그에 찍힌다 (§3-1) |
| Learn이 아무것도 안 한다 | 마커가 하나도 안 잡혔다 — 마커 추적이 켜졌는지, 튜닝 창 `Calibration markers`의 `in view` 수를 본다. 로그에 이유가 찍힌다 |
| 문을 열었더니 차가 튄다 | 문에 마커가 붙어 있다. 움직이는 부품에는 붙이면 안 된다 (§3-1) |
| 모니터 화면에 실제 방이 안 나온다 | 정상 — 관전 카메라에는 패스스루가 없다. Monitor를 `Headset mirror`로 |
| **플레이를 누르자마자 에디터가 꺼진다** — 콜스택 `VarjoOpenXR` → `UVarjoOpenXRFunctionLibrary::SetViewOffset` → `CXMRTuningWindowComponent` | 09-15 수정 전 빌드의 버그다. 튜닝 창이 저장된 View offset을 헤드셋 세션이 뜨기 전에 보냈고, Varjo 런타임이 빈 세션을 읽다 죽었다. 최신을 받아 빌드한다. 빌드 전까지는 `Saved/CXMR/Tuning.json`에서 `"MR.ViewOffset"` 줄을 지운다. 지금은 세션이 뜬 뒤에 적용되고 로그에 `View offset ... will be applied once the headset session is running`이 찍힌다 |

**로그 카테고리**: `LogCXMR`(서브시스템) `LogCXMRHands`(손) `LogCXMRDebug`(마커)
`LogCXMRErgo`(착좌) `LogCXMRPawn`(패널) `LogCXMRPlacement`(배치·캘리브) `LogCXMRMask`(마스킹)
`LogCXMRVehicle`(차량 로더) `LogCXMRInput`(입력) `LogCXMRSpectator`(모니터 화면)

**마커 계측기**: 콘솔에 `CXMR.DebugMarkers 1`, 또는 고급 창 Diagnostics → `Marker axes and labels` — 플러그인이 보고하는 마커 pose를 **가공 없이** 그린다.
캘리브가 이상할 때 "마커를 못 보는 것"인지 "오프셋이 틀린 것"인지 가른다.

---

## 10. 개발 작업

**빌드** (C++ 수정 후):
```
Engine/Build/BatchFiles/Build.bat GMTCK_MREditor Win64 Development -Project=<절대경로>/GMTCK_MR.uproject
```

⚠️ **에디터를 반드시 먼저 닫는다.** 에디터가 `.uasset`을 잠그기 때문에 **빌드·git 브랜치 전환·머지**
중 실패하면 워킹트리가 반쯤 지워진다(실제로 겪음). 복구 전에 사용자 변경과 새 파일부터 확인한다. 광범위한 restore/reset으로 변경을 지우지 않는다.

**애셋 작업도 기존 에디터를 먼저 닫는다.** 자동화용 에디터 한 개만 실행하고 끝나면 종료한다. C++ 빌드와 애셋 작업은 나눈다.

**설정을 Blueprint 클래스 기본값으로 넣을 때 주의**: Python으로 CDO를 고치면 PIE가 블루프린트를
재인스턴싱하며 값을 날린다. 에디터 UI로 넣거나 C++ 생성자에서 기본값을 준다.
