# NipsEngine — Week 8 Shadow Mapping, PSM & Shadow Resource Management

> **Shadow Mapping · PSM · CSM · PCF · VSM · Shadow Atlas · Multi-light Shadow**

크래프톤 정글 GameTechLab 자체 게임엔진 제작 교육의 8주차 결과물입니다. DirectX 11 기반 Forward Renderer에 광원 관점의 Shadow Mapping을 추가하고, PSM(Perspective Shadow Mapping), CSM(Cascade Shadow Map), PCF/VSM 필터, 다중 광원용 Shadow Resource 관리와 에디터 검증 도구를 구현했습니다.

이 문서는 과제 명세뿐 아니라 현재 저장소의 실제 렌더 경로와 구현 범위를 기준으로 작성했습니다.

## 학습 목표

- Light Perspective에서 Shadow Map을 생성하고 Camera Perspective에서 조회하는 과정을 이해한다.
- PSM의 목적과 Virtual Camera·Post-Perspective 행렬 구성 과정을 구현한다.
- Shadow Bias와 Slope Bias가 Shadow Acne과 Peter Panning에 미치는 영향을 확인한다.
- 여러 광원의 Shadow Map을 선택·할당·조회하는 구조를 설계한다.
- CSM, PCF, VSM, Shadow Atlas의 품질·성능·메모리·SRV 수 트레이드오프를 확인한다.

## 구현 요약

| 영역 | 구현 내용 |
| --- | --- |
| Shadow 대상 광원 | `Cast Shadows`가 켜진 Directional, Point, Spot Light |
| Directional Shadow | 고정 3 Cascade CSM, 광원별 2D Texture Array |
| Point Shadow | 광원당 6개 Face, 선택된 광원이 공유하는 Cube Map Array |
| Spot Shadow | 4096×4096 Depth Atlas에 영역 단위 패킹 |
| 투영 방식 | Standard / PSM 전환, PSM 계산 실패 시 Standard 폴백 |
| 필터 | SSM, 3×3 PCF, VSM |
| Artifact 제어 | Resolution Scale, Bias, Slope Bias, Sharpen |
| Resource 관리 | Depth Resource Pool, VSM Resource Pool, Atlas Allocator |
| 에디터 검증 | Shadow Map Preview, Override Camera, Shadow Stats, Light Debug |

과제 문서에는 네 종류의 광원이 언급되지만, 실제 Shadow Map 생성 대상은 Directional·Point·Spot Light입니다. Ambient Light는 장면 조명에는 참여하지만 방향이나 위치 기반의 Shadow Map은 생성하지 않습니다.

## 전체 렌더 흐름

`FRenderPipeline`은 장면의 Depth Prepass와 Opaque Pass보다 먼저 `FShadowPass`를 실행합니다. 현재 Camera와 Light 정보를 이용해 Shadow 대상 광원을 선별하고, 광원 종류에 맞는 View·Projection과 GPU Resource를 구성한 뒤 Static Mesh의 Depth를 기록합니다.

```text
RenderCollector / RenderBus
  │
  ├─ Shadow caster와 Light 수집
  │
  ├─ ShadowLightSelector
  │   ├─ Cast Shadows가 켜진 광원만 후보로 등록
  │   ├─ Point·Spot의 카메라 영향도 계산
  │   └─ 해상도와 Cascade 요청 생성
  │
  ├─ ShadowPass
  │   ├─ Directional: Camera Frustum을 3개 Cascade로 분할
  │   ├─ Point: 6방향 Cube View 생성
  │   ├─ Spot: Atlas 영역 할당
  │   ├─ Standard / PSM 투영 상태 결정
  │   └─ Light Perspective에서 Shadow caster 렌더링
  │
  ├─ VSM 후처리
  │   ├─ Directional: Depth → Moment → Horizontal / Vertical Blur
  │   └─ Point: Cube Face 렌더링 중 Moment를 직접 출력
  │
  └─ OpaquePass / UberLit
      ├─ Light index를 Shadow constant index로 변환
      ├─ Light-space 좌표와 Shadow texture 조회
      └─ SSM / PCF / VSM 결과를 광원 기여도에 반영
```

