# NipsEngine — Week 9+ Cinematic Camera & Impact Feedback

> **PlayerCameraManager · Camera Modifier · Spring Arm · Screen Effects · Hit Feedback · Release Packaging**

9주차 게임잼 결과물인 `Drift Salvage`에 카메라 기반 시네마틱 연출, 화면 후처리, 시간 제어와 충돌 피드백을 추가한 프로젝트입니다.

C++에는 재사용 가능한 카메라·후처리·시간 관리 기반을 구현하고, Lua에서는 이 기능을 조합해 게임 시작, 피격, 게임오버와 등대 도착 연출을 구성했습니다.

이 문서는 9주차의 Lua 게임잼 기반 기능을 반복해서 설명하지 않고, 9+주차 과제에서 추가하거나 확장한 코드와 최종 게임 적용 상태에 집중합니다.

## 학습 목표

- 게임잼 결과물에 카메라 기반 시네마틱 연출을 추가한다.
- 게임 안에 팀 번호와 구성원의 이름을 표시한다.
- Player Camera Manager, Camera Component, Camera Modifier의 역할을 이해한다.
- Camera Shake, Letter Box, Fade In/Out, Spring Arm과 Camera Transition을 구현한다.
- Hit Stop, Hit Squash, Knockback과 Slomo로 타격감을 개선한다.
- Gamma Correction과 Vignetting을 후처리로 구현한다.
- Release Standalone Build 형태로 실행 가능한 결과물을 구성한다.

## 9주차와 9+주차 범위

9주차에는 LuaJIT·Lua Script Component·Coroutine·Delegate·충돌 Event를 이용해 `Drift Salvage`의 게임 규칙, UI, 점수와 시작·종료 흐름을 구현했습니다.

9+주차에는 이 게임 위에 다음 기능을 추가했습니다.

- View Target과 Camera Transition을 관리하는 Camera Manager
- Wave·Sequence Camera Shake와 FOV Kick
- Spring Arm Component
- Fade, Letter Box, Vignette, Gamma Correction
- Scaled/Unscaled Time과 Hit Stop·Slomo
- Hit Squash, Knockback, Camera Shake, Sound를 결합한 충돌 반응
- 시작·피격·게임오버·등대 도착 Camera Direction
- Camera Sequence 데이터와 전용 Editor
- Release 패키징 Batch

## 구현 요약

| 영역 | 구현 내용 | 최종 게임 적용 |
| --- | --- | --- |
| Camera Manager | View Target, Transition, Modifier, 최종 POV 계산 | 적용 |
| Camera Shake | Wave Oscillator와 JSON Camera Sequence Pattern | 충돌·피격 및 테스트 입력에 적용 |
| FOV Kick | 시간 기반 FOV Pulse Modifier | API 구현, 게임 호출은 주석 처리 |
| Spring Arm | 거리, 회전 상속, 충돌 축소, 위치 Lag | Component 구현, 최종 Scene에는 미배치 |
| Screen Effects | Fade, Letter Box, Vignette, Gamma Correction | Fade·Letter Box·Vignette 적용, Gamma는 샘플에서만 호출 |
| Time Manager | Scaled/Unscaled Delta, Hit Stop, Slomo | Slomo 적용, Hit Stop 호출은 비활성 |
| Hit Feedback | Hit Squash, Knockback, Shake, Slomo, Sound | `HitReaction.lua`와 HUD에 적용 |
| Cinematic Direction | Intro, Game Over, Lighthouse Outro Camera | `DriftSalvageHud.lua`에서 적용 |
| Team Credit | Team 4와 구성원 이미지 Decal | 최종 Scene에 배치 |
| Sequence Editor | JSON Channel·Key·Bezier 편집 | Sequence Shake 제작·테스트에 사용 |
| Release Build | 실행 파일과 런타임 폴더 패키징 | Batch 제공, 의존성 복사 제한 존재 |

## 전체 카메라·연출 흐름

```text
Lua Gameplay / HUD
        │
        ├─ Camera API ───────────────┐
        │                            ▼
        │                 FWorldCameraInterface
        │                            │
        │                            ▼
        │                 FPlayerCameraManager
        │                 ├─ View Target Blend
        │                 ├─ Camera Modifier
        │                 └─ Screen Effect State
        │                            │
        │                            ├─ Final POV → Viewport Camera
        │                            └─ Effect State → RenderBus
        │                                              │
        └─ HitFeel / TimeManager ─> FTimeManager        ▼
                                     │       ScreenEffectsRenderPass
                                     ▼
                              Scaled Gameplay Tick
```

