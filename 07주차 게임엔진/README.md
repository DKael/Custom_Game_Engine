# NipsEngine — Week 7 Forward Lighting, Uber Shader & 2.5D Light Culling

> **Forward Shading · Ambient/Directional/Point/Spot Light · Gouraud/Lambert/Blinn-Phong · Normal Mapping · Forward+ 2.5D**

크래프톤 정글 GameTechLab 자체 게임엔진 제작 교육의 7주차 결과물입니다. DirectX 11 기반 Forward Renderer에 네 종류의 Light Component와 조명 모델을 추가하고, 화면 타일별로 영향을 주는 Point·Spot Light만 선별하는 **Forward+ 2.5D Tile Light Culling**을 구현했습니다.

이 문서는 과제 명세뿐 아니라 현재 저장소의 실제 렌더 경로, Shader 분기, GPU Resource 한도를 기준으로 작성했습니다.

## 학습 목표

- Forward Shading에서 Per-Vertex Lighting과 Per-Pixel Lighting의 차이를 구현으로 확인한다.
- Ambient, Directional, Point, Spot Light의 위치·방향·감쇠 특성을 이해한다.
- 하나의 Uber Shader에서 View Mode, Material, Normal Map, Light Culling 조합을 처리한다.
- Tile-based Light Culling으로 Pixel Shader가 순회하는 지역 광원 수를 줄인다.
- 화면상의 2D Tile에 깊이 구간을 더한 2.5D Culling의 필요성과 구조를 이해한다.

## 구현 요약

| 영역 | 구현 내용 |
| --- | --- |
| Light Component | Ambient, Directional, Point, Spot Light와 전용 Actor |
| 조명 모델 | Per-Vertex Gouraud, Per-Pixel Lambert, Per-Pixel Blinn-Phong |
| Uber Shader | View Mode, Normal Map, Static Mesh/Decal, Culling 여부를 Shader Key로 조합 |
| Material | Diffuse·Specular Texture와 Material Color, Shininess, Tangent-space Normal Map |
| Light Culling | 16×16 Tile, Depth Prepass, Tile Frustum, 32-bit Depth Slice Mask |
| GPU 자료구조 | Point/Spot별 Tile Grid와 Light Index Structured Buffer |
| 광원 예산 | Scene에서 Point·Spot을 영향도순으로 각각 최대 256개 업로드 |
| 검증 도구 | Light Debug Shape, Culling On/Off, Light Hitmap, World Normal View |

## 전체 렌더 흐름

`FRenderCollector`는 활성화된 Light Component를 종류별 GPU 데이터로 변환합니다. Renderer는 Point·Spot Light의 영향도를 계산해 업로드 대상을 먼저 제한하고, Depth Prepass와 Compute Shader로 현재 Viewport의 Tile Light List를 생성합니다.

```text
Light Actor / Component
  │
  └─ RenderCollector
      ├─ Ambient Light Constant
      ├─ Directional Light Array
      ├─ Point Light Array
      └─ Spot Light Array
          │
          └─ UpdateLightingBuffer
              ├─ Point·Spot 영향도 정렬
              └─ 종류별 최대 256개 GPU 업로드
                  │
                  ├─ Depth Prepass
                  │   └─ Opaque Static Mesh Depth 기록
                  │
                  ├─ TileLightCulling25D Compute Shader
                  │   ├─ Tile별 깊이 범위와 점유 Mask 생성
                  │   ├─ Point Light Sphere 판정
                  │   └─ Spot Light Sphere Broad Phase + Cone 판정
                  │
                  └─ Opaque / Decal Pass
                      └─ UberLit에서 Tile에 포함된 지역 광원만 계산
```

Light Culling을 끄면 Compute Dispatch와 Tile List 조회는 생략하지만, Scene 단위의 Point·Spot 최대 256개 선별과 Depth Prepass는 그대로 수행합니다.

관련 코드:

- `NipsEngine/Source/Engine/Render/Scene/RenderCollector.cpp`
- `NipsEngine/Source/Engine/Render/Renderer/Renderer.cpp`
- `NipsEngine/Shaders/TileLightCulling25D.hlsl`
- `NipsEngine/Shaders/UberLit.hlsl`

## 1. Light Component와 Actor