`Unlit` ViewMode에서는 Opaque Pass가 `UberUnlit.hlsl`로 교체되어 최종 화면에 조명과 그림자를 적용하지 않습니다. 다만 현재 파이프라인은 `Unlit`에서도 앞단의 Shadow Pass 자체를 건너뛰지는 않습니다.

관련 코드:

- `NipsEngine/Source/Engine/Render/Scene/RenderCollector.cpp`
- `NipsEngine/Source/Engine/Render/Scene/ShadowLightSelector.cpp`
- `NipsEngine/Source/Engine/Render/Renderer/RenderFlow/RenderPipeline.cpp`
- `NipsEngine/Source/Engine/Render/Renderer/RenderFlow/ShadowPass.cpp`
- `NipsEngine/Source/Engine/Render/Renderer/RenderFlow/OpaqueRenderPass.cpp`
- `NipsEngine/Shaders/ShadowMap.hlsl`
- `NipsEngine/Shaders/UberLit.hlsl`

## 1. Shadow Light Selection과 다중 광원 대응

`FShadowLightSelector`는 Ambient Light와 `Cast Shadows`가 꺼진 광원을 제외합니다. Directional Light에는 가장 높은 우선순위를 부여하고, Point·Spot Light는 카메라와의 거리, 감쇠 반경, 밝기와 Spot 방향을 조합해 영향도 점수를 계산합니다.

| Light | 후보 선택 예산 | Shadow 표현 | GPU Resource |
| --- | ---: | --- | --- |
| Directional | Spot과 합쳐 최대 16개 | 3개 Cascade | 2D Texture Array |
| Point | 최대 32개 | 6개 Cube Face | 공유 Cube Map Array |
| Spot | Directional과 합쳐 최대 16개 | 단일 Perspective Map | 4096 Depth Atlas |
| Ambient | 제외 | 없음 | 없음 |

후보를 만든 뒤에는 Directional → Point → Spot 순서로 요청을 정리하고, `UberLit`의 Shadow Constant Buffer 크기에 맞춰 최종 32개까지만 처리합니다. 따라서 후보 예산의 합이 32를 넘으면 모든 후보가 실제 Shadow Map을 받는 것은 아닙니다.

### 광원별 해상도

| Light | 기본 해상도 | 허용 범위 |
| --- | ---: | ---: |
| Directional | 1024 | 256~2048 |
| Point | 512 | 256~1024 |
| Spot | 1024 | 256~2048 |

`Shadow Resolution Scale`은 0.125~4.0 범위로 제한되며, 계산된 해상도는 128 단위로 정렬됩니다. Point Light는 하나의 Cube Map Array를 공유하므로, 해당 프레임에 선택된 Point Light 중 가장 큰 요청 해상도가 전체 Cube Array 해상도가 됩니다.

관련 코드:

- `NipsEngine/Source/Engine/Render/Scene/ShadowLightSelector.cpp`
- `NipsEngine/Source/Engine/Render/Renderer/RenderFlow/OpaqueRenderPass.h`

## 2. 광원별 Shadow Map 구성

### Directional Light: 3-Cascade CSM

Directional Light는 Camera View Frustum을 다음 세 구간으로 분할합니다.

```text
Camera Near ~ 50  → Cascade 0
50          ~ 200 → Cascade 1
200         ~ 400 → Cascade 2
```

각 구간의 Frustum Corner 8개를 World Space에서 구한 뒤 Light Space로 변환하고, 이를 감싸는 Orthographic Projection을 생성합니다. 가까운 영역에 동일 해상도의 Texture를 더 좁게 배분해 하나의 넓은 Shadow Map을 사용할 때보다 근거리 Shadow의 밀도를 높입니다.

세 Cascade는 광원별 2D Texture Array의 Slice에 저장됩니다. `UberLit`은 현재 Pixel의 View-space depth와 `CascadeSplits`를 비교해 조회할 Slice를 선택합니다.