## 1. PlayerCameraManager와 View Target

`FPlayerCameraManager`는 World가 소유하며 현재 View Target, 전환 대상, Blend Timeline, Camera Modifier와 화면 효과 상태를 함께 관리합니다.

매 갱신 시 다음 순서로 최종 카메라를 계산합니다.

1. Fade, Letter Box와 Vignette Timeline을 갱신한다.
2. 현재·전환 대상의 `UCameraComponent`에서 기본 POV를 읽는다.
3. View Target 전환 중이면 두 POV를 보간한다.
4. Camera Modifier를 적용한다.
5. 최종 POV를 `FViewportCamera`에 전달한다.

유효하지 않은 Actor가 View Target으로 지정되면 Camera Component를 가진 첫 번째 Pawn을 우선 찾고, 없으면 첫 Camera Actor로 폴백합니다. 움직이는 Camera를 따라갈 수 있도록 현재와 전환 대상의 POV를 매 프레임 다시 읽습니다.

### Camera Transition

`SetViewTargetWithBlend`는 위치, 회전, FOV와 Clip Plane을 시간에 따라 보간합니다.

- 위치: Linear Interpolation
- 회전: Quaternion Slerp
- FOV·Near/Far Clip·Ortho Height: Scalar Interpolation
- Blend Function: Linear, SmoothStep, Ease In, Ease Out, Ease In Out

Perspective와 Orthographic 사이의 Projection Type은 Blend 도중 혼합하지 않고 전환이 완료되는 시점에 대상 값을 사용합니다.

View Target Blend와 화면 효과, Camera Modifier는 Unscaled Delta Time을 받도록 설계되어 Slomo 중에도 연출 시간이 게임 속도 배율에 종속되지 않습니다.

## 2. Camera Modifier

일시적인 카메라 변화는 `FCameraModifier` 파생 객체로 분리했습니다. Manager는 Modifier를 Priority 기준으로 정렬하고, `ModifyCamera`가 `false`를 반환하거나 비활성화된 Modifier를 제거합니다.

### Wave Camera Shake

`FWaveOscillatorPattern`은 축별 진폭과 주파수로 사인파를 계산합니다.

- Location X/Y/Z
- Rotation Pitch/Yaw/Roll
- FOV

Shake 결과에는 남은 수명에 비례한 `1 - NormalizedTime` Weight를 적용해 종료 지점으로 갈수록 감쇠시킵니다.

`HitReaction.lua`는 충돌 시 0.18초의 Rotation Shake를 사용하고, `DriftSalvageHud.lua`는 체력이 감소할 때 Location Shake를 실행합니다.

### Sequence Camera Shake

`FSequenceCameraShakePattern`은 JSON Sequence의 현재 시각을 평가해 Location, Rotation, FOV Offset을 생성합니다. Sequence 파일의 마지막 Key 시각이 Modifier Duration이 됩니다.

`PlayerController.lua`에는 다음 테스트 입력이 있습니다.

- `P`: 3초 Wave Oscillator Shake
- `L`: `SampleCameraShake` Sequence를 Scale 0.1로 재생

### FOV Kick

`FFOVKickModifier`는 전달받은 Degree 값을 Radian으로 변환하고, `sin(πt)` Pulse로 FOV를 `0 → Peak → 0` 형태로 변화시킵니다.

Lua `Camera.FOVKick`까지 바인딩되어 있지만 `PlayerController.lua`의 호출은 주석 처리되어 최종 게임 흐름에서는 사용되지 않습니다.

## 3. Spring Arm Component

`USpringArmComponent`는 Camera와 기준 Component 사이의 거리, 회전 상속, 충돌 축소와 위치 Lag를 계산합니다.

- Target Arm Length와 Socket Offset
- Pitch, Yaw, Roll 개별 상속
- 장애물 충돌 시 Arm Length 축소
- Arm Length 복원 Blend
- Camera Location Lag
- 충돌 Ray Debug Draw

대상 Camera 탐색 순서는 직접 연결된 자식 Camera, Pawn Camera, Owner의 첫 Camera Component입니다.

충돌 검사는 Pivot에서 목표 Socket까지 `LineTraceSingle`로 수행합니다. `CollisionProbeRadius`는 Sphere Sweep 반지름으로 쓰이지 않고, Hit Distance에서 Padding과 함께 차감하는 안전 여유값으로 사용됩니다.

