# NipsEngine — Week 6 Projection Decal & Post Processing

> **Projection Decal · Scene Depth · Multi-Pass Rendering · Exponential Height Fog · Fireball · Movement Component · FXAA**

크래프톤 정글 GameTechLab 자체 게임엔진 제작 교육의 6주차 결과물입니다. DirectX 11 기반 Forward Renderer에 **Projection Decal**과 Scene Depth 기반 효과를 추가하고, Fog·Fireball·Depth View·FXAA를 여러 Render Pass로 연결했습니다. Component가 렌더 데이터를 만들고, `FRenderCollector`와 `FRenderBus`를 거쳐 Renderer와 Shader에 전달되는 엔진 구조도 함께 확장했습니다.

이 문서는 과제 명세뿐 아니라 현재 저장소의 실제 렌더 경로와 구현 범위를 기준으로 작성했습니다.

## 학습 목표

- Forward Rendering 환경에서 Projection Decal을 렌더링한다.
- Decal 전용 Vertex/Pixel Shader와 Render Pass를 구성한다.
- Pixel Shader에서 Scene Depth로 화면의 표면 위치를 복원한다.
- Post-process Pass를 연결하며 Multi-Pass Rendering 흐름을 이해한다.
- Exponential Height Fog, Fireball, Movement Component, FXAA를 엔진 구조에 통합한다.
- Decal의 Fade와 정렬, Show 제어 및 통계 기능을 구현한다.

## 구현 요약

| 영역 | 구현 내용 |
| --- | --- |
| Projection Decal | X축 Forward Box Volume, Scene Depth 기반 위치 복원, 전용 Shader와 Pass |
| Decal 제어 | Material, Sort Order, 수동 Fade Amount, 원거리·근거리 Distance Fade |
| Scene Depth | 공유 Render Target의 Sub-Viewport UV 보정, Depth View Mode |
| Multi-Pass | Opaque → Decal → Translucent와 Fog·Fireball·Outline·FXAA 연결 |
| Fog | 높이에 따른 지수 밀도와 Basic Inscattering을 계산하는 Full-screen Pass |
| Fireball | Scene 표면 중 구체 내부 Pixel에 색을 가산하는 Screen-space Volume Effect |
| Movement | Root Component 대상 Rotation/Projectile Movement와 회전 복제 예제 |
| FXAA | Viewport-local Edge 탐색, 최대 10단계 Span Search, Global/Local 품질 설정 |
| 에디터 | Actor·Component 생성, Decal 통계, View Mode, FXAA 설정, Volume Debug |

## 전체 렌더 흐름

Editor Renderer는 네 개의 Viewport를 순서대로 같은 Host Render Target에 렌더링합니다. 각 Viewport마다 World를 다시 수집하고 Sub-Viewport 위치와 크기를 GPU Constant로 전달합니다.

```text
World / Primitive Components
  │
  └─ FRenderCollector
      ├─ Opaque / Decal / Translucent Command
      ├─ Fireball Command
      ├─ Fog Constant와 Command — Fog View Mode에서만
      └─ Decal / Frustum Culling 통계
          │
          └─ FRenderBus
              │
              ├─ Scene Pass
              │   Opaque → Decal → Translucent
              │
              ├─ Post-process
              │   Fog → Fireball → Grid
              │       → Selection Mask → Outline → FXAA/Copy
              │
              └─ Editor Overlay
                  Editor Line → Font → SubUV/Billboard → Gizmo
```

`DepthScene` View Mode에서는 일반 Post-process 분기 대신 Depth Visualizer를 먼저 실행하고 Selection Mask·Outline을 처리한 뒤, FXAA의 비활성 Copy 경로로 최종 Target에 전달합니다. 이 분기에서는 Fireball Pass를 실행하지 않습니다.

관련 코드:

