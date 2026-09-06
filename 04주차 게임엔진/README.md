# CO-PASS Engine — Week 4 Static Mesh Pipeline & Multi-Viewport

> **OBJ Static Mesh Pipeline · Material Sections · Multi-Viewport · OBJ Viewer**

크래프톤 정글 GameTech Lab 자체 게임엔진 제작 교육의 4주차 결과물입니다. 이 문서는 4주차 과제인 **Wavefront OBJ 임포트**, **정적 메시 렌더링**, **다중 뷰포트**, **리소스 Bake**, **OBJ Viewer** 구현에만 집중합니다.

![CO-PASS Logo](Editor/Resources/Tool/copass.png)

## 학습 목표

- Wavefront OBJ 파일 포맷을 이해하고 파싱한다.
- Blender에서 내보낸 OBJ/MTL 리소스를 엔진 런타임 구조로 변환한다.
- `UStaticMesh`와 `UStaticMeshComponent`를 통해 월드에 정적 메시를 배치하고 렌더링한다.
- SubMesh Section 단위의 다중 머티리얼을 지원한다.
- 원본 OBJ를 매번 파싱하지 않도록 바이너리 `.uasset`으로 Bake한다.
- 여러 카메라를 한 화면에 배치하는 Multi-viewport를 구현한다.

## 구현 요약

| 과제 항목 | 구현 내용 |
| --- | --- |
| OBJ Parsing | `v`, `vt`, `vn`, `f`, `mtllib`, `usemtl`을 읽고 정점 조합 중복 제거·UV V축 반전·다각형 삼각화를 수행 |
| Static Mesh | `UStaticMesh`가 CPU 메시 정보와 D3D11 Vertex/Index Buffer를 관리 |
| Multiple Material | `usemtl` 경계를 Section으로 분리하고 Section별 Material Slot을 렌더링 |
| Asset Bake | OBJ/MTL을 `.uasset`으로 직렬화하고 source hash가 같으면 재생성을 생략 |
| World Placement | `AStaticMeshActor`와 `UStaticMeshComponent`로 씬에 메시를 배치하고 CPU Ray Picking 및 Outline에 연결 |
| Multi-viewport | 1·2·3·4분할 레이아웃, 분할선 조절, 뷰포트별 카메라와 공용 선택 상태 지원 |
| OBJ Viewer | OBJ 파일 열기/드래그 앤 드롭, Orbit 카메라, View/Cull 모드, 좌표계 변환 |
| Diagnostics | FPS·프레임 시간·엔진 Heap·시스템 메모리·추적 대상 GPU 메모리를 활성 뷰포트에 표시 |
| UV Scroll | 로드된 머티리얼의 UV 속도를 Details에서 편집하고 시간과 함께 셰이더 상수 버퍼로 전달 |

## 1. OBJ → 엔진 리소스 파이프라인

```text
Blender
  └─ OBJ + MTL + Texture
       └─ FEditorAssetImporter
            ├─ OBJ Parser → FStaticMesh + SubMesh Sections
            ├─ MTL Parser → FMaterial
            └─ Binary Bake → StaticMesh / Material .uasset
                 └─ UAssetManager + Loader
                      └─ UStaticMesh / UMaterial
                           └─ D3D11 Static Mesh Renderer
```

### 1.1 Wavefront OBJ Parsing

`FEditorAssetImporter`와 `FStaticMeshLoader`는 OBJ 텍스트를 읽어 엔진의 `FStaticMesh` 구조로 변환합니다.

- Position(`v`), UV(`vt`), Normal(`vn`)을 읽어 `FMeshVertexPNCT` 정점을 구성합니다.
- `v/vt/vn`, `v//vn` Face Token을 해석하고 같은 정점 조합은 캐시해 재사용합니다.
- OBJ의 UV 원점과 엔진 좌표계를 맞추기 위해 V 좌표를 `1 - V`로 반전합니다.
- 삼각형보다 큰 Face는 첫 정점을 기준으로 Fan Triangulation합니다.
- `usemtl`이 바뀌는 지점에서 `FSubMesh`를 생성해 Index 범위와 기본 머티리얼 이름을 기록합니다.
- 메시의 Local Bounding Box와 CPU Position/Index 데이터를 유지해 Bounds 및 기하 정보에 활용합니다.
- 로더는 정점/인덱스 데이터로 D3D11 immutable Vertex Buffer와 Index Buffer를 생성합니다.

관련 코드:

