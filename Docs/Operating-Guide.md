# CXMR 사용법

이 프로젝트를 **어떻게 작동시키는가**. 마커 번호를 바꾸고, 차량을 갈아끼우고, 세션을 돌리는 실무 절차.

| 문서 | 용도 |
|---|---|
| **이 문서** | 사용법 — 뭘 어디서 바꾸고 어떻게 돌리는가 |
| `Headset-Test-Checklist.md` | 헤드셋 실측 순서와 "정상인데 안 되는 것처럼 보이는" 경우 |
| `Varjo-Capabilities.md` | Varjo가 실제로 뭘 지원하는가 (사실관계 원전) |
| `Editor-Followup.md` | 에디터가 열려 있어야 하는 미완 작업 (PP_MR 알파, WBP 행, 마커 ID) |

**애셋 경로 규칙**: `/CXMR/...` = 플러그인(템플릿 공통, 건드리면 모든 프로그램에 영향).
`/Game/...` = 이 프로젝트(프로그램별 데이터). **프로그램마다 다른 값은 전부 `/Game/`에 둔다.**

---

## 1. 5분 시작

1. `GMTCK_MR.uproject` 열기
2. `/Game/Map/L_Main` 열기 → **Play**
3. 보여야 하는 것: 앞쪽 2.5m에 흰 큐브 차량, 왼손을 들면 컨트롤 패널

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

패널은 **왼손에 붙어** 있고 **오른손 레이저**로 누른다.

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

**실측 절차**:
1. 차량 3D 모델에서 마커를 붙일 지점의 좌표를 읽는다 (차 원점 기준, cm)
2. 실물에도 **같은 자리에** 마커를 붙인다
3. 그 좌표를 Local Offset의 Translation에 넣는다
4. 마커가 기울어져 붙는다면 Rotation도 넣는다 (대시보드 경사 등)

**Local Offset을 0으로 두면** 차량 원점이 마커 위에 정확히 얹힌다. 테스트할 땐 이게 편하다.

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
| **VR Only** | VR 배경을 끄면 스스로 숨는다 | 하늘, 바닥, 벽, 안개, 구름 — **MR에서 실제 세계를 가리는 모든 것** |
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

**조작**: 패널의 `< 착좌 >` 버튼, 또는 `VarjoInput → Cycle Manikin Action`에 Axis1D IA를 지정하면
스틱 flick으로도 순환한다(트림/차량 순환과 같은 히스테리시스).

⚠️ **이 기능은 오랫동안 발동 경로 자체가 없었다.** `UCXMRErgonomicsComponent`는 완전히 구현돼
구독까지 하고 있었지만 `RequestErgonomicsStep`을 부르는 곳이 하나도 없어서, 이 문서가 설명하는
동작을 아무도 볼 수 없었다. 지금은 패널 버튼이 기본 경로다.

동작이 모드에 따라 **정반대**다:
- **VR**: 세계가 가상이므로 **내 시점을 옮긴다**
- **MR**: 실제 몸은 못 옮기므로 **차를 옮긴다**
- **MR + MarkerAnchor**: **아무것도 안 옮긴다.** eye point가 이미 물리적으로 실재하므로
  (실물 시트) 차를 옮기면 캘리브레이션과 싸운다. 로그에 이유가 찍힌다

⚠️ Hip Point는 저장만 하고 아직 이동에 쓰지 않는다.

---

## 7. 컨트롤 패널 조정

`BP_CXMRPawn` → `Control Panel` 관련 프로퍼티. **C++ 리빌드 불필요.**

| 프로퍼티 | 기본값 | 의미 |
|---|---|---|
| `Panel Offset` | (8, 0, 4) | 왼손 컨트롤러 기준 위치(cm) |
| `Panel Rotation` | pitch −25, yaw 180 | 기울기. yaw 180이 착용자 쪽을 향하게 한다 |
| `Panel Scale` | 0.03 | cm/픽셀. 432×721px → 약 13×22cm |
| `Panel Draw Size` | 432×721 | WBP 콘텐츠 크기와 **일치해야** 잘리지 않는다 |
| `Control Panel Class` | WBP_CXMRControlPanel | 다른 패널로 교체 가능 |

**패널이 안 보이면** 너무 가까워 근접 클리핑(10cm)에 잘린 것이다 — `Panel Offset`의 X를 늘린다.

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
| 착좌 순환이 반응 없다 | 패널 `< 착좌 >` 버튼을 쓰거나 `Cycle Manikin Action`에 IA를 지정한다 (§6) |
| 차가 마커에서 어긋난 곳에 놓인다 | `Local Offset`이 틀렸다 (§3) |
| MR을 켜도 실제 세계가 안 보인다 | 배경 오브젝트에 VROnly가 없다 → `B`로 확인 |
| 패널의 Mixed Reality가 안 켜진다 | CVar를 못 찾은 것. 로그에 `LogCXMR: Warning: Mixed reality toggle ignored` |
| 턴테이블이 안 돈다 | `Placement → Mode`가 `MarkerAnchor`다. 실내에서는 회전을 막는 게 설계다 |
| 손이 허공에 얼어붙어 있다 | 그럴 수 없다 — 추적이 끊기면 그리지 않는다. 보인다면 실제로 추적 중이다 |
| `H`를 눌러도 손이 안 나온다 | `OpenXRHandTracking` 플러그인 활성 여부. 로그 `LogCXMRHands: Hand tracker present:` |
| 캘리브 후 다른 마커가 안 잡힌다 | `Stop Marker Tracking When Calibrated`가 켜져 있다 → 끈다 |

**로그 카테고리**: `LogCXMR`(서브시스템) `LogCXMRHands`(손) `LogCXMRDebug`(마커) `LogCXMRErgo`(착좌) `LogCXMRPawn`(패널)

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