`ULightComponentBase`는 공통 속성인 Color와 Intensity를 관리하고, 각 파생 Component가 광원 종류별 데이터를 추가합니다.

| Light | 주요 데이터 | Shader 동작 |
| --- | --- | --- |
| Ambient | Color, Intensity | 모든 표면에 Material Ambient Color와 함께 적용 |
| Directional | Direction, Color, Intensity | 거리 감쇠 없이 동일한 방향의 직사광 계산 |
| Point | Position, Radius, Color, Intensity | Radius 내부에서 거리 감쇠와 방향별 Lambert/Specular 계산 |
| Spot | Position, Direction, Radius, Inner/Outer Cone, Color, Intensity | 거리 감쇠와 원뿔 각도 감쇠를 함께 계산 |

Point Light의 거리 감쇠는 `(1 - distance / radius)²` 형태입니다. Spot Light는 같은 거리 감쇠에 Inner/Outer Cone 사이의 `smoothstep` 결과를 제곱해 각도 감쇠를 추가합니다.

수집 단계에서는 비활성 Light를 제외합니다. Ambient는 단일 Constant에 기록되고, Directional·Point·Spot은 배열에 추가됩니다. 따라서 Ambient Light가 여러 개이면 수집 순서상 마지막 값이 앞의 값을 대체하며 합산되지 않습니다.

### Actor와 에디터 표현

에디터의 Actor 생성 메뉴에서는 네 종류의 전용 Light Actor를 생성할 수 있습니다.

| Actor | 기본 구성 | Debug 표현 |
| --- | --- | --- |
| `AAmbientLightActor` | Scene Root + Ambient Light | 전용 Billboard와 Shape 없음 |
| `ADirectionalLightActor` | Scene Root + Directional Light + Billboard | Cyan 방향 화살표 |
| `APointLightActor` | Scene Root + Point Light + Billboard | Yellow Radius Sphere |
| `ASpotLightActor` | Scene Root + Spot Light + Billboard | Green Outer Cone, Yellow Inner Cone |

Directional·Point·Spot Actor의 Billboard는 Light 전용 Sprite를 사용하고, Color Property가 바뀌면 같은 색으로 Tint됩니다. Debug Shape의 색은 광원색이 아니라 종류를 구분하기 위한 고정색입니다.

`Add Component` 목록에는 Directional·Point·Spot Light Component가 등록되어 있습니다. Ambient Light는 전용 Actor로는 만들 수 있지만 일반 Actor에 추가하는 Component 목록에는 등록되어 있지 않습니다.

관련 코드:

- `NipsEngine/Source/Engine/Component/Light/LightComponentBase.cpp`
- `NipsEngine/Source/Engine/Component/Light/AmbientLightComponent.cpp`
- `NipsEngine/Source/Engine/Component/Light/DirectionalLightComponent.cpp`
- `NipsEngine/Source/Engine/Component/Light/PointLightComponent.cpp`
- `NipsEngine/Source/Engine/Component/Light/SpotLightComponent.cpp`
- `NipsEngine/Source/Engine/GameFramework/AmbientLightActor.cpp`
- `NipsEngine/Source/Engine/GameFramework/DirectionalLightActor.cpp`
- `NipsEngine/Source/Engine/GameFramework/PointLightActor.cpp`
- `NipsEngine/Source/Engine/GameFramework/SpotLightActor.cpp`
- `NipsEngine/Source/Editor/UI/EditorControlWidget.cpp`
- `NipsEngine/Source/Editor/UI/EditorPropertyWidget.cpp`

## 2. Uber Shader와 조명 모델

`UberLit.hlsl`은 별도의 Lighting Shader를 여러 개 두지 않고 Compile Macro로 기능 조합을 만듭니다. `FShaderManager`는 시작 시 다음 네 조건의 조합을 순회해 Shader Variant를 준비합니다.

| Shader Key 조건 | 값 |
| --- | --- |
| View Mode | 9개 Enum 값 |
| Normal Map | 사용 / 미사용 |
| Opaque Type | Static Mesh / Decal |
| Light Culling | 사용 / 미사용 |

현재 조합 수는 **9 × 2 × 2 × 2 = 72개**입니다. 런타임에는 Render Command의 Material과 현재 Viewport 설정으로 `FShaderKey`를 만들고 해당 Variant를 선택합니다.

### View Mode별 실제 계산