- `NipsEngine/Source/Editor/EditorRenderPipeline.cpp`
- `NipsEngine/Source/Engine/Render/Scene/RenderCollector.cpp`
- `NipsEngine/Source/Engine/Render/Scene/RenderBus.cpp`
- `NipsEngine/Source/Engine/Render/Renderer/Renderer.cpp`

## 1. Projection Decal

### Component와 Actor

`UDecalComponent`는 Unit Cube를 투영 Volume으로 사용하는 Primitive Component입니다. World AABB와 AABB Raycast를 제공하므로 에디터에서 선택할 수 있으며 다음 속성을 편집할 수 있습니다.

| 속성 | 역할 |
| --- | --- |
| Material | 투영할 Diffuse/Alpha Texture |
| Sort Order | 겹치는 Decal의 렌더 순서 |
| Fade Amount | 최종 Alpha에 곱하는 수동 값 |
| Distance Fade | Camera 거리 기반 Fade 활성화 |
| Fade Start/End Distance | Fade 구간의 양 끝 |
| Transform Scale | Projection Box 크기와 깊이 |

`ADecalActor`는 Decal Component를 Root로 만들고 기본 `DicePaper` Material을 연결합니다. 기존 Actor에는 `Add Component` 메뉴를 통해 Decal Component를 추가할 수 있습니다.

### X축 Projection과 위치 복원

Decal의 Local X축은 Projection 깊이 방향이고, Local YZ 평면을 Texture UV로 사용합니다. Shader는 Unit Cube를 그리면서 Opaque Pass가 기록한 Scene Depth를 읽어 현재 화면에 보이는 표면 위치를 Decal Local Space로 복원합니다.

```text
Decal Unit Cube
  → Model × View × Projection
  → InverseClipToLocal을 Constant Buffer에 저장
  → Pixel의 NDC XY + Scene Depth로 Clip Position 구성
  → Decal Local Position 복원
  → [-0.5, 0.5] Box 내부 여부 검사
  → UV = Local YZ
  → Texture Alpha × Fade Alpha로 합성
```

배경 Depth가 1이거나 복원된 위치가 Box 밖이면 Pixel을 버립니다. 최종 Alpha가 0.05보다 작아도 Clip해 불필요한 Blend를 줄입니다.

Decal Pass는 Front Face를 Cull해 Camera가 Volume 안에 들어가도 뒷면이 Rasterize되도록 구성합니다. Depth Stencil View는 바인딩하지 않으며, Geometry Depth Test 대신 Pixel Shader가 Scene Depth SRV를 직접 조회합니다. 따라서 Decal은 Depth Buffer를 수정하지 않고 Scene Color에 Alpha Blend됩니다.

### 정렬과 Fade

Decal Command는 `Sort Order`가 작은 값부터 렌더링됩니다. 값이 같으면 현재 구현은 Camera 거리가 아니라 **World Origin에서 먼 Decal을 먼저** 배치합니다.

Distance Fade 식은 다음과 같습니다.

```text
DistanceFade = 1 - saturate((CameraDistance - Start) / (End - Start))
FinalAlpha   = FadeAmount × DistanceFade
```

`Start < End`이면 먼 거리에서 사라지는 일반적인 Fade가 되고, `Start > End`이면 Camera에 가까워질수록 사라지는 Near Fade로 동작합니다. `AFakeSpotlightActor`는 이 역방향 구간을 사용합니다. Start와 End가 사실상 같으면 거리 Fade 계산을 건너뜁니다.

과제의 Fade In/Out 항목 중 현재 구현된 범위는 **수동 Fade Amount와 Camera Distance Fade**입니다. 시간에 따라 자동 진행되는 Fade 애니메이션이나 Lifetime 기반 Fade는 구현되어 있지 않습니다.

### Show 제어와 통계

Viewport의 `Decal` Show Flag가 꺼져 있으면 Decal Command를 만들지 않습니다. Frustum Culling 경로에서는 다음 값을 Viewport별로 기록합니다.