Spring Arm은 Component Factory와 Property Editor에 등록되어 있으나, 최종 `Scene_Game.Scene`에는 `USpringArmComponent`가 배치되어 있지 않습니다.

## 4. Camera Sequence와 Editor

Camera Sequence는 `Asset/Sequences` 아래 JSON 파일로 저장합니다.

```text
Location.X / Y / Z
Rotation.X / Y / Z
FOV
```

각 Key는 Time, Value, Interpolation Mode와 Arrive/Leave Time·Tangent를 가집니다.

- Linear 구간은 두 Key 값을 선형 보간합니다.
- Bezier 구간은 시간 Control Point를 구성하고 이분 탐색으로 현재 시간에 대응하는 Curve Parameter를 찾습니다.
- 중복되지 않는 파일 Stem은 경로 없이 이름만으로 불러올 수 있습니다.
- 같은 Stem이 여러 개면 전체 상대 경로를 요구합니다.
- 로드한 Sequence는 Cache하고 Reload 시 해당 Cache를 무효화합니다.

Camera Sequence Editor는 다음 기능을 제공합니다.

- 새 Sequence 생성
- 파일 목록 갱신, 불러오기, 저장, Reload
- Channel별 Key 추가·삭제·Drag
- Linear·Bezier 모드 선택
- Arrive/Leave Time과 Tangent 편집
- Time·Value 영역 Framing

현재 Sequence는 독립 Cutscene Actor나 Timeline이 아니라 Camera Shake Pattern의 Offset 데이터로 사용됩니다.

## 5. Screen Effects Render Pass

`FScreenEffectsRenderPass`는 이전 Pass의 Scene Color를 입력으로 받아 Full-screen Triangle 하나로 화면 효과를 합성합니다.

| 효과 | 처리 방식 |
| --- | --- |
| Vignette | Aspect Ratio를 보정한 화면 중심 거리로 가장자리 감쇠 |
| Letter Box | 상·하단 UV 영역을 검은색으로 Mask |
| Fade In/Out | Fade Color와 Scene Color를 Amount로 보간 |
| Gamma Correction | `pow(Color, 1 / Gamma)` 적용 |

Shader 내부 적용 순서는 Vignette, Letter Box, Fade, Gamma Correction입니다. 모든 효과가 비활성인 프레임은 이전 Pass의 SRV/RTV를 그대로 전달해 Full-screen Draw를 생략합니다.

### 최종 게임에서의 사용

- 게임 시작: `IntroCamera` 전환과 Letter Box
- 플레이 시작·재시작: Fade In/Out
- 체력 감소: 붉은 Vignette Flash와 Camera Shake
- 낮은 체력: 지속 Vignette
- 체력 0: 강한 Vignette와 Game Over Camera

Gamma Correction은 엔진·Lua API와 Shader에는 구현되어 있지만, 호출하는 `IntroDirector.lua`가 최종 실행 경로에 연결되어 있지 않아 실제 게임 HUD 연출에서는 활성화되지 않습니다.

## 6. Time Manager와 Hit Feedback

`FTimeManager`는 원본 Frame Delta와 게임에 전달할 Scaled Delta를 분리합니다.

```text
Unscaled Delta Time
        │
        ├─ Camera Blend / Shake / Screen Effect Duration
        ├─ Hit Stop·Slomo Remaining Time
        └─ Global Time Dilation 적용
                         │
                         ▼
                  Scaled Gameplay Delta
```

- Hit Stop과 Slomo의 남은 시간은 Unscaled Delta로 감소합니다.
- Hit Stop과 Slomo가 겹치면 Hit Stop을 우선합니다.
- 중첩 Slomo 요청은 더 느린 Time Scale과 더 긴 Duration을 유지합니다.
- Lua에서 Scaled·Unscaled Delta와 현재 Time Scale을 조회할 수 있습니다.

### HitReaction.lua

`HitReaction.lua`는 Actor Tag에 따라 반응 Profile을 선택합니다.

- Tire가 Boat와 충돌하면 Static Mesh Scale을 사인파로 확대했다 복원합니다.
- 충돌 방향을 계산해 Boat에 Knockback을 적용합니다.
- 0.35배, 0.25초 Slomo를 실행합니다.
- Rotation Camera Shake와 Tire 충돌 Sound를 재생합니다.
- Boat와 Rock의 충돌에도 공통 Slomo·Shake 피드백을 적용합니다.

Hit Stop Engine·Lua API는 구현되어 있지만 현재 Script 호출은 주석 처리되어 실제 충돌 피드백에는 Slomo가 사용됩니다.