| View Mode | 계산 위치 | 실제 동작 |
| --- | --- | --- |
| `Unlit` | 조명 계산 없음 | Diffuse Texture 또는 Material Diffuse Color 출력 |
| `Lit_Gouraud` | Vertex Shader | 정점마다 Blinn-Phong을 계산한 뒤 Diffuse·Specular를 보간 |
| `Lit_Lambert` | Pixel Shader | 픽셀마다 Diffuse 조명 계산 |
| `Lit_Phong` | Pixel Shader | Half Vector를 사용하는 Blinn-Phong Diffuse·Specular 계산 |
| `WorldNormal` | Pixel Shader | 최종 World Normal을 0~1 색 범위로 변환해 출력 |

`Lit_Phong`이라는 View Mode 이름을 사용하지만, Shader 식은 Reflection Vector 기반 Phong이 아니라 `H = normalize(L + V)`를 사용하는 Blinn-Phong입니다.

Gouraud는 정점에서 조명 계산을 끝내므로 정점 수가 적은 Mesh에서는 작은 Specular Highlight가 약해지거나 사라질 수 있습니다. Lambert와 Blinn-Phong은 보간된 World Position·Normal을 바탕으로 픽셀마다 계산해 더 세밀한 결과를 만듭니다.

에디터 View 메뉴에는 `Unlit`, `Wireframe`, `DepthScene`, `Fog`, 세 Lighting Mode, `WorldNormal`이 표시됩니다. Enum의 기본 `Lit` 값은 메뉴 순회가 `Unlit`부터 시작해 직접 선택할 수 없습니다.

관련 코드:

- `NipsEngine/Shaders/UberLit.hlsl`
- `NipsEngine/Source/Engine/Render/Resource/ShaderManager.h`
- `NipsEngine/Source/Engine/Render/Resource/ShaderManager.cpp`
- `NipsEngine/Source/Engine/Render/Common/ViewTypes.h`
- `NipsEngine/Source/Editor/UI/EditorMainPanel.cpp`

## 3. Material과 Normal Mapping

Material Loader는 MTL의 다음 항목을 읽습니다.

| MTL 항목 | Material 데이터 | Shader 사용 |
| --- | --- | --- |
| `Ka` | Ambient Color | Ambient Light 계산 |
| `Kd`, `map_Kd` | Diffuse Color/Texture | Unlit 및 Diffuse Lighting의 표면색 |
| `Ks`, `map_Ks` | Specular Color/Texture | Gouraud·Blinn-Phong Specular 색 |
| `Ns` | Shininess | Blinn-Phong 지수 |
| `map_Bump`, `bump` | Bump Texture | Tangent-space Normal Map으로 해석 |
| `map_Ka` | Ambient Texture | Resource 로드·SRV 바인딩까지만 수행 |

Static Mesh에 Bump Texture가 있으면 Render Collector가 이를 Normal Map Variant로 연결합니다. Pixel Shader는 보간된 Tangent를 Normal에 다시 직교화하고 Bitangent를 만든 뒤 TBN Basis로 Texture Normal을 World Space로 변환합니다.

```text
Texture RGB
  → [-1, 1] 범위의 Tangent-space Normal
  → Tangent 재직교화
  → Bitangent = cross(Normal, Tangent)
  → TBN 변환
  → Lambert / Blinn-Phong / WorldNormal에 사용
```

현재 `map_Bump` Texture를 높이 맵에서 Normal Map으로 변환하지 않고 RGB Normal로 직접 해석합니다. 또한 `AmbientMap`은 `t7`에 바인딩되지만 `UberLit.hlsl`에서 샘플링하지 않으므로 실제 Ambient 결과에는 `Ka`와 Diffuse Texture가 사용됩니다.

Normal Map은 Static Mesh의 `Lit_Lambert`, `Lit_Phong`, `WorldNormal`에 반영됩니다. Gouraud Lighting은 Vertex Shader에서 먼저 계산되기 때문에 Pixel Shader에서 읽는 Normal Map이 조명 결과에는 반영되지 않습니다.

관련 코드:

- `NipsEngine/Source/Engine/Render/Resource/Material.cpp`
- `NipsEngine/Source/Engine/Core/ResourceManager.cpp`
- `NipsEngine/Source/Engine/Render/Scene/RenderCollector.cpp`
- `NipsEngine/Shaders/UberLit.hlsl`