- `TotalDecalCount`: 보이는 Actor가 소유한 전체 Decal 수
- `TotalVisibleDecalCount`: Show Flag와 Frustum 판정을 통과해 제출된 Decal 수

선택한 Decal은 Projection Volume을 확인할 수 있도록 Editor Pass에 초록색 Box를 그립니다. 과제에서 언급한 Decal 소요 시간 통계는 구현되어 있지 않고 개수 통계만 제공합니다.

관련 코드:

- `NipsEngine/Source/Engine/Component/DecalComponent.h`
- `NipsEngine/Source/Engine/Component/DecalComponent.cpp`
- `NipsEngine/Source/Engine/GameFramework/PrimitiveActors.cpp`
- `NipsEngine/Source/Engine/Render/Scene/RenderCollector.cpp`
- `NipsEngine/Source/Engine/Render/Renderer/Renderer.cpp`
- `NipsEngine/Shaders/Decal.hlsl`

## 2. Scene Depth와 Multi-Pass Rendering

### Sub-Viewport 좌표 보정

네 Editor Viewport는 하나의 Host Color/Depth Texture 일부를 공유합니다. Full-screen Triangle이나 Volume Shader의 UV는 현재 Viewport 기준 0~1이므로, Renderer가 다음 값을 `SceneDepthBuffer`에 기록합니다.

| 값 | 역할 |
| --- | --- |
| `ViewportUVOffset` | Host Texture 안에서 현재 Viewport가 시작하는 UV |
| `ViewportUVScale` | Host Texture에서 현재 Viewport가 차지하는 UV 크기 |
| `DepthTextureSize` | 정수 Pixel 좌표 계산과 Clamp에 사용하는 전체 Texture 크기 |

```text
Viewport Local UV
  → ViewportUVOffset + LocalUV × ViewportUVScale
  → Host Depth Texture UV
```

Decal, Fog, Fireball, Depth Visualizer가 같은 보정을 사용하므로 인접 Viewport의 Depth를 잘못 읽지 않습니다.

### DepthScene View Mode

`DepthScene.hlsl`은 `SV_VertexID`로 만든 Full-screen Triangle에서 Depth Texture를 직접 읽습니다.

- 배경 Depth 1.0은 검은색으로 출력합니다.
- Orthographic Projection은 Hardware Depth를 그대로 회색조로 표시합니다.
- Perspective Projection은 Depth를 선형화한 뒤 고정 최대 거리 300으로 나누어 표시합니다.
- DepthScene에서는 FXAA를 적용하지 않고 Copy 경로로 최종 Post-process Target에 전달합니다.

### Pass별 Scene Depth 사용

| Pass | 형태 | Scene Depth 사용 |
| --- | --- | --- |
| Decal | Unit Cube Volume | 표면 위치 복원과 Box 내부 판정 |
| Fog | Full-screen Triangle | 표면까지 거리와 World Position 복원 |
| Fireball | Unit Cube Volume | 효과 구체 내부에 있는 Scene 표면 판정 |
| DepthScene | Full-screen Triangle | Depth 자체를 회색조로 시각화 |
| FXAA | Full-screen Triangle | Depth가 아니라 Scene Color 사용 |

Decal과 Fireball은 화면 전체를 처리하는 일반적인 Post-process와 달리 Unit Cube의 Screen Projection 범위만 Rasterize하는 Volume Pass입니다.

관련 코드:

- `NipsEngine/Source/Engine/Render/Scene/RenderCommand.h`
- `NipsEngine/Source/Engine/Render/Renderer/Renderer.cpp`
- `NipsEngine/Shaders/DepthScene.hlsl`
- `NipsEngine/Shaders/Decal.hlsl`
- `NipsEngine/Shaders/Fog.hlsl`
- `NipsEngine/Shaders/FireBall.hlsl`

## 3. Exponential Height Fog