## 7. Lua 기반 Cinematic Direction

최종 게임 연출은 `IntroDirector.lua`가 아니라 Engine이 World 활성화 시 생성하는 `DriftSalvageHud.lua` Script Component에서 실행됩니다.

### 게임 시작

1. Title UI에서 Start를 선택한다.
2. `IntroCamera`로 즉시 전환한다.
3. Letter Box를 0.5초 동안 표시하고 파도 Sound를 재생한다.
4. 3초 후 Player Camera로 1.5초 Blend한다.
5. Letter Box를 해제한 뒤 Countdown과 Gameplay를 시작한다.

### 체력 0 Game Over

- `GameOverCamera`를 찾고 없으면 런타임에 생성합니다.
- Boat 진행 방향을 기준으로 Camera 위치를 계산하고 Boat를 바라보게 합니다.
- Camera를 3초 동안 Blend하면서 Boat를 아래로 가라앉히고 회전시킵니다.
- 연출이 끝나면 Game Over UI를 표시합니다.

### 등대 도착 Outro

- `CineCamera_Outro`로 3초 동안 Blend합니다.
- 0.4배 Slomo를 적용합니다.
- Boat를 감속시키며 등대 방향으로 이동시킨 뒤 정지합니다.
- 잠시 정적인 여백을 둔 후 Ending UI를 표시합니다.

### 팀 정보 표시

최종 Scene의 Outro 구간에는 다음 Texture를 사용하는 Decal Actor가 배치되어 있습니다.

- `Team4.png`
- `기홍.png`
- `준혁.png`
- `형도.png`
- `효범.png`

따라서 팀 번호와 구성원 표시 요구는 최종 Scene에서 충족합니다. `IntroDirector.lua`에 남아 있는 `Team 7`, `Member A / B / C` 문구는 실행 경로에 연결되지 않은 초기 샘플입니다.

## 8. Lua Camera API

Camera와 시간 기능은 Lua가 내부 C++ 클래스에 직접 의존하지 않도록 API Table로 노출했습니다.

| Lua API | 기능 |
| --- | --- |
| `Camera.SetViewTargetWithBlend` | 기본 SmoothStep Camera 전환 |
| `Camera.SetViewTargetWithBlendEx` | Blend Function을 지정한 전환 |
| `Camera.Shake` | Wave 또는 Sequence Shake 생성 |
| `Camera.FOVKick` | FOV Pulse 생성 |
| `Camera.FadeIn`, `FadeOut` | 화면 Fade 제어 |
| `Camera.SetLetterBox` | Letter Box 높이와 Blend 제어 |
| `Camera.SetVignette` | 강도·반경·Softness·색상·Blend 제어 |
| `Camera.EnableGammaCorrection` | Gamma Correction On/Off |
| `HitFeel.HitStop`, `Slomo` | 충돌 연출용 시간 제어 |
| `TimeManager.*` | Time Scale과 Delta 조회·제어 |

Lua 요청은 `FWorldCameraInterface` 또는 `UWorld`의 `FTimeManager`를 거쳐 실제 시스템에 전달됩니다.

## 빌드 및 실행

### 요구 환경

- Windows 10/11 64-bit
- Visual Studio 2022
- MSVC v143 Toolset
- Windows 10 SDK
- Direct3D 11 지원 GPU와 Driver
- 저장소에 포함된 FMOD Header·Library와 Runtime DLL

### 실행 방법

1. `GenerateProjectFiles.bat`을 실행합니다.
2. `NipsEngine.sln`을 Visual Studio에서 엽니다.
3. `Release | x64`로 빌드하거나 `ReleaseBuild.bat`을 실행합니다.
4. 빌드 결과 또는 패키징 결과의 `NipsEngine.exe`를 실행합니다.

Visual Studio 빌드 결과는 `NipsEngine/Bin/<Configuration>`에 생성됩니다. x64 빌드의 Post-Build Event는 `fmod.dll`을 해당 출력 폴더로 복사합니다.

`ReleaseBuild.bat`은 Release x64 빌드 후 기존 `ReleaseBuild` 폴더를 다시 만들고 다음 항목을 복사합니다.

- `NipsEngine.exe`
- `imgui.ini`
- `Shaders`
- `Asset`
- `Settings`를 복사하려는 경로
- 존재하는 경우 `Saves`

## 구현 범위

