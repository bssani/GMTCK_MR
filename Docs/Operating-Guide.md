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
   착용자 손의 3D 패널은 기본으로 꺼져 있다(§7)

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
| `G` | 시선 점 — 보고 있는 곳에 점(무언가에 닿으면 초록, 10m 안에 없으면 노랑) |
| `I` | Foveated 영역 표시 — 고해상도로 그려지는 시선 영역을 노랗게 칠한다 |
| `R` | 재캘리브레이션 |
| `E` | 차량을 내 앞에 배치 |
| `F` | 컨트롤 패널 클릭 (레이저가 가리키는 곳) |

### 넘버패드 — 차량 위치 미세조정 (§3-1)

**처음 조정을 시작할 때 내가 보고 있던 방향 기준**이다(2026-09-14 시점 기준으로 변경, **09-21 고정**).
차가 어느 쪽을 향해 있든 같은 키는 같은 방향으로 움직이고, **중간에 고개를 돌려도 축이 따라 돌지 않는다.**

| 키 | 차가 움직이는 방향 |
|---|---|
| `NumPad 7` / `NumPad 9` | 내게서 멀어짐 / 가까워짐 (머리 방향을 수평으로 편 기준) |
| `NumPad 1` / `NumPad 3` | 왼쪽 / 오른쪽 |
| `NumPad 0` / `NumPad .` | 위 / 아래 |
| `NumPad 4` / `NumPad 6` | 왼쪽으로 돌기 / 오른쪽으로 돌기 (위에서 볼 때 반시계 / 시계) |
| **저장** `NumPad Enter` | 첫 입력은 위치 확인·새 마커 캡처, 모두 관측한 뒤 다시 누르면 저장 |
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

**회전 중심**은 `VehicleRoot → Placement → Nudge Pivot`이다. 기본 `Markers`는 보이는 캘리브레이션 마커들의 중심이라,
마커 옆에서 먼저 맞춰 둔 부분이 돌려도 제자리에 남는다. `Viewer`(내 머리), `VehicleOrigin`(차 원점)도 고를 수 있다.

컨트롤 패널 Calibration 탭과 튜닝 창은 **차의 월드 위치**를 보여 준다(예전 패널의 X/Y/Z/Yaw는 차 기준 내부값이라 키 방향과 부호가 맞지 않았다).

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

⚠️ **아무 일도 안 하는 키**: `G`(gaze) `C`(dynamic tracking) `I`(foveation 시각화).
Varjo 예제에서 애셋만 넘어오고 C++ 배선이 없다. 필요해지면 `UCXMRVarjoInputComponent`에
액션을 추가하고 `IMC_Varjo`에 매핑한다.

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
| 왼쪽 스틱 좌/우 | 턴테이블 회전 (누르고 있는 동안) |
| 왼쪽 `A` / `B` | 좌/우 연속 회전 토글 |
| 오른쪽 스틱 좌/우 | 트림 순환 |
| 오른쪽 스틱 상/하 | 차량 순환 |
| 오른쪽 트리거 | 컨트롤 패널 클릭 |
| (미배정) | 착좌 위치 순환 — `CycleManikinAction`에 IA를 지정하면 동작 (§6) |

손 패널을 켰을 때(`Show Hand Panel`, §7) 패널은 **왼손에 붙고** **오른손 레이저**로 누른다. 기본은 꺼져 있다.

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
3. 넘버패드나 Placement의 `− / +`로 실물 USB·센터콘솔과 맞춘다.
4. `Confirm Alignment` 또는 넘버패드 `Enter`로 현재 위치를 확인한다. 이전 관측값은 캡처에 사용하지 않는다.
5. 차량과 마커를 고정한 채 등록된 마커를 차례로 보이게 한다. 동시에 모두 보일 필요는 없다.
6. `Save Alignment` 또는 `Enter`를 다시 눌러 저장한다. 부족한 마커 수와 저장 불가 사유는 화면·패널·로그에 표시한다.

나중에 미세조정하면 이전 확인·캡처가 취소된다. `Enter`로 다시 확인하고 새 관측을 모은 뒤 저장한다.
다음 실행은 저장한 마커 관계로 복원한다. 하나만 보이면 `Use Restored Alignment`로 실물 정렬을 확인한다.
차량을 움직이지 않고 기존 정렬을 재저장하면 복원용 마커 집합과 기준 ID를 유지한다.
학습으로 마커 구성이 바뀌면 이전 완료 정보는 해제된다. 새 구성은 확인·캡처 후 저장한다.

기존 `CXMR.LearnMarkers`는 보이는 번호를 추가하는 별도 명령이다. 새 확인·캡처·저장 흐름에서는 번호를 미리 등록해야 한다.

### 박스로 맞추기 — 눈대중 대신 모서리 (2026-09-16 추가)