## 4. Forward+ 2.5D Tile Light Culling

Forward Rendering에서 모든 픽셀이 모든 Point·Spot Light를 순회하면 광원 수에 비례해 Pixel Shader 비용이 증가합니다. 이 프로젝트는 화면을 16×16 Pixel Tile로 나누고, 현재 Tile의 표면에 영향을 줄 가능성이 있는 지역 광원만 목록에 남깁니다.

### Depth Prepass와 Tile 깊이 정보

Renderer는 Opaque Pass 전에 Static Mesh 전용 VS-only Depth Prepass를 실행합니다. Compute Shader의 한 Thread Group은 16×16 Thread로 구성되어 Tile 한 개의 Pixel을 병렬로 처리합니다.

각 Tile에서는 다음 깊이 정보를 만듭니다.

- Depth가 1인 배경 Pixel을 제외한 View-space 최소·최대 깊이
- 최소~최대 깊이를 선형으로 32등분한 점유 Bit Mask
- Tile 네 모서리의 View Ray로 구성한 네 개의 Side Plane

단순한 2D Tile Culling은 화면상 같은 Tile에 투영되기만 하면 깊이가 크게 다른 광원도 후보로 남깁니다. 2.5D 경로는 Tile 안에 실제 표면이 존재하는 깊이 Slice만 Bit로 표시하고, Light가 차지하는 깊이 Mask와 겹치지 않으면 후보에서 제외합니다.

### Point Light 판정

Point Light는 Position과 Radius로 만든 Sphere를 사용합니다.

1. Sphere가 Tile Side Plane 바깥인지 확인합니다.
2. Sphere의 깊이 구간이 Tile 최소·최대 깊이와 겹치는지 확인합니다.
3. 2.5D Light Depth Mask와 Tile Occupancy Mask가 겹치는지 확인합니다.

### Spot Light 판정

Spot Light는 유한 Cone을 감싸는 Sphere로 Broad Phase를 수행한 뒤, Cone의 축·높이·밑면 반경을 이용해 Tile Frustum Plane과 Near/Far Plane에 대한 Cone Support Test를 추가로 수행합니다. 마지막으로 Cone의 깊이 범위와 Tile Occupancy Mask를 비교합니다.

Sphere만으로 판정하는 것보다 계산은 늘어나지만, 넓은 Broad Phase Sphere 때문에 실제 Cone 바깥의 Tile가 과도하게 남는 것을 줄입니다.

### Compute Shader 출력

Point와 Spot은 별도의 Grid와 Index Buffer를 사용합니다.

| Resource | 내용 |
| --- | --- |
| `TilePointLightGrid` | Tile별 Point Index 시작 위치와 개수 |
| `TilePointLightIndices` | Tile에 포함된 Point Light Index |
| `TileSpotLightGrid` | Tile별 Spot Index 시작 위치와 개수 |
| `TileSpotLightIndices` | Tile에 포함된 Spot Light Index |

한 Tile은 Point와 Spot을 각각 최대 256개 저장합니다. Viewport 크기가 바뀌면 Renderer가 Tile 수에 맞춰 Buffer를 다시 만들며, 활성 Sub-Viewport의 위치와 크기를 `ForwardPlusConstants`로 전달합니다.

Pixel Shader는 `SV_POSITION`에서 Viewport 시작 좌표를 빼 Tile 좌표를 구하고, Grid의 `{offset, count}`를 따라 해당 Tile의 Light만 순회합니다. Ambient와 Directional은 화면 전체에 영향을 주므로 Tile List를 거치지 않습니다.

관련 코드:

- `NipsEngine/Shaders/DepthPrepass.hlsl`
- `NipsEngine/Shaders/TileLightCulling25D.hlsl`
- `NipsEngine/Shaders/UberLit.hlsl`
- `NipsEngine/Source/Engine/Render/Renderer/Renderer.h`
- `NipsEngine/Source/Engine/Render/Renderer/Renderer.cpp`

## 5. Scene Light 선택과 GPU Binding

Tile Culling 전에 Scene 전체에서 GPU에 올릴 Point·Spot Light를 각각 최대 256개로 제한합니다. 이는 Light Culling이 켜졌는지와 관계없이 적용됩니다.

