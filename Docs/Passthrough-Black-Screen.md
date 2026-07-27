# 패스스루가 안 뜨고 검은 화면 — 원인 가리기

`M`(MR) + `B`(VR 배경 off)에서 실제 방이 아니라 검정만 보이는 증상.

**리빌드도 에디터도 필요 없다.** 지금 돌고 있는 빌드 그대로 콘솔에서 5분이면 원인이 갈린다.

---

## 왜 검은가

Varjo는 패스스루를 직접 합성하지 않는다. OpenXR 컴포지터의 alpha-blend 모드에 얹히고,
조건은 이것 하나다 (Varjo 원문):

> *"Pixels with RGBA(0,0,0,0) display only the VST image"*

**알파가 0인 픽셀에만** 실세계가 보인다. `B`로 하늘·바닥을 숨겨도 **그 자리의 알파가 1이면
그냥 검정**이다. 즉 "배경을 숨겼는데 검다"는 것은 **알파가 0으로 내려가지 않았다**는 뜻이다.

알파를 1로 붙잡아둘 수 있는 곳은 네 군데뿐이다. 아래는 그 넷을 하나씩 떼어보는 절차다.

---

## 0. 먼저 이것부터 (질문 하나)

검은 화면에서 **흰 테스트 차량과 컨트롤 패널은 보이는가?**

| 관측 | 의미 |
|---|---|
| **보인다** | VR 렌더는 정상. 배경 알파만 문제 → **1번으로** |
| **아무것도 없이 완전 검정** | 알파 문제가 아니다. 렌더 자체가 죽은 것이므로 아래 절차는 의미 없다 |

---

## 1. 상태 한 번에 읽기

리빌드했다면 콘솔에:

```
CXMR.DumpMRState
```

한 번에 전부 찍힌다. **콘솔로는 못 보는 것이 하나 들어 있다** — Varjo 플러그인이 판단하는
MR 상태와 CXMR이 캐시한 상태의 대조다. 둘이 어긋나면 알파 이전에 그게 원인이다.

리빌드 전이면 콘솔에 아래를 하나씩 쳐서 값만 읽는다(변경 아님):

```
xr.OpenXREnvironmentBlendMode
r.PostProcessing.PropagateAlpha
r.SceneColorFormat
r.AntiAliasingMethod
```

### 읽는 법

| 값 | 판정 |
|---|---|
| `xr.OpenXREnvironmentBlendMode`가 **3이 아니다** | MR이 실제로 안 켜졌다. **알파 문제가 아니다** — 여기서 멈추고 이 사실을 알린다 |
| `r.PostProcessing.PropagateAlpha`가 **0** | **원인 D 확정.** 어떤 픽셀도 투명해질 수 없다 → 2-D로 |
| 둘 다 정상 | 알파 경로 어딘가가 범인. 2번으로 |

---

## 2. 하나씩 떼어보기

**각 명령을 친 뒤 `M`+`B` 상태에서 실제 방이 보이는지만 본다.**
어느 하나에서 패스스루가 나타나면 거기서 멈춘다 — 그게 원인이다.

### 2-C. Scene color format

```
r.SceneColorFormat 3
```

> 이 프로젝트는 `r.SceneColorFormat`이 설정돼 있지 않고 `DefaultScalability.ini`도 없어서
> **품질이 Epic 기본**이다. `Varjo-Capabilities.md` §2가 정확히 이 조합을 경고한다 —
> Epic의 64bit `PF_FloatRGBA` 포맷이 알파를 이상하게 만든다는 것. 처방이 `r.SceneColorFormat 3`인데
> 지금 적용돼 있지 않다.

### 2-B. TSR이 알파를 뭉개는 경우

```
r.TSR.AlphaChannel 1
```

안 되면(또는 `<not registered>`면):

```
r.AntiAliasingMethod 2
```

> `r.AntiAliasingMethod=3`(TSR)인데 `r.TSR.AlphaChannel`이 설정돼 있지 않다.
> TSR이 알파를 보존하지 않으면 배경이 불투명하게 resolve된다. `2`는 TAA로 후퇴.

### 2-A. 포스트프로세스 체인 (PP_MR)

```
ShowFlag.PostProcessing 0
```

색이 이상해지는 건 정상이다(톤매퍼까지 꺼진다). **패스스루 유무만** 본다.

> 여기서 패스스루가 뜨면 PP 체인이 범인이고, `PP_MR`이 유력하다.
> `Varjo-Capabilities.md` §4-1 step 7이 기록한 로직이
> *"`CustomDepth < SceneDepth` → Opacity 0, **아니면 1.0**"* 이라, 마스크 구멍 **바깥 전 영역에
> 불투명 알파를 칠하게 된다.*
>
> 정밀 확인(에디터): `L_Main` → `PostProcessVolume_0` →
> `Rendering Features > Post Process Materials`에서 **`PP_MR`만 배열에서 뺀다.**

### 2-D. 알파 전파가 아예 꺼진 경우

`r.PostProcessing.PropagateAlpha`는 런타임 변경이 안 되는 설정이다. config는
`Config/DefaultEngine.ini`의 `[/Script/Engine.RendererSettings]`에 `=True`로 들어 있으므로,
런타임에 0이면 **config가 안 먹은 것**이다. 섹션 위치·중복 키·로드 순서를 본다.

---

## 3. 결과별 영구 반영

⚠️ **2번의 콘솔 변경은 세션 한정이다.** 껐다 켜면 원복된다.

| 판정 | 조치 | 누가 |
|---|---|---|
| **C** | `Config/DefaultEngine.ini`에 `r.SceneColorFormat=3` | 코드 쪽에서 처리 가능 |
| **B** | `r.TSR.AlphaChannel=1`, 안 되면 `r.AntiAliasingMethod=2` | 〃 |
| **A** | `PP_MR`에서 구멍 바깥은 입력 알파를 그대로 통과시키도록 수정 | **에디터 필요** |
| **D** | config 로드 경로 수정 | 코드 쪽 |

### A인 경우의 머티리얼 수정

`/CXMR/Core/Materials/PP_MR`:
- 구멍(`CustomDepth < SceneDepth` **이고** `MRMask`=1) → Opacity 0 (지금과 동일)
- **그 외** → 상수 1.0 대신 `SceneTexture:PostProcessInput0`의 **알파를 그대로 통과**

⚠️ `PP_MR`은 `/CXMR/` 공용 콘텐츠다 — 고치면 모든 프로그램에 영향이 간다.
⚠️ 수정 후 **`N`(마스킹) 회귀 확인 필수** — 마스크 큐브 구멍은 계속 뚫려야 한다.

### B를 택할 때 주의

`r.AntiAliasingMethod`를 TSR에서 바꾸면 `Varjo-Capabilities.md` §17의 "Varjo가 검증해 배포한
예제 설정"에서 벗어난다. TSR을 유지한 채(`r.TSR.AlphaChannel=1`) 해결되면 그쪽을 택한다.
화질·성능을 다시 본다.

---

## 4. 넷 다 아니면

용의자 목록 밖이다. 다음을 본다:

- OpenXR 스왑체인 포맷이 알파를 담는가
- `r.DefaultBackBufferPixelFormat=4`가 이 엔진 버전에서 알파를 가진 포맷인가
- 레벨에 알파 1을 쓰는 전역 요소가 남아 있는가 (`B`로 안 숨는 액터 — `CXMRSceneObjectComponent`가
  안 붙은 배경물. `Operating-Guide.md` §5의 경고)