| 구분 | 9+주차 구현 상태 | 최종 콘텐츠 적용 |
| --- | --- | --- |
| Player Camera Manager | 구현 | 적용 |
| Camera Component·View Target | 구현 | 적용 |
| Camera Transition·Blend Function | 구현 | 적용 |
| Wave Camera Shake | 구현 | 적용 |
| Sequence Camera Shake | 구현 | 테스트 입력 적용 |
| FOV Kick | 구현 | 비활성 |
| Spring Arm | 구현 | Scene 미배치 |
| Fade In/Out | 구현 | 적용 |
| Letter Box | 구현 | 적용 |
| Vignette | 구현 | 적용 |
| Gamma Correction | 구현 | 최종 게임에서 비활성 |
| Hit Stop | 구현 | Script 호출 비활성 |
| Slomo | 구현 | 적용 |
| Hit Squash·Knockback | 구현 | 적용 |
| Camera Sequence Editor | 구현 | Sequence Shake 제작·테스트 |
| 팀 번호·구성원 표시 | 구현 | Scene Decal로 적용 |
| Release Build Batch | 구현 | 의존성 복사 보완 필요 |

### 제한 사항

- **Camera Timeline 이중 갱신**: `UGameEngine::Tick`은 World Tick 전후에 `UpdateCamera`를 각각 호출하고 두 호출 모두 전체 Unscaled Delta를 Manager에 전달합니다. 따라서 Blend, Shake와 화면 효과 Timeline이 의도한 시간보다 빠르게 진행될 수 있습니다.
- **Modifier 적용 순서**: Modifier를 Priority 오름차순으로 정렬한 뒤 역순으로 순회하므로 숫자가 큰 Priority가 먼저 적용됩니다. 현재 Shake와 FOV Kick은 대부분 가산 방식이라 영향이 작지만, 덮어쓰기형 Modifier를 추가할 때 우선순위 규약을 정리해야 합니다.
- **Spring Arm 미적용**: Component와 Editor Property는 구현되어 있지만 최종 Scene에는 배치되지 않았습니다.
- **Line Trace 기반 충돌**: Spring Arm의 `CollisionProbeRadius`는 실제 Sphere Sweep에 사용되지 않습니다. 가는 장애물이나 Camera 부피를 고려한 충돌 정확도에는 한계가 있습니다.
- **FOV Kick 비활성**: Engine과 Lua API는 구현되어 있지만 최종 Gameplay 호출은 주석 처리되어 있습니다.
- **Gamma Correction 비활성**: Shader와 API는 구현되어 있지만 최종 게임의 실행 Script에서는 사용하지 않습니다.
- **Hit Stop 비활성**: Engine과 Lua Binding은 구현되어 있지만 `HitReaction.lua` 호출은 주석 처리되어 Slomo만 사용합니다.
- **Sequence 용도 제한**: Camera Sequence는 독립 Cutscene Timeline이 아니라 Camera Shake Offset 데이터로만 재생됩니다.
- **Camera Shake Blend 제한**: 별도의 Blend In/Out Curve가 없고 전체 수명 동안 선형 감쇠 Weight를 사용합니다.
- **Projection 전환 제한**: Perspective와 Orthographic 사이를 연속적으로 보간하지 않고 View Target Blend가 끝날 때 Projection Type을 변경합니다.
- **화면 전체 후처리**: Screen Effects Pass가 UI Pass 뒤에 실행되므로 Fade, Letter Box, Vignette와 Gamma Correction이 3D Scene뿐 아니라 게임 UI에도 함께 적용됩니다.
- **사용하지 않는 Intro 샘플**: `IntroDirector.lua`의 임시 Team 문구와 `CineCamera_Intro` Tag는 최종 `DriftSalvageHud.lua`·`IntroCamera` 실행 경로와 일치하지 않습니다. 최종 팀 정보는 Scene Decal이 담당합니다.
- **Release Runtime DLL 누락**: Visual Studio 출력 폴더에는 Post-Build Event로 `fmod.dll`이 복사되지만 `ReleaseBuild.bat`은 실행 파일만 패키지 루트로 복사합니다. 다른 환경에서 Standalone Package를 실행하려면 `fmod.dll` 복사가 필요합니다.
- **Release 폴더 누락**: `ReleaseBuild.bat`은 현재 저장소에 없는 `NipsEngine/Settings`를 복사하려 하고, 존재하는 `NipsEngine/Config`는 복사하지 않습니다. 런타임·편집 설정까지 완전한 패키지로 만들려면 복사 목록을 정리해야 합니다.