Point Light는 Intensity와 Radius가 크고 Camera에서 Light Volume까지의 거리가 가까울수록 높은 점수를 받습니다. Spot Light는 Cone Broad Phase Sphere에 같은 거리·영향도 기준을 적용하고, Light 방향이 Camera를 향하는 정도를 가중치로 더합니다. `std::partial_sort`로 점수가 높은 Light만 업로드합니다.

| Light | Scene 업로드 한도 | Tile당 한도 | Tile Culling |
| --- | ---: | ---: | --- |
| Ambient | 1개 Constant | 해당 없음 | 항상 계산 |
| Directional | Buffer 용량 64개 | 해당 없음 | 항상 계산 |
| Point | 256개 | 256개 | Sphere + 2.5D Mask |
| Spot | 256개 | 256개 | Sphere/Cone + 2.5D Mask |

`UberLit`의 주요 Resource Slot은 다음과 같습니다.

| Slot | Resource |
| ---: | --- |
| `t0` | Point Light Structured Buffer |
| `t1` | Spot Light Structured Buffer |
| `t2`, `t3` | Point/Spot Tile Light Index |
| `t4`, `t5` | Point/Spot Tile Grid |
| `t6`~`t9` | Diffuse, Ambient, Specular, Bump Texture |
| `t10` | Directional Light Structured Buffer |
| `b11` | Viewport와 Tile 정보를 담은 Forward+ Constants |
| `b13` | Ambient와 종류별 Light Count |

Light Culling이 꺼진 Lambert·Blinn-Phong Pixel Shader는 Tile List 대신 업로드된 Point·Spot Light 전체를 순회합니다. 따라서 Culling On/Off 비교는 최대 256개로 선별된 동일한 Light Set 안에서 Tile List 사용 효과를 비교하는 구조입니다.

Gouraud는 Vertex Shader에서 업로드된 전체 Light Set을 순회하며 Tile List를 읽지 않습니다. Shader Key에는 Culling On/Off Variant가 존재하지만 Gouraud 조명 계산에는 차이가 없습니다.

관련 코드:

- `NipsEngine/Source/Engine/Render/Renderer/Renderer.cpp`
- `NipsEngine/Source/Engine/Render/Scene/LightInfo.h`
- `NipsEngine/Shaders/UberLit.hlsl`

## 6. 에디터 검증 도구

Viewport Settings의 Light 항목에서 다음 기능을 전환할 수 있습니다.

| 도구 | 확인 항목 |
| --- | --- |
| Directional Light Debug | Light 방향 화살표 |
| Point Light Debug | 감쇠 Radius Sphere |
| Spot Light Debug | Inner/Outer Cone |
| Light Culling Mode | Tile Culling 사용/미사용 경로 비교 |
| Light Hitmap Overlay | Tile별 Point+Spot 후보 수 시각화 |
| Lighting View Mode | Gouraud/Lambert/Blinn-Phong 결과 비교 |
| WorldNormal | Vertex/Normal Map 적용 후 최종 Normal 확인 |

Light Hitmap은 Point·Spot Grid의 `count`를 더해 `log2(count + 1)` 기준 색상 Ramp로 표시합니다. Point 비율은 적색, Spot 비율은 청색 성분으로 보조 표시하고 16×16 Tile 경계도 함께 그립니다. Culling Mode가 꺼져 있으면 Hitmap Pass도 실행하지 않습니다.

관련 코드:

- `NipsEngine/Source/Editor/UI/EditorViewportOverlayWidget.cpp`
- `NipsEngine/Source/Editor/Settings/EditorSettings.cpp`
- `NipsEngine/Shaders/LightHitmapOverlay.hlsl`
- `NipsEngine/Source/Engine/Render/Renderer/Renderer.cpp`

## 기능 확인용 Scene

저장소에는 Lighting Model, Normal Map, Light Culling을 확인할 수 있는 Scene이 포함되어 있습니다.