`UHeightFogComponent`는 다음 Fog Parameter를 제공합니다.

| 속성 | 역할 |
| --- | --- |
| Inscattering Color | Scene Color 위에 합성할 Fog 색 |
| Fog Density | 기준 높이에서의 밀도 |
| Height Falloff | 높이에 따른 지수 감쇠 |
| Start Distance | Camera 근처의 Fog 제외 거리 |
| Cutoff Distance | Fog 적분 최대 거리 |
| Max Opacity | 계산된 Fog Alpha 상한 |
| Component Z | Fog 기준 높이 |

Fog Pixel Shader는 Depth로 World Position과 Camera Ray를 복원합니다. 수평에 가까운 Ray는 일정 밀도로 적분하고, 그 외의 Ray는 높이에 따른 지수 함수의 해석적 적분을 사용합니다.

```text
Scene Depth
  → World Position
  → Camera Ray와 실제 표면 거리
  → Start/Cutoff 구간 적용
  → Exponential Height Density 적분
  → 1 - exp(-Integral)
  → Max Opacity 제한
  → Inscattering Color Alpha Blend
```

배경 Pixel은 실제 표면 거리가 없으므로 Cutoff Distance가 있으면 그 값을 사용하고, 없으면 1,000,000의 대체 거리를 사용합니다.

`AExponentialHeightFog`는 Height Fog Component를 Root로 갖습니다. 여러 Fog Actor가 있어도 Collector는 처음 발견한 활성 Fog 하나를 Render Bus에 기록한 뒤 종료합니다.

현재 활성 Editor 경로에서 Fog는 `Fog` View Mode일 때만 수집됩니다. `FShowFlags::bFog`는 선언되어 있지만 Editor Overlay에 노출되지 않고 Collector 조건에도 사용되지 않습니다.

관련 코드:

- `NipsEngine/Source/Engine/Component/HeightFogComponent.h`
- `NipsEngine/Source/Engine/Component/HeightFogComponent.cpp`
- `NipsEngine/Source/Engine/GameFramework/PrimitiveActors.cpp`
- `NipsEngine/Source/Engine/Render/Scene/RenderCollector.cpp`
- `NipsEngine/Shaders/Fog.hlsl`

## 4. Fireball과 Fake Spotlight

### Fireball

`UFireBallComponent`는 Color, Intensity, Radius, Radius FallOff를 가집니다. Radius에 맞춰 확대한 Unit Cube를 그리되, Pixel Shader는 Scene Depth로 복원한 위치가 실제 구체 안에 있는지 다시 검사합니다.

```text
Fireball Volume Rasterization
  → Scene Depth에서 표면 위치 복원
  → Fireball Local Space 변환
  → Box 범위 검사
  → Sphere 거리 검사
  → 중심 거리 기반 Glow 계산
  → Color × Intensity를 Additive Blend
```

이 효과는 주변 Object에 물리 기반 Light를 추가하는 것이 아닙니다. 이미 Depth에 기록된 표면 중 Fireball 구체 내부에 있는 Pixel을 밝게 만드는 Screen-space Additive Effect입니다. 배경 Pixel은 제외되며 Fog 뒤에 실행되므로 Fireball 색 자체는 Fog 합성의 영향을 받지 않습니다.

`AFireballActor`는 Sphere Static Mesh를 Root로 두고 Fireball Component와 Projectile Movement Component를 조합합니다. 기본값으로 X축 방향 초기 속도 5를 설정합니다. 선택 시 Fireball Volume을 초록색 Box로 표시합니다.

### Fake Spotlight

`AFakeSpotlightActor`는 실제 Lighting 계산 없이 두 Component를 조합합니다.

- Cylindrical Billboard: Spotlight Icon 표현
- Decal Component: `DecalFakeSpotlight` Material을 바닥에 투영