- `Editor/Source/Importer/EditorAssetImporter.cpp`
- `Engine/Source/Asset/AssetLoader/StaticMeshLoader.cpp`
- `Engine/Source/Renderer/RenderAsset/StaticMeshResource.h`

### 1.2 MTL과 다중 머티리얼 Section

OBJ의 `mtllib`와 `usemtl` 정보를 이용해 `.mtl`을 읽고, SubMesh Section마다 머티리얼을 연결합니다.

- `.mtl`의 `Ka`, `Kd`, `Ks`, `Ke`, `Tf`, `Ns`, `Ni`, `d`/`Tr`, `illum`을 `FMaterial`로 보관합니다.
- `map_Ka`, `map_Kd`, `map_Ns`, `map_bump`/`bump` 경로를 파싱하고 텍스처 의존성을 로드합니다.
- `UStaticMesh`는 SubMesh 개수에 맞는 Material Slot을 가지며, `UStaticMeshComponent`는 컴포넌트 단위 Override를 우선 적용합니다.
- 렌더러는 메시마다, 다시 SubMesh마다 순회해 해당 Slot의 머티리얼을 바인딩하고 `DrawIndexed`를 호출합니다.

현재 Static Mesh 셰이더가 직접 사용하는 머티리얼 값은 Diffuse Texture, Diffuse Color, Opacity, UV Scroll입니다. `Lit` 모드의 조명은 고정된 Half-Lambert와 Hemisphere Ambient로 계산합니다. Normal/Specular 등 MTL 데이터와 경로는 로드·보관하지만 Normal Mapping이나 MTL 기반 Specular Lighting에는 연결하지 않았습니다.

관련 코드:

- `Engine/Source/Asset/AssetLoader/MaterialLoader.cpp`
- `Engine/Source/Asset/StaticMesh.h`
- `Engine/Source/Engine/Component/Mesh/StaticMeshComponent.cpp`
- `Engine/Source/Renderer/D3D11/D3D11StaticMeshRenderer.cpp`

### 1.3 바이너리 Bake

에디터용 `FEditorAssetImporter`는 OBJ/MTL 파싱 결과를 원본 파일 옆의 `.uasset`으로 저장합니다.

- Static Mesh `.uasset`: Header, source hash, Vertex/Index 정보, Bounds, SubMesh 정보, Material Asset 경로와 CPU Picking용 Position/Index를 저장합니다.
- Material `.uasset`: Material 이름, 색상/투명도/맵 경로, UV Scroll 속도를 저장합니다.
- `Magic`, `Version`, `AssetType`, FNV-1a source hash가 기존 `.uasset` Header와 일치하면 Bake를 건너뜁니다.
- 런타임 `FStaticMeshLoader`와 `FMaterialLoader`는 `.obj`/`.mtl`뿐 아니라 타입 Header를 검사한 `.uasset`도 로드합니다.

이 흐름은 매 실행마다 텍스트 OBJ를 다시 파싱하지 않고, 변환된 리소스를 재사용하기 위한 파이프라인입니다. `.uasset`에는 Texture 바이너리를 내장하지 않고 참조 경로를 기록하므로 원본 Texture 파일은 함께 유지해야 합니다.

> 과제 문서의 `FObjManager` 역할은 현재 코드베이스에서 별도 클래스명이 아니라 `UAssetManager`와 `FStaticMeshLoader` 조합으로 구현되어 있습니다.

## 2. 월드 배치와 Static Mesh 렌더링

`AStaticMeshActor`는 기본 Scene Root와 `UStaticMeshComponent`를 자식 컴포넌트로 생성합니다. Outliner에서 Static Mesh Actor를 생성하면 기본 OBJ 경로를 지정할 수 있고, Details에서 메시 경로와 Material Slot을 편집할 수 있습니다.

`UStaticMeshComponent`는 씬 파일에 메시 경로를 저장합니다. 씬을 로드하면 `ResolveAssetReferences()`가 경로를 `UStaticMesh`로 해석하고, 메시의 SubMesh 수에 맞춰 Material Slot을 초기화합니다.

렌더링 단계에서는 Scene이 `FStaticMeshRenderItem`을 만들고 다음 데이터를 제출합니다.

- World Matrix
- Static Mesh Render Resource
- SubMesh별 Material Interface
- World AABB
- 선택·가시성·Picking 상태