Directional Light Component에는 `Cascade Count`, `Shadow Distance`, `Cascade Splits` 데이터가 선언·직렬화되어 있습니다. 그러나 현재 활성 Shadow Pass는 이 값을 사용하지 않고 위의 세 구간을 고정으로 생성합니다.

### Point Light: 공유 Cube Map Array

Point Light는 광원 위치에서 `+X`, `-X`, `+Y`, `-Y`, `+Z`, `-Z` 여섯 방향을 각각 90도 Perspective Projection으로 렌더링합니다. Far Plane은 Light의 Attenuation Radius를 사용합니다.

선택된 모든 Point Light의 Face는 하나의 Cube Map Array에 연속해서 저장됩니다.

```text
Point Light 0 → Array Face 0~5
Point Light 1 → Array Face 6~11
Point Light N → Array Face 6N~6N+5
```

Pixel Shader는 Pixel에서 Light까지의 방향과 정규화한 거리를 이용해 `TextureCubeArray`를 조회합니다. Shadow caster의 Bounding Sphere가 Light Radius 밖에 있으면 해당 광원의 Shadow Draw에서 제외합니다.

### Spot Light: Shadow Atlas

Spot Light는 각 광원마다 Texture를 생성하는 대신 4096×4096 Depth Atlas를 공유합니다. `FShadowAtlasAllocator`는 Atlas를 사분면으로 재귀 분할하는 Buddy 방식으로 요청 해상도와 같은 정사각형 영역을 찾습니다.

```text
Spot Shadow Request
  → 해상도별 Atlas 영역 할당
  → 해당 영역을 Viewport로 설정
  → Spot Light Perspective에서 Depth 렌더링
  → UV Offset / Scale을 Shadow Constant에 기록
  → UberLit에서 Atlas UV로 변환해 샘플링
```

Atlas가 가득 차면 새 4096×4096 Resource를 만들 수 있도록 구현되어 있습니다. 다만 현재 Opaque Pass는 첫 번째 Atlas SRV만 Shader에 바인딩하므로, 한 프레임에 두 번째 Atlas가 필요한 구성은 완전히 지원되지 않습니다.

관련 코드:

- `NipsEngine/Source/Engine/Render/Renderer/RenderFlow/ShadowPass.cpp`
- `NipsEngine/Source/Engine/Render/Resource/ShadowAtlasAllocator.cpp`
- `NipsEngine/Source/Engine/Render/Resource/ShadowResourcePool.cpp`
- `NipsEngine/Source/Engine/Component/Light/DirectionalLightComponent.cpp`
- `NipsEngine/Source/Engine/Component/Light/PointLightComponent.cpp`
- `NipsEngine/Source/Engine/Component/Light/SpotLightComponent.cpp`

## 3. Standard Shadow Projection과 PSM

PSM은 Camera에 가까운 영역에 Shadow Map 정밀도를 더 배분하기 위해 Camera Perspective 이후의 공간에서 Light Projection을 구성하는 방식입니다. 이 프로젝트에서는 Viewport의 전역 `Projection Mode`와 광원별 `Apply PSM`이 모두 켜져 있어야 PSM을 요청합니다.

| Light | PSM 지원 | 처리 방식 |
| --- | --- | --- |
| Directional | 지원 | Receiver/Caster Bounds로 Virtual Camera를 맞춘 뒤 Post-Perspective View·Projection 계산 |
| Spot | 지원 | Camera에 보이는 Receiver/Caster를 Spot Cone에 맞춰 Perspective Projection 계산 |
| Point | 미지원 | 6 Face Standard Cube Projection 유지 |

Directional PSM은 `Camera Slider Back` 값으로 Virtual Camera의 후퇴 거리를 조절합니다. Spot PSM은 Light 위치·방향·Outer Cone·Attenuation Radius와 유효한 Receiver/Caster Point를 이용해 Near/Far와 투영 범위를 맞춥니다.

다음과 같은 경우에는 행렬에 잘못된 값이 전달되지 않도록 Standard Shadow로 폴백합니다.