4번 "실물에 겹칠 때까지 맞춘다"가 어려운 이유는 **맞춰 볼 모서리가 없어서**다. 콘솔 자리에 놓은 평범한 박스는
콘솔 CAD와 닮은 데가 없다. 그래서 **같은 크기의 박스를 가상에도 넣고, 박스를 박스에 맞춘다.** 가상 박스는
차에 붙어 있으므로 박스가 맞는 순간 콘솔도 제자리다. 실제 박스는 움직이지 않는다 — 차가 박스로 온다.

**준비 (에디터, 1회)**
1. 실제 박스를 줄자로 잰다 (앞뒤 X, 좌우 Y, 높이 Z, cm). 단단하고 모서리가 각진 박스를 쓴다
2. 프로젝트 쪽 차량 액터 BP(`/Game/...`, 예: `BP_New`)에 **CXMR Alignment Box** 컴포넌트를 추가한다
   (클래스는 플러그인 `/CXMR/` C++)
3. `Box Size`에 잰 값을 넣는다. **컴포넌트 원점 = 박스 바닥면 중앙, X = 박스 앞쪽**. 뷰포트에서 CAD 콘솔 기준으로
   실제 박스가 서 있어야 할 자리에 놓는다
4. `Touch Corners`: 짚을 모서리 순서. 기본은 윗면 앞왼쪽 → 앞오른쪽 → 뒤오른쪽(3개째는 검증용)

**방법 A — 눈으로**: 헤드셋에 하늘색 박스 윤곽이 보인다. 넘버패드나 튜닝 창으로 윤곽을 실제 박스에 겹친 뒤 Learn.
모서리끼리 대 보면 되므로 콘솔 전체를 눈대중하는 것보다 쉽다. 두세 방향에서 확인한다.

**방법 B — 손가락으로 짚기**
1. 튜닝 창 **Alignment box → Touch mode**를 켠다. 모서리에 번호가 뜬다(노랑 = 다음에 짚을 곳)
2. 실제 박스의 1번 모서리를 **오른손 검지 끝으로 위에서** 짚는다
3. 짚은 채로 **왼손으로 핀치**(엄지·검지 붙이기) → 0.5초 동안 손끝을 가만히 둔다. 번호가 초록이 된다
   (핀치 대신 튜닝 창 **Record corner now**, 콘솔 `CXMR.BoxTouch`도 된다)
4. 2번, 3번 모서리도 같게 한다. 마지막 모서리가 들어가면 차가 스스로 옮겨지고 Touch mode가 꺼진다
5. `Status`의 **cm apart**가 짚은 점과 박스 모서리의 차이다. 1cm 안쪽이면 좋다. 2cm를 넘으면 경고가 뜬다 →
   `Box Size`, 짚은 모서리 순서, 손이 움직였는지를 본다
6. **Learn marker layout from this pose (and save)** (같은 분류에 버튼이 있다)

- 짚는 손을 바꾸려면 `Touching hand`(왼손으로 짚으면 오른손 핀치)
- `Fingertip to nearest corner`는 지금 손끝이 가장 가까운 가상 모서리에서 몇 cm인지 보여 준다 — 맞춘 뒤 검증에 쓴다
- 손 추적이 실제 손에서 어긋나 있으면 결과도 그만큼 어긋난다. 먼저 §7 Hands의 손 보정(마커에 손끝 대고 Snap)을 한다
- 위에서 짚는다고 가정하고 손끝 관절 위치를 손가락 반지름만큼 내린다(`Touch From Above`). 옆에서 짚으면 끈다
- **박스가 작으면 방향 오차가 멀리서 커진다.** 40×30cm 박스에 짚기 오차 ±0.5cm를 넣은 시험에서 박스 모서리는 0.6cm 안에
  맞았지만 1m 떨어진 점은 최대 3cm 어긋났다. 짚는 모서리는 **서로 가장 먼 것**으로 고르고, 시트처럼 먼 부분이 중요하면
  방법 A로 한 번 더 본다

### 왜 마커가 필요한가 — 첫 정렬은 어차피 수동인데

**마커는 첫 정렬을 자동화하는 물건이 아니라, 한 번 맞춘 것을 세션을 넘어 재현하는 물건이다.**

앱이나 헤드셋을 다시 켜면 **OpenXR이 월드 원점을 새로 잡는다.** 어제 저장한 "차량 월드 좌표"는
오늘 방의 엉뚱한 지점이다. 반면 마커는 실공간에 물리적으로 고정돼 있으므로, 마커를 보면
"지금 좌표계에서 마커가 어디인지"를 알 수 있고 저장해 둔 *마커↔차량* 관계로 차를 제자리에 놓는다.

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

내용은 **앵커 기준 마커 포즈**, 검증한 마커 ID, 기준 ID와 차량 식별 정보다.
차량 월드 위치는 다음 실행에서 마커로 다시 계산한다.

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

`NumPad Enter`는 첫 입력에서 확인·캡처를 시작한다. 모든 등록 마커를 새로 관측한 뒤 다시 누르면 파일에 저장한다.

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

- 학습 시점에 **모든 마커가 동시에 보여야** 서로의 상대 위치가 잡힌다. 안 보인 마커는 로그에
  `Marker N is in the layout but was not in view while learning`으로 이름이 찍힌다