| Scene | 실제 구성 | 확인 항목 |
| --- | --- | --- |
| `GouraudTest.Scene` | Static Mesh 1, Directional 1 | Gouraud와 Per-Pixel Lighting 비교 |
| `LightDemoScene.Scene` | Ambient/Directional/Point/Spot 각 1, 여러 Material과 Decal | 네 Light 종류, Normal Map 유무, 통합 Lighting |
| `PointLightTest.Scene` | Static Mesh 1, Point 1 | Radius와 거리 감쇠 |
| `SpecularTest.Scene` | Static Mesh 1, Directional 1 | Shininess와 Blinn-Phong Highlight |
| `LightCullingTest0.Scene` | Point 2, Spot 2, Static Mesh 중심 장면 | Point/Spot Tile 판정과 Hitmap |
| `LightCullingTest1.Scene` | Static Mesh 8,000, Point 9,261 | 대량 장면에서 Scene Light 선별과 Tile Culling Stress Test |

권장 확인 순서:

1. `LightDemoScene.Scene`에서 네 종류의 Light와 Normal Map 적용 결과를 확인합니다.
2. 같은 표면을 `Lit_Gouraud`, `Lit_Lambert`, `Lit_Phong`으로 전환해 계산 위치와 Highlight 차이를 비교합니다.
3. `WorldNormal`에서 Normal Map 적용 전후의 표면 방향을 확인합니다.
4. `LightCullingTest0.Scene`에서 Hitmap을 켜 Point·Spot 후보 수와 Tile 경계를 확인합니다.
5. Culling Mode를 끄고 같은 장면의 Lighting 결과와 성능을 비교합니다.
6. `LightCullingTest1.Scene`에서 대량 Light 중 영향도 상위 256개만 Point Buffer에 업로드되는 동작을 확인합니다.

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

- Forward Lighting은 Ambient·Directional·Point·Spot Light를 지원합니다.
- Gouraud는 Per-Vertex Blinn-Phong, Lambert와 Blinn-Phong은 Per-Pixel 경로입니다.
- 2.5D Tile Culling 대상은 지역 광원인 Point·Spot Light입니다.
- Tile 크기는 16×16 Pixel이고, 깊이는 Tile별 최소~최대 구간을 32개 Slice로 나눕니다.
- Scene에서는 Point·Spot을 각각 최대 256개 업로드하고, Tile별 목록도 종류마다 최대 256개입니다.
- Depth Prepass와 Culling은 현재 활성 Sub-Viewport 크기를 기준으로 실행됩니다.
- Shadow Mapping은 이 주차 구현 범위에 포함되지 않으며 다음 주차 주제입니다.

### 제한 사항

- Gouraud Lighting은 Vertex Shader에서 전체 업로드 광원을 순회하므로 Tile Culling과 Normal Map이 실제 조명 계산에 반영되지 않습니다.
- Light Culling을 꺼도 Depth Prepass와 Point·Spot 상위 256개 선별은 유지되므로, 완전히 최적화 전의 모든 광원 순회 경로와 같지는 않습니다.
- Depth Prepass는 Opaque Static Mesh의 Geometry만 그리며 Material Alpha Mask를 반영하는 Pixel Shader 경로는 없습니다.
- `AmbientMap`은 Resource 로드와 `t7` 바인딩까지만 구현되어 있고 `UberLit`에서 샘플링하지 않습니다.
- `map_Bump`와 `bump`는 높이 맵 변환 없이 Tangent-space Normal Map으로 직접 해석합니다.
- 여러 Ambient Light의 기여도는 합산하지 않고 마지막으로 수집된 Light가 이전 값을 덮어씁니다.
- Ambient Light Actor에는 Billboard가 없으며, 일반 Actor의 `Add Component` 목록에도 Ambient Light가 없습니다.
- Directional Light Buffer는 64개 용량으로 생성되지만 업로드 전에 개수를 64개로 제한하는 방어 코드가 없습니다.
- Tile별 Point·Spot 목록은 각각 256개에서 잘리며, Buffer도 모든 Tile에 최대 개수만큼의 Index 공간을 할당합니다.
- `LightCullingTest1.Scene`에 9,261개의 Point Light가 있어도 현재 Renderer가 실제 GPU 조명 계산에 올리는 것은 영향도 상위 256개입니다.
- Static Mesh의 `Lit_Gouraud`와 달리 Decal의 같은 View Mode 분기는 Per-Pixel Lambert로 처리됩니다.
- Light Debug/Hitmap 설정은 저장 함수에서 `View` JSON을 Root에 복사한 뒤 추가되고 `bLightCullingMode`는 저장 항목에도 빠져 있어, 다음 실행에 정상적으로 보존되지 않습니다.