- Render Context 또는 Camera Basis가 유효하지 않은 경우
- Directional PSM에 사용할 Receiver가 없는 경우
- Virtual Camera 또는 Post-Perspective 행렬 생성에 실패한 경우
- Spot Light Basis나 Receiver Fit 결과가 유효하지 않은 경우
- 최종 PSM 행렬에 NaN/Infinity가 포함된 경우

폴백 원인과 복구 여부는 로그로 남기며, Directional/Spot Light Debug가 켜져 있으면 진단 정보를 더 자주 확인할 수 있습니다.

관련 코드:

- `NipsEngine/Source/Engine/Render/Common/PSMCalculator.h`
- `NipsEngine/Source/Engine/Render/Renderer/RenderFlow/ShadowPass.cpp`
- `NipsEngine/Source/Engine/Render/Common/ShadowTypes.h`

## 4. Shadow Artifact와 Filtering

### Bias, Slope Bias와 Sharpen

`Shadow Bias`는 저장된 Depth와 현재 Pixel Depth를 비교할 때 더하는 기본 Offset입니다. 값이 너무 작으면 표면이 자기 자신을 가리는 Shadow Acne이 나타나고, 너무 크면 그림자가 물체에서 떨어지는 Peter Panning이 발생합니다.

`Shadow Slope Bias`는 Surface Normal과 Light 방향의 각도에서 구한 기울기에 비례해 추가됩니다. 비스듬한 표면에서 필요한 Bias를 늘리되 정면 표면의 과도한 이격을 줄이는 역할을 합니다.

2D Shadow에서는 해상도와 투영 방식에 따른 최소 Bias도 함께 적용합니다. PSM은 왜곡된 공간에서 비교하므로 Standard보다 큰 해상도 기반 최소값을 사용합니다.

`Shadow Sharpen`은 PCF와 Directional VSM Blur의 Filter Scale을 1.0에서 최소 0.25까지 줄입니다. 값이 높을수록 Sampling/Blur 반경이 작아져 경계가 더 선명해집니다.

### SSM, PCF, VSM

| Filter | 실제 처리 | 적용 범위 |
| --- | --- | --- |
| SSM | Shadow Depth를 한 번 비교 | Directional, Point, Spot |
| SSM + PCF | 3×3 주변 Sample을 평균 | 2D Map, Cube Map, Atlas |
| VSM | 1·2차 Moment와 Chebyshev Bound로 가시성 계산 | Directional, Point |

Directional VSM은 Depth Texture를 Moment Texture로 변환한 뒤 가로·세로 두 번 Blur합니다. Point VSM은 각 Cube Face를 그릴 때 Depth와 Depth² Moment를 Render Target에 직접 기록하며, 별도의 2D 양방향 Blur 단계는 거치지 않습니다.

Spot Light는 Depth Atlas 경로만 구현되어 있습니다. 전역 Filter를 VSM으로 설정해도 Spot Shadow는 자동으로 SSM + PCF로 해석됩니다. VSM에는 최소 분산과 Light Bleeding Reduction을 적용해 분산이 지나치게 작거나 밝은 영역이 번지는 문제를 완화합니다.

관련 코드:

- `NipsEngine/Shaders/UberLit.hlsl`
- `NipsEngine/Shaders/Multipass/ShadowVSMConvertPass.hlsl`
- `NipsEngine/Shaders/Multipass/ShadowVSMBlurPass.hlsl`
- `NipsEngine/Shaders/Multipass/ShadowPointVSMPass.hlsl`

## 5. Shadow Resource와 Shader Binding

### Resource Pool

Depth Resource는 Map Type, Allocation Mode, Resolution, Cascade Count, Cube Count가 같은 미사용 항목을 Pool에서 재사용합니다. VSM의 Moment/Temporary Texture도 해상도, Slice 수, Cube 여부가 같은 Resource를 별도 Pool에서 재사용합니다.

Light와 Object는 모두 Movable로 간주하므로 Shadow Depth는 매 프레임 다시 그립니다. Pool은 이전 프레임의 GPU Texture를 재활용해 생성·해제 비용을 줄이는 역할이며, 정적인 Shadow 결과를 캐싱하는 구조는 아닙니다.