Billboard와 Decal 모두 `Start > End`인 Distance Fade를 사용해 Camera가 지나치게 가까워지면 사라지고 일정 거리 밖에서는 보이도록 구성했습니다.

관련 코드:

- `NipsEngine/Source/Engine/Component/FireBallComponent.cpp`
- `NipsEngine/Source/Engine/GameFramework/FireballActor.cpp`
- `NipsEngine/Source/Engine/GameFramework/PrimitiveActors.cpp`
- `NipsEngine/Shaders/FireBall.hlsl`

## 5. Movement Component

`UMovementComponent`는 Owner Actor가 설정될 때 해당 Actor의 Root Scene Component를 이동 대상으로 연결합니다.

### Rotation Movement

`URotationMovementComponent`는 매 Tick마다 Quaternion으로 회전을 갱신합니다.

- Euler Rotation Rate 또는 Axis-Angle 방식 선택
- Local Space에서는 `Old × Delta`, World Space에서는 `Delta × Old` 순서로 합성
- 0에 가까운 Rotation Axis는 Up Vector로 대체
- Pivot Translation의 회전 전후 차이만큼 위치를 보정해 공전 형태 구현

### Projectile Movement

`UProjectileMovementComponent`는 `BeginPlay`에서 Velocity와 Acceleration을 초기값으로 설정합니다. Tick에서는 Acceleration으로 Velocity를 갱신한 뒤 `Velocity × DeltaTime`만큼 Root Component를 이동합니다.

- Initial Velocity
- Initial Acceleration
- Gravity Enabled
- Gravitational Acceleration

현재 구현은 이동 적분만 담당하며 충돌, Bounce, 수명 종료, 최대 속도 제한은 포함하지 않습니다.

### 회전 복제 예제

`USpawnRandomRotatingCopiesComponent`는 PIE World에서 Owner Actor를 지정 개수만큼 복제하고, 각 복제 Actor에 무작위 Axis-Angle Rotation Movement를 붙입니다.

- Spawn Count와 Radius
- 최소·최대 회전 속도
- 2D/3D Scatter
- 고정 Random Seed
- 원본 Actor 회전 또는 숨김
- 복제 Actor에서 Spawner를 제거해 재귀 복제 방지

World가 PIE가 아니거나 한 세션에서 이미 실행했다면 Spawn을 건너뛰며, 생성 후 Spatial Index를 동기화합니다.

관련 코드:

- `NipsEngine/Source/Engine/Component/MovementComponent.cpp`
- `NipsEngine/Source/Engine/Component/RotationMovementComponent.cpp`
- `NipsEngine/Source/Engine/Component/ProjectileComponent.cpp`
- `NipsEngine/Source/Engine/Component/SpawnRandomRotatingCopiesComponent.cpp`

## 6. FXAA

FXAA는 현재 Viewport의 Scene Color에서 밝기 대비가 큰 Edge를 찾고, Edge에 수직인 방향으로 Sample 위치를 이동해 계단 현상을 줄입니다.

현재 Shader의 처리 순서는 다음과 같습니다.

1. 중심과 상하좌우 Pixel의 Luma 범위를 계산합니다.
2. `Edge Threshold`와 `Edge Threshold Min`보다 대비가 작으면 조기 종료합니다.
3. 대각선을 포함한 주변 Luma로 수평/수직 Edge 방향을 결정합니다.
4. Edge를 따라 양방향으로 최대 10단계 Span Search를 수행합니다.
5. Edge 위치 기반 Offset과 `Subpix` 기반 Offset 중 큰 값을 사용합니다.
6. 보정한 UV에서 Scene Color를 다시 Sample합니다.

### Multi-Viewport 대응

FXAA Constant에는 현재 Sub-Viewport의 역해상도, Host Texture 안의 UV 시작점과 크기가 들어갑니다. 모든 Sample UV를 Viewport Local 0~1 범위로 Clamp한 뒤 Host UV로 변환하므로, Edge 탐색이 Splitter나 옆 Viewport까지 넘어가지 않습니다.