- 마커가 물리적으로 밀리면 저장값이 거짓이 된다 → 다시 Learn
- id 0은 Varjo가 무효 값으로 쓰므로 학습하지 않는다
- 마커가 기울어 붙어 있어도(수직면 등) `Keep Level`이 켜져 있으면 차는 수평으로 놓인다

### 마커 몇 개를 쓸 것인가

`VehicleRoot` 액터 → `Placement` 컴포넌트에서 설정한다.

| 설정 | 의미 |
|---|---|
| **Min Markers To Calibrate** | 이 개수가 모여야 캘리브 완료로 친다. **1이면 단일 마커, 2 이상이면 멀티마커** |
| **Mode** | `MarkerAnchor`(마커로 정렬 — 실내) / `PawnRelative`(내 앞에 배치 — 실외 턴테이블) |
| **Freeze After Calibration** | 완료 후 이 컴포넌트가 차량 재배치를 멈춤 |
| **Stop Marker Tracking When Calibrated** | ⚠️ **기본 off로 둘 것.** 켜면 헤드셋의 마커 추적이 **전역으로** 꺼져서 DynamicObject 마커까지 죽는다 |
| **Start Marker Tracking On Begin Play** | 기본 on. 시작할 때 마커 추적을 켠다(세션이 올라올 때까지 20초 재시도) |
| **Calibration Settle Seconds** | 기본 1.5. 마커가 충분히 보인 뒤 이 시간 동안 갱신을 받고 고정한다 — 첫 샘플의 노이즈를 굳히지 않게 |

**단일 마커는 각도 노이즈가 증폭된다**(레버암). 마커 하나의 방향이 1° 틀어지면 3m 떨어진 차 뒤쪽은
5cm 밀린다. **마커 2개 이상을 쓰면** 시스템이 마커 *위치*들 사이의 기준선으로 yaw를 계산하고
roll/pitch는 0으로 고정한다(차는 바닥에 평평히 놓인다는 가정) — 훨씬 안정적이다.

### 지금 프로젝트 상태

`DA_MarkerProfile_TestCar`에는 **ID 0 하나(무효 값)**만 들어 있고 `Min Markers = 1`이다.
→ 실물 마커로는 **Learn을 한 번 해야** 레이아웃이 생긴다. 이후 `Saved/CXMR/MarkerCalib_DA_MarkerProfile_TestCar.json`에 남는다.

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
| **Trims** | 트림 목록 (아래) |
| **Ergonomics** | 퍼센타일 착좌 데이터 |

### 트림과 CMF는 **컴포넌트 태그**로 지정한다

이름이나 인덱스가 아니라 태그다. 차량 BP를 다시 임포트하거나 파츠를 쪼개도 **태그만 살아 있으면**
프로파일은 그대로 유효하다.

**트림(파츠 보임/숨김)**:
- 트림의 `Visible Parts`에 보여줄 태그를 나열한다
- **어떤 트림에도 언급되지 않은 태그와 태그 없는 컴포넌트는 항상 보인다** → 공유 바디는 표시 불필요.
  예외만 태그하면 된다

**CMF(재질 교체)**:
- `CMF Options[].Materials[]`에 `{Part Tag, Material Slot, Material}`
- **Part Tag를 비우면 차 전체**에 적용된다 (외장 페인트의 일반적 경우)

**Default CMF**: 트림을 고르면 자동으로 선택될 CMF 인덱스. 범위를 벗어나면 0으로 접힌다.

### 차량 목록 (`UCXMRVehicleCatalog`)

`Vehicles` 배열에 프로파일들을 넣으면 차량 순환이 동작한다.
**카탈로그를 지정하지 않으면 차량 순환만 조용히 무시된다**(트림 순환은 정상 동작).

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
(트림/차량 순환과 같은 히스테리시스).

컨트롤 패널 **Viewer 탭 → Human factors → Manikin `[-][+]`**으로도 순환한다(2026-09-15부터. 예전 WBP 패널에는 이 행이 없었다).

⚠️ **차량 프로파일에 ergonomics 애셋이 연결돼 있어야 동작한다.** 안 걸려 있으면
`ResolveProfile()`이 null을 반환해 눌러도 조용히 아무 일도 일어나지 않는다.

동작이 모드에 따라 **정반대**다:
- **VR**: 세계가 가상이므로 **내 시점을 옮긴다**
- **MR**: 실제 몸은 못 옮기므로 **차를 옮긴다**
- **MR + MarkerAnchor**: **아무것도 안 옮긴다.** eye point가 이미 물리적으로 실재하므로
  (실물 시트) 차를 옮기면 캘리브레이션과 싸운다. 로그에 이유가 찍힌다

⚠️ Hip Point는 저장만 하고 아직 이동에 쓰지 않는다.

---

## 7. 컨트롤 패널 조정

`BP_CXMRPawn` → `Control Panel` 관련 프로퍼티. **C++ 리빌드 불필요.**