### 고정 Shadow SRV 슬롯

Opaque Pass는 Shadow Resource를 다음 다섯 슬롯에 바인딩합니다.

| Slot | Resource |
| ---: | --- |
| `t14` | Directional Depth 2D Array |
| `t15` | 공유 Point Depth Cube Array |
| `t16` | Spot Depth Atlas |
| `t17` | Directional VSM Moment 2D Array |
| `t18` | 공유 Point VSM Moment Cube Array |

Point Light와 Spot Light를 Array/Atlas에 모아 광원마다 SRV를 추가하지 않고 고정 슬롯으로 조회할 수 있게 했습니다. 반면 현재 바인딩 구조는 종류별 첫 번째 Resource 하나만 전달하므로, 여러 Directional Resource 또는 여러 Spot Atlas가 필요한 경우에는 추가 SRV 인덱싱이 필요합니다.

관련 코드:

- `NipsEngine/Source/Engine/Render/Resource/ShadowResourcePool.cpp`
- `NipsEngine/Source/Engine/Render/Renderer/RenderFlow/OpaqueRenderPass.cpp`
- `NipsEngine/Source/Engine/Render/Renderer/RenderFlow/OpaqueRenderPass.h`
- `NipsEngine/Shaders/UberLit.hlsl`

## 6. 에디터 검증 도구

### Viewport와 Light Property

| 도구 | 확인 가능한 내용 |
| --- | --- |
| `Projection Mode` | Standard / PSM 전환 |
| `Filter Mode` | SSM / SSM + PCF / VSM 전환 |
| `Cast Shadows` | 선택한 Light의 Shadow 생성 여부 |
| `Apply PSM` | Directional·Spot Light의 PSM 허용 여부 |
| `Shadow Resolution Scale` | 광원별 Shadow 요청 해상도 |
| `Shadow Bias` | 기본 Depth 비교 Offset |
| `Shadow Slope Bias` | 표면 기울기 기반 추가 Bias |
| `Shadow Sharpen` | PCF/VSM Filter 반경 축소 |
| `Shadow Map Preview` | Directional Cascade, Point Face, Spot Atlas 영역 |
| `Override Camera` | 선택한 Light와 Slice의 View·Projection으로 Viewport 전환 |

Preview는 선택한 Shadow Resource의 Array Slice를 별도 Preview Texture로 복사합니다. Spot Light는 전체 Atlas가 아니라 해당 Light에 할당된 UV 영역만 잘라 표시합니다. Point Light는 `+X`, `-X`, `+Y`, `-Y`, `+Z`, `-Z` Face를 선택할 수 있습니다.

`Override Camera`는 Light Component의 `BuildShadowView()` 결과를 사용합니다. 따라서 광원 기준 View를 확인하는 용도이며, PSM의 Post-Perspective 왜곡까지 그대로 재현하는 Preview는 아닙니다.

### Shadow Stats와 Console

`Shadow Stats`는 다음 정보를 수집해 표시합니다.

- Shadow-casting Light 수와 종류
- Logical Shadow Map, Resource View 수
- Light별 Projection/Filter Mode, 해상도와 Texture Format
- Cascade, Cube Face, Atlas Region별 Slice 정보
- Depth, VSM Moment, VSM Temporary Texture의 메모리 사용량

Console에서는 다음 명령을 지원합니다.

```text
stat shadow
shadow_filter ssm
shadow_filter pcf
shadow_filter vsm
```

`stat shadow`는 현재 Viewport의 Shadow 통계를 전환하며, `shadow_filter`는 Viewport Settings와 같은 전역 Filter Mode를 변경합니다.

관련 코드:

- `NipsEngine/Source/Editor/UI/EditorPropertyWidget.cpp`
- `NipsEngine/Source/Editor/UI/EditorViewportOverlayWidget.cpp`
- `NipsEngine/Source/Editor/UI/EditorConsoleWidget.cpp`
- `NipsEngine/Source/Editor/Viewport/EditorViewportClient.cpp`

