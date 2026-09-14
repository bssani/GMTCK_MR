# CXMR 사용법

이 프로젝트를 **어떻게 작동시키는가**. 마커 번호를 바꾸고, 차량을 갈아끼우고, 세션을 돌리는 실무 절차.

| 문서 | 용도 |
|---|---|
| **이 문서** | 사용법 — 뭘 어디서 바꾸고 어떻게 돌리는가 |
| `Headset-Test-Checklist.md` | 헤드셋 실측 순서와 "정상인데 안 되는 것처럼 보이는" 경우 |
| `Varjo-Capabilities.md` | Varjo가 실제로 뭘 지원하는가 (사실관계 원전) |
| `Passthrough-Black-Screen.md` | **MR을 켰는데 검은 화면일 때** — 콘솔만으로 원인 가리기 |
| `Editor-Followup.md` | 에디터가 열려 있어야 하는 미완 작업 (PP_MR 알파, WBP 행, 마커 ID) |

**애셋 경로 규칙**: `/CXMR/...` = 플러그인(템플릿 공통, 건드리면 모든 프로그램에 영향).
`/Game/...` = 이 프로젝트(프로그램별 데이터). **프로그램마다 다른 값은 전부 `/Game/`에 둔다.**

---

## 1. 5분 시작

1. `GMTCK_MR.uproject` 열기
2. `/Game/Map/L_Main` 열기 → **Play**
3. 보여야 하는 것: 앞쪽 2.5m에 흰 큐브 차량, 모니터에 따로 뜨는 컨트롤 창(진행자용), 왼쪽 위의 튜닝 창(수치 입력, §7).
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
| `K` | View Offset — `EYE`(눈 위치) ↔ `CAMERA`(패스스루 카메라 위치) |
| `T` | Depth Test |
| `U` | Environment Depth Estimation |
| `Y` | **Depth Test Range on/off** (§2-1) |
| `←` `→` | **Depth Range NearZ 감소/증가** |
| `↓` `↑` | **Depth Range FarZ 감소/증가** |
| `V` | 마커 추적 on/off |
| `H` | 손 추적 시각화 |
| `R` | 재캘리브레이션 |
| `E` | 차량을 내 앞에 배치 |
| `F` | 컨트롤 패널 클릭 (레이저가 가리키는 곳) |

### 넘버패드 — 차량 위치 미세조정 (§3-1)

**내가 보고 있는 방향 기준**이다(2026-09-14 변경). 차가 어느 쪽을 향해 있든 같은 키는 같은 방향으로 움직인다.

| 키 | 차가 움직이는 방향 |
|---|---|
| `NumPad 7` / `NumPad 9` | 내게서 멀어짐 / 가까워짐 (머리 방향을 수평으로 편 기준) |
| `NumPad 1` / `NumPad 3` | 왼쪽 / 오른쪽 |
| `NumPad 0` / `NumPad .` | 위 / 아래 |
| `NumPad 4` / `NumPad 6` | 왼쪽으로 돌기 / 오른쪽으로 돌기 (위에서 볼 때 반시계 / 시계) |
| **저장** `NumPad Enter` | 조정값을 마커 기록에 반영하고 파일로 저장 |
| **초기화** `NumPad *` | 조정값을 0으로 |

**회전 중심**은 `VehicleRoot → Placement → Nudge Pivot`이다. 기본 `Markers`는 보이는 캘리브레이션 마커들의 중심이라,
마커 옆에서 먼저 맞춰 둔 부분이 돌려도 제자리에 남는다. `Viewer`(내 머리), `VehicleOrigin`(차 원점)도 고를 수 있다.

컨트롤 패널 Calibration 탭과 튜닝 창은 **차의 월드 위치**를 보여 준다(예전 패널의 X/Y/Z/Yaw는 차 기준 내부값이라 키 방향과 부호가 맞지 않았다).

⚠️ **NumLock이 꺼져 있으면 하나도 안 먹는다.** 꺼진 상태에서는 NumPad 7이 Home, 9가 PageUp으로
전달되어 매핑이 통째로 빗나간다. 아무 반응이 없으면 NumLock부터 확인한다.

이전에는 **차 기준 축 + 반대 방향**이라, 차가 돌아가 있거나 기울어 있으면 "왼쪽으로 돌기가 위로 움직이는" 식으로
축이 꼬였다. 마커가 1개만 보일 때 마커의 기울기가 차에 그대로 들어간 것도 원인이어서, 이제 **차는 항상 수평**으로
놓는다(`Placement → Keep Level`, 기본 켬).

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