**손 패널은 기본으로 꺼져 있다.** 착용자는 차를 판단하러 온 사람이고 세션은 데스크톱 창에서 돌리므로,
레벨 안에 3D UI가 떠 있으면 헤드셋에서도 모니터 화면에서도 방해만 된다. 착용자가 혼자 조작해야 할 때만
`Show Hand Panel`을 켠다. 켜면 아래 값으로 왼손에 붙는다.

| 프로퍼티 | 기본값 | 의미 |
|---|---|---|
| `Show Hand Panel` | **false** | 착용자 왼손에 3D 패널을 띄울지. 끄면 오른손 레이저도 같이 꺼진다 |
| `Panel Offset` | (8, 0, 4) | 왼손 컨트롤러 기준 위치(cm) |
| `Panel Rotation` | pitch −25, yaw 180 | 기울기. yaw 180이 착용자 쪽을 향하게 한다 |
| `Panel Scale` | 0.03 | cm/픽셀. 432×520px → 약 13×16cm |
| `Panel Draw Size` | 432×520 | 손 패널 해상도(픽셀). 내용이 더 길면 스크롤된다 |
| `Control Panel Class` | CXMR Control Panel (C++) | 다른 패널로 교체 가능 |

**패널이 안 보이면** 너무 가까워 근접 클리핑(10cm)에 잘린 것이다 — `Panel Offset`의 X를 늘린다.

### CXMR Control 통합 창