FXAA가 꺼져 있어도 Pass 자체는 실행합니다. 이때 Shader가 현재 Viewport의 Scene Color를 Post-process Render Target으로 그대로 복사합니다. 이후 Editor Overlay가 같은 최종 Target에 그려질 수 있도록 출력 경로를 통일하기 위한 구조입니다.

각 Viewport는 독립적으로 다음 설정을 가집니다.

- FXAA Enabled
- Use Global Params
- Local Subpix
- Local Edge Threshold
- Local Edge Threshold Min

Global 기본값은 Editor Settings에 저장되며 Viewport 메뉴에서 Global 값을 Local로 복사할 수 있습니다. `DepthScene` View Mode에서는 FXAA를 강제로 비활성 Copy 경로로 처리합니다.

관련 코드:

- `NipsEngine/Source/Engine/Render/Common/ViewTypes.h`
- `NipsEngine/Source/Editor/EditorRenderPipeline.cpp`
- `NipsEngine/Source/Editor/UI/EditorMainPanel.cpp`
- `NipsEngine/Source/Engine/Render/Renderer/Renderer.cpp`
- `NipsEngine/Shaders/ShaderFXAA.hlsl`

## 에디터 검증 도구

| 위치 | 기능 |
| --- | --- |
| Actor 생성 | Decal, Fake Spotlight, Fireball, Fog Actor |
| Add Component | Decal, Fireball, Projectile Movement, Rotation Movement, Random Rotating Copies |
| View 메뉴 | Lit, Unlit, Wireframe, DepthScene, Fog |
| FXAA 메뉴 | Viewport별 활성화와 Global/Local Parameter 전환 |
| Viewport Settings | Decal Show Flag, FXAA Global 기본값 |
| Viewport 통계 | 전체 Decal 수와 Frustum 통과 Decal 수 |
| 선택 표시 | Decal과 Fireball Volume Debug Box |

Fog 효과는 Viewport Settings의 별도 Checkbox가 아니라 View 메뉴의 `Fog` Mode로 확인합니다.

## 기능 확인용 Scene

| Scene | 실제 구성 | 확인 항목 |
| --- | --- | --- |
| `DecalBlood.Scene` | Decal 5, Static Mesh 중심 장면 | 벽·바닥 Projection, Material, Sort Order |
| `Dices with Fog.Scene` | Fog 1, Fake Spotlight 1, Static Mesh 14 | Height Fog와 Billboard+Decal 조합 |
| `FakeLight.Scene` | Fog 1, Fake Spotlight 1, Static Mesh 6 | Near Fade와 Fake Spotlight |
| `DemoScene.Scene` | Fireball 1, Fog 1, Static Mesh 6 | Fireball Volume, Radius, Intensity |
| `SuperDice.Scene` | Fireball 1, Fog 1, Static Mesh 1 | 단순 장면에서 Fireball 범위 확인 |
| `dice scene depth.Scene` | Static Mesh 108, Fog 1 | DepthScene과 대량 Geometry Depth 확인 |

권장 확인 순서:

1. `DecalBlood.Scene`에서 Decal Show Flag와 Sort Order를 확인합니다.
2. Decal의 Fade Amount와 Distance Fade 구간을 바꿔 Alpha 변화를 확인합니다.
3. `DepthScene`으로 전환해 Opaque Geometry가 기록한 Depth를 확인합니다.
4. `Dices with Fog.Scene`에서 `Fog` View Mode를 선택하고 Height/Density/Falloff를 조절합니다.
5. `DemoScene.Scene`에서 Fireball Radius·Intensity·Falloff를 비교합니다.
6. FXAA를 켜고 Global/Local Parameter를 변경해 Edge와 Sub-pixel 보정 차이를 확인합니다.

## 빌드 및 실행

### 요구 환경