## 기능 확인용 Scene

저장소에 포함된 실제 Scene은 다음 네 개입니다.

| Scene | Light 구성 | 확인 항목 |
| --- | --- | --- |
| `Scene_01_LightComponents.Scene` | Directional 1, Point 7, Spot 6, Ambient 1 | 여러 종류의 Light와 Shadow 선택·통계 |
| `Scene_01_LightComponents_whitewall.Scene` | Directional 1, Point 7, Spot 6, Ambient 1 | 밝은 배경에서 Shadow 경계, Bias, Filter 비교 |
| `Scene_02_LightingModels.Scene` | Directional 1, Point 3, Spot 7, Ambient 1 | 서로 다른 Lighting Model과 Shadow 결과 |
| `Scene_02_LightingModels_Decal.Scene` | Directional 1, Point 3, Spot 8, Ambient 1 | Decal이 포함된 장면의 Lighting·Shadow 확인 |

권장 확인 순서:

1. Directional 또는 Spot Light의 `Cast Shadows`를 전환해 해당 광원의 그림자 생성 여부를 확인합니다.
2. `Projection Mode`를 Standard/PSM으로 바꾸고 `Apply PSM`이 켜진 광원의 결과를 비교합니다.
3. SSM/PCF/VSM을 전환해 경계와 Light Bleeding 차이를 확인합니다.
4. `Shadow Bias`, `Shadow Slope Bias`, `Shadow Sharpen`을 조절해 Artifact 변화를 확인합니다.
5. Directional Cascade와 Point Cube Face, Spot Atlas 영역을 Preview로 확인합니다.
6. `stat shadow`에서 해상도·Resource 수·메모리 변화를 확인합니다.

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

- Shadow caster는 현재 `StaticMeshComponent`만 수집합니다.
- Shadow를 생성하는 광원은 Directional·Point·Spot이며 Ambient Light는 제외합니다.
- Directional CSM은 현재 Camera Near~50, 50~200, 200~400의 세 구간으로 고정됩니다.
- Point 후보는 최대 32개, Directional/Spot 후보는 합쳐 최대 16개이며 최종 Shadow Constant는 32개입니다.
- Point Shadow는 공유 Cube Map Array의 최대 요청 해상도로 통일됩니다.
- Spot Light는 Depth Atlas를 사용하며 VSM 선택 시 PCF로 폴백합니다.
- PSM은 Directional·Spot Light에만 적용되며 계산 실패 시 Standard로 폴백합니다.
- 모든 Light와 Object는 Movable로 간주하고 Shadow Map을 매 프레임 갱신합니다.

### 제한 사항

- `Unlit`은 최종 Opaque Shader에서 조명과 그림자를 제외하지만 Shadow Pass 실행 자체를 생략하지는 않습니다.
- CSM의 Cascade Count·Split·Distance는 활성 Shadow Pass에 연결되지 않았으며, 이를 제어하는 Console 명령도 구현되어 있지 않습니다.
- Directional Light Property의 `Shadow Distance`와 `Cascade Splits` 편집 연결은 현재 완성되지 않았습니다.
- Shader는 종류별 Shadow SRV를 하나씩만 바인딩하므로 실질적으로 Directional Light 한 개와 Spot Atlas 한 개를 전제로 합니다.
- Atlas가 가득 차면 두 번째 Atlas Resource를 생성하지만, 현재 Opaque Pass에서는 첫 번째 Atlas만 샘플링할 수 있습니다.
- Point Light별 Resolution Scale이 달라도 공유 Cube Map Array는 선택된 Point Light 중 가장 큰 해상도를 사용합니다.
- Point VSM에는 Directional VSM과 같은 가로·세로 Blur Pass가 적용되지 않습니다.
- `Override Camera`는 Standard Light View·Projection을 사용하며 PSM Warping 결과를 직접 보여주지는 않습니다.
- Shadow caster는 Static Mesh로 한정되고, Material의 Alpha Mask를 반영하는 전용 Shadow Pixel Shader 경로는 없습니다.