1. 마커를 **아무 데나** 붙인다 (잘 보이는 고정 부위)
2. 그 마커 번호들을 프로파일에 등록한다 (§3). 위치는 안 넣어도 된다 — **번호만 맞으면 된다**
3. 실행 → 차가 엉뚱한 자리에 뜬다 (아직 관계를 모르니 당연하다)
4. `E`로 대략 놓고, **넘버패드로 실물에 겹칠 때까지 맞춘다** (§2)
5. 콘솔에 **`CXMR.LearnMarkers`**
6. 끝. 이후 마커만 보이면 그 자리가 재현된다

5번이 하는 일: 지금 보이는 마커들의 포즈를 **차량 기준 상대값으로** 계산해
(`마커월드.GetRelativeTransform(차량월드)`) 프로파일에 쓰고, 파일로 저장한다.

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
Saved/CXMR/MarkerCalib_<id>.json          ← 현재 값 (저장할 때마다 덮어씀)
Saved/CXMR/MarkerCalib_<id>.startup.json  ← 이번 세션이 시작될 때의 값 (세션 중 불변)
```

`<id>`는 마커 프로파일의 **Calibration Id**다. 비워두면 애셋 이름을 쓴다.
전화로 불러줄 이름이므로 짧게 짓는다. **두 프로파일이 같은 id를 쓰면 서로의 파일을 덮어쓴다** —
로드할 때 파일 안의 프로파일 이름과 다르면 경고가 찍힌다.

`.startup`은 **되돌리기용**이다. 하루 종일 만지다 망쳐도 아침 상태로 갈 수 있다 —
`.startup.json`을 `.json`으로 복사하고 다시 로드하면 된다. 실수는 보통 연속으로 일어나므로
"직전 값"보다 "오늘 시작 전"이 실제로 필요한 지점이다.

내용은 **차량 기준 마커 포즈**뿐이다 — 차량 위치는 저장하지 않는다
(마커에서 매번 계산되므로 저장할 이유가 없다).

```json
{
  "profile": "DA_MarkerProfile_TestCar",
  "units": "cm, degrees; marker pose in vehicle-local space",
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
| `CXMR.SaveCalibration` | 현재 값을 파일로 (경로도 로그에 찍는다) |
| `CXMR.ResetCalibration` | 파일 삭제 + 프로파일이 원래 갖고 있던 값으로 복원 |

`NumPad Enter`(오프셋 저장)도 파일까지 쓴다.

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

- **프로파일에 등록된 마커 ID만** 학습된다. 미등록 마커를 자동으로 넣는 기능은 아직 없다.
  번호는 `CXMR.DebugMarkers 1` 또는 로그의 `LogCXMRDebug: DETECTED id=` 줄로 확인한다
- 학습 시점에 **모든 마커가 동시에 보여야** 서로의 상대 위치가 잡힌다
- 마커가 물리적으로 밀리면 저장값이 거짓이 된다 → 다시 `CXMR.LearnMarkers`

### 마커 몇 개를 쓸 것인가

`VehicleRoot` 액터 → `Placement` 컴포넌트에서 설정한다.

| 설정 | 의미 |
|---|---|
| **Min Markers To Calibrate** | 이 개수가 모여야 캘리브 완료로 친다. **1이면 단일 마커, 2 이상이면 멀티마커** |
| **Mode** | `MarkerAnchor`(마커로 정렬 — 실내) / `PawnRelative`(내 앞에 배치 — 실외 턴테이블) |
| **Freeze After Calibration** | 완료 후 이 컴포넌트가 차량 재배치를 멈춤 |
| **Stop Marker Tracking When Calibrated** | ⚠️ **기본 off로 둘 것.** 켜면 헤드셋의 마커 추적이 **전역으로** 꺼져서 DynamicObject 마커까지 죽는다 |

**단일 마커는 각도 노이즈가 증폭된다**(레버암). 마커 하나의 방향이 1° 틀어지면 3m 떨어진 차 뒤쪽은
5cm 밀린다. **마커 2개 이상을 쓰면** 시스템이 마커 *위치*들 사이의 기준선으로 yaw를 계산하고
roll/pitch는 0으로 고정한다(차는 바닥에 평평히 놓인다는 가정) — 훨씬 안정적이다.

### 지금 프로젝트 상태

`DA_MarkerProfile_TestCar`에는 **ID 0 하나, 오프셋 0**만 들어 있고 `Min Markers = 1`이다.
→ **0번 마커를 쓰면 차량이 마커 위에 정확히 얹힌다.** 다른 번호 마커는 무시된다.

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

### 데스크톱 컨트롤 창 ★ 진행자용

**실행하면 모니터에 별도 OS 창이 하나 뜬다.** 헤드셋 화면은 그대로고, 이 창은 데스크톱에 따로
있다(Varjo Lab이 앱 옆에 뜨는 것과 같은 방식).

CXR에서 헤드셋을 쓴 사람은 clay를 보러 온 결정권자지 조작자가 아니다. **세션은 옆에서 다른
사람이 돌린다.** 손목 패널을 레이저로 찌르게 하는 대신 마우스로 조작하라는 것이 이 창의 목적이다.

- **손 패널과 같은 패널 위젯(C++)을 쓴다.** 둘 다 서브시스템만 읽고 쓰므로 **상태가 저절로 동기화**된다 —
  손 패널을 켜 두었다면 창에서 MR을 켤 때 손목 패널 표시도 같이 바뀐다
- **HMD·컨트롤러 입력은 창 포커스와 무관하다**(OpenXR 런타임에서 직접 온다). 진행자가 창을
  클릭해도 착용자는 계속 컨트롤러를 쓸 수 있다
- ⚠️ **키보드 단축키는 포커스를 따른다.** 창을 클릭한 뒤에는 `M` `B` 같은 키가 게임에 안 간다.
  게임 화면을 한 번 클릭하면 돌아온다

설정은 `BP_CXMRPawn → Desktop Panel`:

| 프로퍼티 | 기본값 | 의미 |
|---|---|---|
| `Panel Class` | CXMR Control Panel (C++) | 창에 띄울 위젯 |
| `Open On Begin Play` | true | 시작하자마자 열기 |
| `Window Size` | 480 × 640 | 창 크기(픽셀) |
| `Window Title` | CXMR Control | 제목 표시줄 |

BP에서 `Open Window` / `Close Window` / `Toggle Window`를 부를 수 있다.
PIE를 멈추면 자동으로 닫힌다(Slate 창은 GC 대상이 아니라 명시적으로 닫지 않으면 에디터에 남는다).

### 튜닝 창 ★ 수치를 직접 넣는 곳

**실행하면 컨트롤 창과 별개로 `CXMR Tuning` 창이 모니터 왼쪽 위에 뜬다.** 컨트롤 창이 켜고 끄는 곳이라면,
이 창은 Depth 범위·노출·차 위치 이동량 같은 **숫자를 직접 넣는** 곳이다. 콘솔 `CXMR.Tuning`으로 열고 닫는다.

| 분류 | 항목 |
|---|---|
| **Vehicle placement** | 현재 차 위치(월드) · 보인 마커 수 · 한 번 누를 때 이동량(cm)·회전량(°) · 멀어짐/오른쪽/위/시계 방향 `[-][+]` · 회전 중심(마커/내 머리/차 원점) · 수평 유지 · 저장 / 마커 배치 학습 / 마커 다시 읽기 / 조정 취소 |
| **Mixed reality** | MR · VR 배경 · 마스킹 · View Offset(0 눈 ~ 1 카메라) |
| **Depth** | Depth Test · 범위 제한 · 범위 near / far(m) · 환경 depth 추정 |
| **Display** | 노출 보정(EV) |
| **Input** | 넘버패드 조정 속도 |
| **Hands** | 오른손·왼손 검지 끝 위치(머리 기준 cm) · 검지 끝 − 가장 가까운 마커 · 손 보정 앞/오른쪽/위(cm) · 마커에 맞추기 버튼 — 절차는 `Headset-Test-Checklist.md` §8 |

- **값은 매 프레임 실제 기능에서 읽어 온다.** 키보드·컨트롤러·컨트롤 창으로 바꾼 것도 바로 보인다
- 숫자 칸은 **드래그하거나 클릭해서 입력**한다. 드래그하는 동안 바로 적용되고, 손을 떼거나 Enter를 누를 때 저장된다
- **차 위치는 좌표를 넣는 게 아니라 이동량 + `[-][+]` 버튼**이다. 방향은 넘버패드와 같이 **내 시점 기준**이다(§2)
- `Default` 버튼은 그 항목을 기본값으로 되돌리고 저장값도 지운다
- ⚠️ 컨트롤 창과 같이 **키보드 단축키는 포커스를 따른다.** 창을 클릭한 뒤에는 게임 화면을 한 번 클릭해야 키가 다시 먹는다

**저장** — `Saved/CXMR/Tuning.json`, **PC별**(마커 캘리브레이션 파일과 같은 이유: 이 방·이 조명에 맞춘 값이다).

| 저장되는 것 (다음 실행에 그대로) | 저장 안 되는 것 (매 세션 새로) |
|---|---|
| View Offset, 범위 제한 on/off, 범위 near/far, 노출 보정, 이동량·회전량, 넘버패드 속도, 손 보정(앞/오른쪽/위) | MR·VR 배경·마스킹·Depth Test·환경 depth on/off, 회전 중심, 수평 유지 |

차 위치 자체는 이 파일이 아니라 **캘리브레이션 파일**(`Saved/CXMR/MarkerCalib_*.json`, §3-1)이 담당한다.
창의 "Save adjustment into the marker layout"은 `NumPad Enter`와 같다.
파일을 지우면 전부 기본값으로 돌아간다.

**노출 보정**은 레벨의 **Unbound Post Process Volume**의 Exposure Compensation을 덮어쓴다. 그런 볼륨이
없는 레벨에서는 아무 효과가 없다.

설정은 `BP_CXMRPawn → Tuning Window`:

| 프로퍼티 | 기본값 | 의미 |
|---|---|---|
| `Open On Begin Play` | true | 시작하자마자 열기 |
| `Window Size` | 480 × 900 | 창 크기(픽셀) |
| `Window Position` | (40, 60) | 창 위치(픽셀). 가운데 뜨는 컨트롤 창과 겹치지 않게 |
| `Window Title` | CXMR Tuning | 제목 표시줄 |

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

### 컨트롤 패널 모양과 탭 구조

**컨트롤 패널은 튜닝 창과 같은 모양이다**(2026-09-15). 둘 다 `CXMRPanelUI`가 그린다 — 어두운 바탕, 파란 분류 제목,
왼쪽에 이름 · 오른쪽에 체크박스나 버튼. 데스크톱 컨트롤 창과 손 패널이 같은 패널을 쓴다.

**위젯 블루프린트(`WBP_CXMRControlPanel`)는 더 이상 쓰지 않는다.** 패널은 C++(`UCXMRControlPanelWidget`)이 직접 만든다.
예전에는 WBP의 위젯 이름으로 C++과 연결돼 있어서, 이름이 바뀌거나 위젯이 사라지면 행이 조용히 동작을 멈췄다.

| 탭 | 내용 |
|---|---|
| **Display** | Mixed reality: MR · VR 배경 · Render from(눈/카메라) · 마스킹 / Depth: Depth test · 환경 depth · 범위 제한 · 현재 범위 / Tracking: 마커 추적 · 손 스켈레톤 |
| **Calibration** | 차 위치(월드) · 보인 마커 수 · 마커 다시 읽기 · 내 앞에 배치 / 수동 조정: 저장 · 조정 취소 (이동 자체는 넘버패드나 튜닝 창) |
| **Viewer** | 차량 `[-][+]` · 트림 `[-][+]` · CMF · 다음 CMF / Human factors: 매니킨 `[-][+]` |

- 체크박스는 기능의 실제 상태를 매 프레임 읽는다. 키보드·컨트롤러로 바꿔도 바로 반영된다
- 헤드셋이 지원하지 않는 항목(MR, 마커 추적)은 회색으로 비활성화된다
- 예전 CALIB 탭의 X/Y/Z/Yaw는 차 기준 내부값이라 키 방향과 부호가 맞지 않았다. 대신 **차의 월드 위치**를 보여 준다
- 내용이 `Panel Draw Size`보다 길면 잘리지 않고 스크롤된다

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
| `H`를 눌러도 손이 안 나온다 | `OpenXRHandTracking` 플러그인 활성 여부. 로그 `LogCXMRHands: Hand tracker present:` |
| 캘리브 후 다른 마커가 안 잡힌다 | `Stop Marker Tracking When Calibrated`가 켜져 있다 → 끈다 |
| **넘버패드를 눌러도 차가 안 움직인다** | ① **NumLock 확인** ② 로그에 `Marker offset adjusted`가 찍히는지 본다. 안 찍히면 입력이 도달하지 않은 것이고, 찍히는데 안 움직이면 배치 계산 쪽이다 |
| 재시작하면 캘리브레이션이 사라진다 | `Saved/CXMR/MarkerCalib_*.json`이 있는지 확인. 없으면 저장이 안 된 것 — `CXMR.SaveCalibration`을 치면 경로가 로그에 찍힌다 (§3-1) |
| `CXMR.LearnMarkers`가 아무것도 안 한다 | 마커가 하나도 안 잡혔거나, 잡힌 마커가 프로파일에 없다. 로그에 이유가 찍힌다 |
| 문을 열었더니 차가 튄다 | 문에 마커가 붙어 있다. 움직이는 부품에는 붙이면 안 된다 (§3-1) |
| 손 패널이 한눈에 안 들어온다 | 잘리지 않고 스크롤된다. 한 번에 다 보이게 하려면 `Panel Draw Size` Y를 키우고 `Panel Scale`을 줄인다 (§7). 재빌드 불필요 |

**로그 카테고리**: `LogCXMR`(서브시스템) `LogCXMRHands`(손) `LogCXMRDebug`(마커)
`LogCXMRErgo`(착좌) `LogCXMRPawn`(패널) `LogCXMRPlacement`(배치·캘리브) `LogCXMRMask`(마스킹)
`LogCXMRVehicle`(차량 로더) `LogCXMRInput`(입력)

**마커 계측기**: 콘솔에 `CXMR.DebugMarkers 1` — 플러그인이 보고하는 마커 pose를 **가공 없이** 그린다.
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