Static Mesh 렌더러는 Render Item의 SubMesh를 순회하며 Section별 머티리얼 상수와 Diffuse Texture를 바인딩한 뒤 해당 Index 범위만 `DrawIndexed`합니다. 선택된 Static Mesh는 별도의 Selection Mask Pass에도 제출되며, 이후 Post-process Outline Pass에서 외곽선을 합성합니다.

### 2.1 CPU Ray Picking

Actor 선택은 뷰포트의 마우스 좌표로 World Ray를 만든 뒤 다음 순서로 판정합니다.

1. Component의 World AABB와 Ray를 검사해 후보를 줄입니다.
2. `.uasset` 또는 OBJ에서 유지한 CPU Position/Index로 Local Triangle을 만들고, World로 변환한 Triangle과 정밀 교차 검사합니다.
3. 교차한 후보 중 Ray 거리 `T`가 가장 가까운 Actor를 선택합니다.

Static Mesh도 기존 Primitive와 같은 선택 흐름에 참여하지만, 별도의 공간 가속 구조 없이 후보 Component의 Triangle을 순회하므로 고밀도 메시가 많을수록 Picking 비용이 증가합니다.

관련 코드:

- `Engine/Source/Engine/Game/StaticMeshActor.cpp`
- `Engine/Source/Engine/Component/Mesh/StaticMeshComponent.cpp`
- `Engine/Source/Engine/Scene.cpp`
- `Engine/Source/Renderer/RendererModule.cpp`
- `Editor/Source/Viewport/Selection/ViewportSelectionController.cpp`

## 3. UV Scroll

`FMaterial`은 `UVScrollSpeed`(X, Y)를 가집니다. Details 패널에서 Material Slot의 UV Scroll 속도를 편집하면 로드된 `UMaterial`의 데이터를 변경하고, Static Mesh 렌더러가 이를 `Time`과 함께 상수 버퍼로 전달해 Pixel Shader에서 UV를 이동시킵니다.

- 설정 위치: Static Mesh Actor의 Material Slot Details
- 바이너리 형식: Material `.uasset`에 `UVScrollSpeed` 필드 포함
- 렌더링 전달: `FMeshLitConstants.ScrollSpeedX/Y`, `FMeshLitConstants.Time`

현재 Details 편집은 메모리에 로드된 머티리얼 에셋 자체를 수정합니다. 따라서 같은 `UMaterial`을 공유하는 Component에 함께 반영되며, 편집값을 Material `.uasset`에 다시 기록하는 저장 경로는 구현되어 있지 않습니다.

관련 코드:

- `Editor/Source/Panel/PropertiesPanel.cpp`
- `Engine/Source/Renderer/RenderAsset/MaterialResource.h`
- `Engine/Source/Renderer/D3D11/D3D11StaticMeshRenderer.cpp`
- `Editor/Content/Shader/StaticMeshShader.hlsl`

## 4. Multi-Viewport

`FWindowOverlayManager`가 여러 `FEditorViewportPanel`과 Splitter를 관리합니다. Scene의 공통 렌더 데이터는 프레임당 한 번 구성하고, 각 패널은 자신의 `SceneView`와 카메라로 동일한 Scene을 렌더링합니다.

지원 레이아웃:

- `Single`
- `TwoColumn`
- `TwoRow`
- `ColumnTwoRow`
- `TwoRowColumn`
- `FourWay`

각 뷰포트는 독립 카메라와 View Orientation을 가지며, 선택 상태는 공용 Selection Controller를 통해 공유합니다. Splitter를 드래그하면 비율을 유지하면서 각 뷰포트의 Origin과 크기를 다시 동기화합니다. 다중 레이아웃은 Free Perspective와 Top·Front·Right Orthographic 조합으로 초기화됩니다.

### Perspective Camera 저장·복원

씬 저장 시 메인 Perspective Viewport의 Camera State를 함께 저장합니다.

- Location
- Rotation
- FOV
- Near / Far Clip

Scene JSON의 `PerspectiveCamera` 필드에서 이를 읽어, 씬을 다시 열었을 때 카메라를 복원합니다.

`FEditorCameraState`에는 `OrthoHeight`가 있지만 Scene Serializer가 JSON에 기록하는 값은 위의 Perspective Camera 5개 항목입니다. Orthographic Camera 상태와 뷰포트 분할 레이아웃은 씬 저장 대상이 아닙니다.

관련 코드:

- `Editor/Source/Viewport/Window/WindowOverlayManager.cpp`
- `Editor/Source/Viewport/EditorViewportClient.cpp`
- `Engine/Source/SceneIO/SceneSerializer.cpp`