**데스크톱 창은 하나다.** 상단의 **Use / Setup**으로 모드를 바꾸고 왼쪽 메뉴에서 작업을 고른다.
Varjo Lab Tools의 Use/Edit 흐름을 참고했다([공식 소개](https://developer.varjo.com/lab-tools)).

| 메뉴 | 용도 | 모드 |
|---|---|---|
| Vehicle | 차량 선택 | Use / Setup |
| View | 눈 기준, 실물 화면, 모니터 화면, 착좌 선택 | Use / Setup |
| Placement | 보정 안내, 마커 상태·정렬 오차, 위치·회전 미세조정 | Use / Setup |
| Display / Depth | 실물 화면·배경, 깊이 범위, 시점 전환, 가상 화면 밝기, 모니터 세부 설정 | Setup |
| USB / Hands | 생성 손 접촉 상태·접촉 여유·해제 여유 | Setup |
| Diagnostics | 마커·손·시선·잡기 상태와 진단 표시 | Setup |

**보정 순서**:
1. 차량 Data Asset의 `Alignment`에서 `Has Driver Eye Reference`를 켠다.
   `Driver Eye Reference`는 **로드되는 원본 차량 액터의 로컬 좌표**다. 눈 기준 위치와 운전자 정면 방향(+X)을 넣는다.
   CAD 원점이 차체에서 멀어도 메시를 재배치하지 않는다. `VehicleRootOffset`은 별도로 한 번만 적용된다.
   기존 HF용 Ergonomics `EyePoint`와 초기 정렬용 `Driver Eye Reference`는 목적이 다르다.
2. 연결한 Marker Profile에 사용할 마커 ID를 모두 등록하고 Role을 `Calibration`으로 지정한다.
   동적 소품 마커는 정렬 저장에 포함하지 않는다.
3. 실제 운전자석에 자연스럽게 앉아 정면을 보고 `Placement → 1 Initial Alignment`를 누른다.
   3초 뒤 추적된 HMD 위치와 yaw로 차량을 한 번 배치한다. 다시 누르면 취소한다.
   눈 기준점·차량·마커 설정이 없거나 HMD 위치 추적이 유효하지 않으면 초기 배치를 적용하지 않는다.
4. 실제 USB·센터콘솔과 비교해 위치와 회전을 미세조정한다.
   Turn은 차량에 지정한 눈 기준점을 중심으로 돈다. Root 위치까지 보정하므로 먼 CAD 원점 주위로 크게 돌지 않는다.
   마커가 새로 보이거나 고개를 돌려도 조정 중인 차량 위치는 유지한다.
5. `2 Confirm Alignment`를 누르고 각 마커를 한 번씩 보이게 한다. 화면의 `captured` 수를 확인한다.
   넘버패드 `Enter`의 첫 입력도 같은 확인·캡처를 시작한다.
   확인 뒤 새로 관측한 좌표만 모은다. 두 마커를 동시에 볼 필요는 없다.
   모든 등록 마커를 관측해야 `3 Save Alignment`가 열린다. 차량과 실물 마커 위치는 고정한 채 관측한다.
   확인 뒤 미세조정·차량 교체·눈 기준·모델 설정 변경을 하면 다시 확인하고 마커를 모아야 한다.
6. `3 Save Alignment` 또는 넘버패드 `Enter`를 다시 누른다. 실제 파일 교체에 성공해야 완료로 표시한다.
   실패하면 기존 프로필과 파일을 유지하며 같은 관측값으로 재시도할 수 있다.

**다음 실행**: 저장한 차량·마커 관계로 복원한다. HMD 초기 맞추기를 매번 반복하지 않는다.
마커 하나로 복원하면 차량을 고정한 상태에서 실물 기준을 확인하고 `Use Restored Alignment`를 누른다.
두 개가 유효하게 관측되면 위치·방향 일치와 다중 계산 오차를 확인한다.
저장된 기준 마커는 등록 ID 중 가장 작은 ID다. 그 마커가 없으면 다른 저장 마커로 복원 후보를 만든다.
마커가 안 보이면 복원을 기다린다. 이미 확정한 정렬은 가림·재관측 때문에 움직이지 않는다.
마커의 실제 배치, 모델 오프셋 또는 실물 좌석·목업이 달라졌을 때 새로 정렬한다.
Setup의 `Advanced Placement → Restore from Markers Again`은 저장값으로 복원을 다시 시도한다.
기존 형식의 보정 파일은 읽을 수 있지만, 단일 마커 확인 복원을 쓰려면 새 흐름으로 한 번 저장한다.

**조작**: 수치는 슬라이더 또는 직접 입력, 선택 항목은 목록, 위치 조정은 `− / +` 버튼을 쓴다.
항목에 마우스를 올리면 설명이 나온다. `Default`는 해당 설정과 저장값을 초기화한다.
기존 손목 패널과 키보드 조작도 같은 기능의 상태를 읽고 쓴다.

**다시 열기**: Play 중 에디터의 `CXMR Control` 버튼으로 창을 열거나 앞으로 가져온다.
`CXMR Settings`는 같은 창의 설정 모드·화면 메뉴를 연다. 콘솔 `CXMR.Tuning`도 같은 창을 열고 닫는다.
X로 닫아도 재열기 가능하며 PIE 종료 시 닫힌다. 키보드 단축키는 창 포커스를 따르므로
게임 단축키를 쓰려면 게임 화면을 클릭한다.

**저장**: 영속화 대상 일반 설정은 조작을 마칠 때 `Saved/CXMR/Tuning.json`에 자동 저장한다.
MR·배경·깊이 비교 등 세션용 스위치는 매 실행마다 다시 설정한다.
차량 보정은 별도 파일 `Saved/CXMR/MarkerCalib_*.json`에 저장한다.
파일은 마커 프로필 ID와 차량 Data Asset 경로로 구분한다. 같은 마커 프로필을 쓰는 차량도 각각 저장한다.
차량 모델 클래스나 오프셋이 바뀌면 이전 정렬을 적용하지 않는다. 모델 설정 변경 뒤 차량을 다시 로드하고 정렬한다.
USB의 기본 입력은 생성되는 손의 접촉이다. 양손의 관절 구와 손가락 뼈 범위를 같은 추적 좌표로 계산한다.
`USB / Hands → Hand Contact Margin`은 손 표면 밖의 접촉 여유(cm), `Hand Release Margin`은 해제 여유다.
기본값은 0.25cm와 0.75cm다. 손이 포트 중심에 닿으면 테두리 빛과 지정한 `Near Sound`가 나오고, 떨어지면 해제된다.
`Show USB Contact Bounds`로 포트 중심과 범위를 확인한다. 손 표시 크기와 손 좌표 보정은 접촉에도 적용된다.
H키로 손 표시를 꺼도 접촉 판정은 계속된다. 추적 유예는 이미 켜진 반응만 잠깐 유지하며 새 접촉을 만들지 않는다.
차량의 USB 메시·재질은 유지하며, 접촉을 케이블 삽입 완료로 표시하지 않는다.
기존 플러그 정렬이 필요한 Blueprint는 USB 액터의 `Use Hand Contact`를 끈다. 이때만 Reach·각도 설정을 사용한다.
실물 케이블·금속 플러그의 깊이 인식은 헤드셋에서 별도로 확인해야 한다.

**창 설정**: `BP_CXMRPawn → Desktop Panel → Open On Begin Play`가 자동 열기를 담당한다.
같은 pawn의 `Tuning Window`가 창을 소유하며 기본 크기는 900×760, 위치는 (40, 60)이다.
`Tuning Window → Open On Begin Play` 기본값은 false다. 기존 BP가 true로 덮어써도 같은 창을 재사용한다.
통합 창의 크기·제목은 `Tuning Window`에서 바꾼다.

아래 기존 절차에서 말하는 “튜닝 창”은 이 통합 창의 해당 메뉴다.
`Vehicle placement / Alignment box / Input`은 배치·보정의 상세 설정,
`Mixed reality / Depth / Display / Monitor`는 화면·깊이,
USB 접촉 설정은 USB·손에 있다. 기존 `Plug tip / USB port` 추정·정렬 설정과 `Hands / Eyes / Grab`은 진단에 있다.

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

### 손목 패널

손목 패널은 기존 `UCXMRControlPanelWidget`의 차량/시점 두 탭을 유지한다.
데스크톱 통합 창은 이 위젯의 차량·시점 바인딩을 재사용하고 보정·설정 메뉴를 추가한다.
행 UI는 `CXMRPanelUI`를 공유하며 내용이 길면 스크롤된다.
위젯 블루프린트 이름으로 버튼을 연결하는 방식은 사용하지 않는다.

### Varjo 예제에서 가져온 기능 (2026-09-15)

Varjo 예제 맵(`/Game/VRTemplate/Maps/VRTemplateMap`)에 있고 CXMR에 없던 것들이다. 코드는 전부 CXMR 플러그인에 있고,
애셋은 `/CXMR/`에만 있다(`/Game/` 참조 없음). **헤드셋 실측 전이다** — PIE에는 헤드셋이 없어 시선·foveation·손 추적·
모니터 스펙테이터 화면 자체는 확인하지 못했다. 확인 순서는 `Headset-Test-Checklist.md` §9~§11.

| 기능 | 켜는 법 | 보여 주는 것 |
|---|---|---|
| **View Offset 부드러운 전환** | `K`, 컨트롤 창 `Render from` | 눈 ↔ 카메라 위치를 0.5초에 걸쳐 옮긴다. 한 번에 바꾸면 가상 장면 전체가 툭 튄다 |
| **마커 라벨** | 컨트롤 창 Display → `Marker axes and labels`, 또는 `CXMR.DebugMarkers 1` | 마커마다 `ID 12  Stationary` 글자(놓치면 빨강 `LOST`)와 반투명 판. 레벨 안 물체라 **헤드셋에서도** 마커 위에 보인다 |
| **시선 점** | `G`, 컨트롤 창 Eyes | 보고 있는 곳에 점. 무언가에 닿으면 초록, 10m 안에 없으면 노랑 |
| **Foveated 영역** | `I`, 컨트롤 창 Eyes | 고해상도 focus view가 덮는 영역을 노랗게 칠한다 |
| **손으로 잡기** | `CXMR Grabbable`을 붙인 액터를 엄지·검지로 집는다 | 집는 동안 손을 따라오고 손가락을 벌리면 놓는다. 물리 물체는 잡는 동안 물리가 멈추고, 놓을 때 손의 속도를 받는다 |
| **모니터 화면** | 컨트롤 창 Monitor, 튜닝 창 Monitor | 헤드셋 미러 대신 ① 착용자 시점을 흔들림 줄이고 수평 유지 ② 차 주위를 천천히 도는 화면 |

**View Offset 전환** — `K`와 컨트롤 창 버튼은 전환, 튜닝 창 `View offset` 숫자는 즉시 바뀐다. 전환 시간은 튜닝 창
`View offset glide`(기본 0.5초, 0 = 즉시, 저장됨). 도중에 런타임이 값을 받지 않으면 그 자리에서 멈추고 로그에
`View offset glide stopped at`이 찍힌다 — 헤드셋이 없는 PIE에서는 항상 이렇게 멈춘다.

**마커 라벨** — 모드는 플러그인이 **지금** 그 마커에 걸고 있는 값이다. 캘리브레이션 마커는 처음 보일 때 마커 프로파일
항목의 `Tracking Mode`가 걸리므로, 라벨이 그 값과 다르면 설정이 안 먹은 것이다. 헤드셋이 보고한 적 없는 마커는 `mode ?`로 나온다.
(움직이는 실물을 마커로 따라가게 하는 DynamicObject 기능은 아직 없다 — 그 역할의 마커에는 아무도 모드를 걸지 않는다.)
예전 화면 글자(`DrawDebugString`)는 라벨로 바꿨다.

**시선 점** — OpenXR로는 두 눈을 합친 시선 하나만 온다(눈별 방향·고정점·신뢰도 없음). Visibility 채널로 10m까지 쏜다.
튜닝 창 `Gaze lands`가 `no eye tracker connected`면 Varjo Base의 시선 추적과 시선 캘리브레이션을 본다.

**Foveated 영역** — 칠해지려면 헤드셋 지원 + Project Settings → Varjo OpenXR의 Rendering Mode `Quad View` +
`Foveated Rendering` on + 시선 추적이 모두 필요하다. 튜닝 창 `Foveated rendering`이 `running`이 아니면 켜도 아무것도
칠해지지 않고, 로그에 이유가 찍힌다. 머티리얼은 예제 것을 복사한 `/CXMR/Core/Materials/PP_CXMRFoveationVisualization`.

**손으로 잡기** — 잡을 액터에 `CXMR Grabbable` 컴포넌트를 붙이고 루트를 **Movable**로 둔다.
- 테스트 물체: 튜닝 창 Grab → `Spawn a test cube in front of me`, 또는 콘솔 `CXMR.SpawnGrabCube`(`1`을 붙이면 물리 큐브)
- 엄지 끝·검지 끝 간격이 `Pinch closes below`(2cm) 아래로 가면 집고, `Pinch opens above`(3.5cm) 위로 벌어져야 놓는다.
  두 값 사이에서는 상태를 유지하므로 경계에서 떨지 않는다
- 집는 순간 물체 경계가 핀치 지점에서 `Reach from the pinch`(5cm) 안이어야 잡힌다. 손가락을 먼저 붙이고 물체를 쓸고 지나가면 잡히지 않는다
- 손 보정(Hands offset, §8 체크리스트)이 그대로 적용된다. 추적이 0.25초 안에 돌아오면 놓지 않는다
- 컨트롤러 입력은 쓰지 않는다. BP에서는 `On Grabbed` / `On Released` 이벤트, `Get Held Actor`, 추적 없이 시험하는 `Set Simulated Pinch`

**모니터 화면** — 두 번째 카메라가 장면을 한 번 더 그리므로(성능 비용) 기본은 꺼짐(`Headset mirror`)이다. 고른 값은 PC별로
저장된다. 해상도는 `BP_CXMRPawn → Spectator → Resolution`(기본 1920×1080).
⚠️ **MR에서는 이 화면에 실제 방이 나오지 않는다.** 패스스루 영상은 헤드셋 런타임 안에서 합성되어 엔진에 오지 않는다.
헤드셋이 없으면(PIE) 띄울 스펙테이터 화면이 없어 텍스처에만 그린다(튜닝 창 `Monitor picture`가 알려 준다).

---

## 8. 새 프로그램용으로 세팅하기

이 템플릿은 **프로그램마다 복제해서** 쓴다. 복제 후 바꿀 것은 전부 `/Game/` 안이다.

1. `/Game/Vehicle/[프로그램]/` 폴더 생성
2. **마커 프로파일** 생성 → 실물 마커 ID와 실측 오프셋 입력 (§3)
3. **차량 프로파일** 생성 → 차량 BP, 마커 프로파일, 트림/CMF (§4)
4. 여러 대면 **카탈로그** 생성
5. `ACXMRVehicleRoot`의 **BP 서브클래스**를 만들어 레벨에 배치 → `Loader`에 프로파일·카탈로그 지정
6. 레벨의 배경 오브젝트에 **VROnly** 부착 (§5)
7. 실물을 가릴 자리에 **Mask Mesh** 배치 (§5)

**`/CXMR/` 안은 건드리지 않는다.** 거기를 고치면 모든 프로그램에 영향이 간다.

---

## 9. 증상 → 원인

| 증상 | 먼저 볼 것 |
|---|---|
| 차량이 아예 안 나온다 | `Loader → Profile`이 비었거나, 프로파일의 `Vehicle Actor`가 비었다. 로그에 경고가 찍힌다 |
| 차량 순환이 안 된다 | `Loader → Catalog` 미지정 (트림 순환은 되는 게 정상) |
| 트림을 바꿔도 아무 변화가 없다 | 파츠에 **컴포넌트 태그**가 없다. 태그 없는 것은 항상 보인다 |
| **Depth Test를 켜면 가상 물체가 심하게 깜빡인다** | **Depth Range가 꺼져 있다 → `Y`로 켜고 방향키로 좁힌다 (§2-1)** |
| **MR을 켜도 패스스루가 안 뜨고 검정 화면이다** | 알파가 0이어야 실세계가 보인다. PPV의 `PP_MR`이 마스크 바깥까지 불투명하게 칠하고 있는지 의심 — PPV blendables에서 `PP_MR`을 빼고 재현되는지 본다 |
| 마커를 인식해도 차가 안 움직인다 | 마커 ID가 프로파일에 없다 / Role이 `Calibration`이 아니다 / 이미 freeze됐다(`R`로 재캘리브) |
| **로그에 `contains id 0` 경고** | 프로파일이 ID 0으로 authoring돼 있다. 0은 Varjo가 무효값으로 거부한다 — 실물에 인쇄된 번호를 넣는다 (§3) |
| 착좌 순환이 반응 없다 | 패널 Viewer 탭의 Manikin `[-][+]`을 쓰거나 `Cycle Manikin Action`에 IA를 지정한다. 차량 프로파일에 ergonomics 애셋이 있어야 한다 (§6) |
| 차가 마커에서 어긋난 곳에 놓인다 | `Local Offset`이 틀렸다 (§3) |
| MR을 켜도 실제 세계가 안 보인다 | 배경 오브젝트에 VROnly가 없다 → `B`로 확인 |
| 패널의 Mixed Reality가 안 켜진다 | CVar를 못 찾은 것. 로그에 `LogCXMR: Warning: Mixed reality toggle ignored` |
| 턴테이블이 안 돈다 | `Placement → Mode`가 `MarkerAnchor`다. 실내에서는 회전을 막는 게 설계다 |
| 손이 허공에 얼어붙어 있다 | 그럴 수 없다 — 추적이 끊기면 그리지 않는다. 보인다면 실제로 추적 중이다 |
| `H`를 눌러도 손이 안 나온다 | 로그 `LogCXMRHands: Hand tracker present: NO`면 `OpenXRHandTracking` 플러그인. `present: yes`인데 `hand data received: left no, right no`이고 5초 뒤 `No hand data from the OpenXR runtime` 경고가 뜨면 **앱이 아니라 런타임이 손을 한 번도 보내지 않은 것**이다 → Varjo Base > Settings > System > Experimental > Hand tracking(Varjo / Ultraleap, Ultraleap이면 추적 서비스도)을 확인하고 플레이를 다시 시작한다 |
| 캘리브 후 다른 마커가 안 잡힌다 | `Stop Marker Tracking When Calibrated`가 켜져 있다 → 끈다 |
| **넘버패드를 눌러도 차가 안 움직인다** | ① **NumLock 확인** ② 로그에 `Marker offset adjusted`가 찍히는지 본다. 안 찍히면 입력이 도달하지 않은 것이고, 찍히는데 안 움직이면 배치 계산 쪽이다 |
| 재시작하면 캘리브레이션이 사라진다 | `Saved/CXMR/MarkerCalib_*.json`이 있는지 확인. 없으면 저장이 안 된 것 — `CXMR.SaveCalibration`을 치면 경로가 로그에 찍힌다 (§3-1) |
| Learn이 아무것도 안 한다 | 마커가 하나도 안 잡혔다 — 마커 추적이 켜졌는지, 튜닝 창 `Calibration markers`의 `in view` 수를 본다. 로그에 이유가 찍힌다 |
| 문을 열었더니 차가 튄다 | 문에 마커가 붙어 있다. 움직이는 부품에는 붙이면 안 된다 (§3-1) |
| `G`를 눌러도 점이 안 보인다 | 튜닝 창 Eyes → `Gaze lands`. `no eye tracker connected`면 Varjo Base의 시선 추적·캘리브레이션 |
| `I`를 눌러도 아무것도 안 칠해진다 | 튜닝 창 Eyes → `Foveated rendering`이 `running`인지. 아니면 Quad View + Foveated Rendering 설정 (§7) |
| 핀치해도 안 잡힌다 | 대상에 `CXMR Grabbable`이 있는지 / 루트가 Movable인지(로그 `not Movable`) / 튜닝 창 Grab의 `gap`이 2cm 아래로 내려가는지 |
| 모니터 화면에 실제 방이 안 나온다 | 정상 — 관전 카메라에는 패스스루가 없다. Monitor를 `Headset mirror`로 |
| **플레이를 누르자마자 에디터가 꺼진다** — 콜스택 `VarjoOpenXR` → `UVarjoOpenXRFunctionLibrary::SetViewOffset` → `CXMRTuningWindowComponent` | 09-15 수정 전 빌드의 버그다. 튜닝 창이 저장된 View offset을 헤드셋 세션이 뜨기 전에 보냈고, Varjo 런타임이 빈 세션을 읽다 죽었다. 최신을 받아 빌드한다. 빌드 전까지는 `Saved/CXMR/Tuning.json`에서 `"MR.ViewOffset"` 줄을 지운다. 지금은 세션이 뜬 뒤에 적용되고 로그에 `View offset ... will be applied once the headset session is running`이 찍힌다 |
| 손 패널이 한눈에 안 들어온다 | 잘리지 않고 스크롤된다. 한 번에 다 보이게 하려면 `Panel Draw Size` Y를 키우고 `Panel Scale`을 줄인다 (§7). 재빌드 불필요 |

**로그 카테고리**: `LogCXMR`(서브시스템) `LogCXMRHands`(손) `LogCXMRDebug`(마커)
`LogCXMRErgo`(착좌) `LogCXMRPawn`(패널) `LogCXMRPlacement`(배치·캘리브) `LogCXMRMask`(마스킹)
`LogCXMRVehicle`(차량 로더) `LogCXMRInput`(입력) `LogCXMRGaze`(시선) `LogCXMRFoveation`(foveation)
`LogCXMRGrab`(손으로 잡기) `LogCXMRSpectator`(모니터 화면)

**마커 계측기**: 콘솔에 `CXMR.DebugMarkers 1`, 또는 컨트롤 창 Display → `Marker axes and labels` — 플러그인이 보고하는 마커 pose를 **가공 없이** 그린다.
캘리브가 이상할 때 "마커를 못 보는 것"인지 "오프셋이 틀린 것"인지 가른다.

---

## 10. 개발 작업

**빌드** (C++ 수정 후):
```
Engine/Build/BatchFiles/Build.bat GMTCK_MREditor Win64 Development -Project=<절대경로>/GMTCK_MR.uproject
```

⚠️ **에디터를 반드시 먼저 닫는다.** 에디터가 `.uasset`을 잠그기 때문에 **빌드·git 브랜치 전환·머지**
중 실패하면 워킹트리가 반쯤 지워진다(실제로 겪음). 커밋은 무사하므로 `git reset --hard`로 복구된다.

**애셋 편집은 반대다** — 에디터가 켜져 있어야 한다. C++과 애셋 작업은 한 번에 하지 말고 나눈다.

**설정을 Blueprint 클래스 기본값으로 넣을 때 주의**: Python으로 CDO를 고치면 PIE가 블루프린트를
재인스턴싱하며 값을 날린다. 에디터 UI로 넣거나 C++ 생성자에서 기본값을 준다.