- Windows 10/11
- Visual Studio 2022
- MSVC v143 C++ Toolset
- Windows 10 SDK
- Direct3D 11 지원 GPU와 Driver
- NuGet Package Restore 사용 가능 환경

프로젝트는 x64 구성에서 C++20을 사용하며, `directxtk_desktop_win10` 2025.10.28.2 Package를 참조합니다.

### 프로젝트 파일 생성과 빌드

1. 저장소 루트의 `GenerateProjectFiles.bat`을 실행합니다.
2. 생성된 `NipsEngine.sln`을 Visual Studio 2022에서 엽니다.
3. NuGet Package Restore로 DirectXTK가 준비되었는지 확인합니다.
4. `Debug | x64` 또는 `Release | x64`로 빌드합니다.
5. 실행 시 Working Directory가 `NipsEngine` 폴더인지 확인합니다.

`ReleaseBuild.bat`은 `Release | x64`를 빌드한 뒤 실행 파일과 `Shaders`, `Asset`, `Settings`, `Saves`를 `ReleaseBuild` 폴더에 패키징합니다.

## 구현 범위

- Decal은 Local X축 Projection, Diffuse/Alpha Texture, Sort Order, Fade Amount와 Camera Distance Fade를 지원합니다.
- Scene Depth를 이용하는 효과는 현재 Viewport의 UV Offset과 Scale을 적용합니다.
- Fog는 한 개의 활성 Height Fog Component를 사용하고 Full-screen Alpha Blend로 합성합니다.
- Fireball은 Unit Cube Volume 안의 Scene 표면을 밝히는 Additive Effect입니다.
- Rotation과 Projectile Movement는 Actor Root Component를 갱신합니다.
- FXAA는 Viewport별 활성화와 Global/Local 품질 Parameter를 지원합니다.

### 제한 사항

- Decal Fade는 수동 값과 Camera Distance 기반이며, 시간·Lifetime 기반 Fade In/Out은 없습니다.
- Decal은 Diffuse/Alpha Texture만 사용하며 Normal·Roughness·Lighting을 수정하는 G-buffer Decal은 아닙니다.
- 같은 Sort Order의 Decal은 Camera가 아닌 World Origin과의 거리로 정렬됩니다.
- Decal과 Fireball Pass는 DSV를 바인딩하지 않고 Scene Depth SRV를 기준으로 판정하므로 Depth Buffer를 갱신하지 않습니다.
- Decal Stat은 전체·제출 개수만 제공하며 CPU/GPU Pass 시간은 측정하지 않습니다.
- Fog는 `Fog` View Mode에서만 수집되며 선언된 `bFog` Show Flag는 활성 Editor 경로에 연결되어 있지 않습니다.
- 여러 Fog가 있어도 수집 순서상 첫 번째 활성 Fog만 적용합니다.
- DepthScene의 Perspective 표시 범위는 300으로 고정되어 있어 Camera Far Clip이나 Scene 규모에 맞춰 자동 조정되지 않습니다.
- Fireball은 실제 Light가 아니며 Depth가 없는 배경에는 적용되지 않고, Fog Pass 뒤에 그려져 Fireball 색 자체는 Fog의 영향을 받지 않습니다.
- Fireball Shader는 `RadiusFallOff`를 원시 거리 비교와 정규화 거리 계산에 혼용해 일부 값에서 Glow가 1보다 커질 수 있습니다.
- Projectile의 Gravity Enabled 값은 명시적 기본 초기값이 없고, Gravity를 Velocity가 아니라 Acceleration에 매 Tick 누적하므로 일정 중력 가속도 모델과 다르게 동작합니다.
- Projectile Movement에는 Collision·Bounce·Lifetime 처리가 없습니다.
- Viewport별 FXAA Enabled와 Local Parameter는 Editor Settings 파일에 저장되지 않아 실행을 다시 시작하면 기본 상태로 돌아갑니다.