## 5. OBJ Viewer

`OBJViewerDebug | x64` 구성은 일반 에디터 대신 독립 OBJ Viewer를 실행합니다.

### 제공 기능

- OBJ 파일 열기 및 Drag & Drop
- 마우스 기반 Orbit / Pan / Zoom 카메라
- 메시 Bounding Box 기반 자동 카메라 맞춤
- Front, Back, Left, Right, Top, Bottom 카메라 정렬과 2초 Slerp 전환
- `Lit`, `Unlit`, `Wireframe` View Mode
- `None`, `Cull Back`, `Cull Front` Cull Mode
- Y-up OBJ 좌표를 엔진 Z-up 좌표로 변환하는 옵션
- 모델 스케일 조정과 FPS 표시

### 조작

- `RMB Drag`: Orbit
- `MMB Drag`: Pan
- `Mouse Wheel`: Zoom

OBJ Viewer는 `FStaticMeshLoader`, `FMaterialLoader`, `FTextureLoader`를 등록한 별도 엔진 루프에서 실행되므로, 에디터 기능과 분리해 OBJ 임포트/렌더링 결과를 빠르게 확인할 수 있습니다. 한 번에 하나의 OBJ를 검사하는 도구이며 Scene 편집·저장 기능은 포함하지 않습니다.

관련 코드:

- `Editor/Source/Viewer/ObjViewerEngineLoop.cpp`
- `Editor/Source/Viewer/OrbitalCameraController.cpp`
- `Editor/Source/Viewer/ViewerImGui.cpp`

## 6. Overlay Stat과 Memory Tracker

활성 뷰포트에 필요한 통계를 오버레이로 표시할 수 있습니다.

- `stat fps`: FPS와 Frame Time 표시/해제
- `stat memory`: 엔진 Heap, 시스템 Physical/Virtual Memory, 추적 GPU 메모리 표시/해제
- `stat none`: 활성 뷰포트의 통계를 모두 해제

GPU 메모리 추적 항목은 Texture, Render Target, Depth Stencil, Vertex Buffer, Index Buffer 및 총합으로 구성됩니다. `FMemoryTracker`는 추적 코드가 연결된 D3D11 리소스의 생성·해제 시점 바이트 수를 집계합니다. 따라서 이 값은 전체 VRAM 사용량이 아니라 엔진이 명시적으로 등록한 리소스의 추정치입니다.

`stat fps`는 Main Loop가 전달한 현재 FPS와 현재 Frame의 `DeltaTime`을 사용하며, 구간 평균이나 GPU Timing 결과는 아닙니다.

관련 코드:

- `Editor/Source/Viewport/EditorViewportClient.cpp`
- `Editor/Source/Panel/ConsolePanel.cpp`
- `Engine/Source/Renderer/MemoryTracker.cpp`

## 7. TObjectIterator

`TObjectIterator<T>`는 전역 `GUObjectArray`를 순회하면서 엔진의 `IsA` 타입 정보를 이용해 `T` 또는 파생 타입만 반환하는 템플릿 이터레이터입니다. 모든 `UObject`를 대상으로 하는 특수화도 제공해 별도 타입 검사 없이 순회할 수 있습니다.

이 기능은 엔진 객체 집합을 타입별로 탐색하기 위한 기반 구현이며, 4주차의 객체 관리 학습 항목에 대응합니다. 현재 저장소에서는 이터레이터 정의 외의 실제 호출 지점은 확인되지 않아 독립 기반 기능으로 남아 있습니다.

관련 코드:

- `Engine/Source/CoreUObject/UObjectIterator.h`
- `Engine/Source/CoreUObject/UObjectIterator.cpp`

## 빌드 및 실행

### 요구 환경

- Windows 10/11
- Visual Studio 2022와 Desktop development with C++
- Direct3D 11 지원 GPU/드라이버
- DirectXTK Desktop Win10 NuGet package `2025.10.28.2`
- 프로젝트 파일 재생성 시 Python 3

### 일반 에디터 실행

1. NuGet 패키지를 복원합니다. `Engine/packages.config`의 DirectXTK가 없으면 `Engine.vcxproj`가 참조하는 `.targets` 파일을 찾지 못해 컴파일 전에 빌드가 중단됩니다. Visual Studio에서 **Restore NuGet Packages**를 실행합니다.
2. `Kraftonjungle_Week4_Team8.sln`을 엽니다.
3. 시작 프로젝트를 `Editor`로 설정합니다.
4. `Debug | x64`를 선택해 빌드 및 실행합니다.

### OBJ Viewer 실행

1. 시작 프로젝트를 `Editor`로 설정합니다.
2. 솔루션 구성에서 `OBJViewerDebug | x64`를 선택합니다.
3. 빌드 후 실행합니다. 일반 에디터가 아니라 OBJ Viewer 창이 시작됩니다.

### 프로젝트 파일 재생성

```powershell
.\GenerateProjectFiles.bat
```

`GenerateProjectFiles.bat`은 `Scripts/GenerateProjectFiles.py`를 실행합니다. 다만 현재 Generator는 `Debug`와 `Release`만 생성하며 `OBJViewerDebug` 구성과 `IS_OBJ_VIEWER` 정의는 생성하지 않습니다. OBJ Viewer가 필요하면 저장소에 포함된 Solution/Project 파일을 사용하거나, Generator에 해당 구성을 추가한 뒤 재생성해야 합니다.

현재 체크인된 Project 기준으로 `Debug`와 `OBJViewerDebug`는 MSVC `v143`, `Release`는 `v145` Toolset을 사용합니다. 설치된 Visual Studio Toolset과 맞지 않으면 Project 속성에서 Platform Toolset을 맞춰야 합니다.

## 샘플 에셋

다음 경로에 OBJ, MTL, Texture, Bake된 `.uasset` 샘플이 포함되어 있습니다.

- `Editor/Content/Sample obj/cube/`
- `Editor/Content/Sample obj/cottage/`
- `Editor/Content/Sample obj/laevat/`
- `Editor/Content/Sample obj/Lumine/`
- `Editor/Content/Sample obj/Wolf/`

예를 들어 `cube/cube-tex.obj`와 동일 폴더의 `cube-tex.uasset`을 비교하면 텍스트 OBJ 원본과 Bake 결과를 함께 확인할 수 있습니다.

## 구현 범위와 제한 사항

- 지원 입력 포맷은 Wavefront OBJ/MTL이며 FBX는 이 주차의 범위가 아닙니다.
- OBJ Face는 1부터 시작하는 양수 Index를 전제로 합니다. 음수 Relative Index와 손상되거나 범위를 벗어난 Index에 대한 방어적 검증은 구현하지 않았습니다.
- 다각형 Face는 Fan 방식으로 삼각화하므로 오목 다각형에서 일반적인 Polygon Triangulation과 결과가 다를 수 있습니다.
- 복수 `mtllib` 선언에서는 첫 번째 라이브러리만 사용하며 `o`, `g`, `s` 같은 OBJ Directive는 별도 엔진 데이터로 보존하지 않습니다.
- MTL의 여러 속성과 Texture 경로를 파싱하지만 렌더링은 Diffuse 중심입니다. Specular Map과 Normal Map은 실제 조명 계산에 사용하지 않습니다.
- Bake된 `.uasset`은 Texture를 내장하지 않습니다. 참조한 Texture의 이동·삭제는 런타임 로딩에 영향을 줍니다.
- Material Slot Descriptor는 범용 Scene Serializer가 처리하는 Property Type에 포함되지 않아 Scene JSON에 `null`로 기록됩니다. 따라서 Component Material Override의 저장·복원은 완성된 상태가 아닙니다.
- Details의 UV Scroll 편집은 공유 `UMaterial`의 메모리 값을 바꾸며 `.uasset`에 다시 저장되지 않습니다. Component별 독립 Override 값도 아닙니다.
- Scene은 메인 Perspective Camera의 Location, Rotation, FOV, Near/Far Clip만 저장합니다. Ortho Height, 각 Orthographic Camera, Viewport Layout은 복원하지 않습니다.
- Actor Picking은 AABB 이후 후보 메시의 Triangle을 CPU에서 순회하며 BVH 같은 가속 구조를 사용하지 않습니다.
- `stat memory`의 GPU 항목은 `FMemoryTracker`에 등록된 리소스의 추정 합계이며 전체 GPU 메모리 사용량이 아닙니다.
- OBJ Viewer는 OBJ 한 개를 확인하는 도구이며 파일 대화상자도 OBJ만 대상으로 합니다.
- `TObjectIterator`는 구현되어 있으나 이 주차 코드에서 실제 사용처는 연결되어 있지 않습니다.

## License

이 프로젝트의 코드는 [MIT License](LICENSE)를 따릅니다. 서드파티 라이브러리는 각 라이선스 조건을 따릅니다.
