# NipsEngine — Week 5+ Play In Editor

> **PIE · Multi-World Lifecycle · Actor/Component Pattern · Text Render · Billboard · Multi-Viewport**

크래프톤 정글 GameTechLab 자체 게임엔진 제작 교육의 5+주차 결과물입니다. 에디터에서 구성한 World를 실행용 World로 복제하고, Play·Pause·Stop 상태에 따라 실행 수명 주기를 제어하는 **PIE(Play In Editor)** 를 구현했습니다. 이 과정에서 `UWorld`–`ULevel`–`AActor`–`UActorComponent`의 소유 관계와 복제 경로를 정리하고, Text Render·Billboard Component 및 4분할 Viewport를 에디터 흐름에 통합했습니다.

이 문서는 과제 명세뿐 아니라 현재 저장소의 실제 상태 전환, 복제 함수, Render Collector와 Editor UI 연결을 기준으로 작성했습니다.

## 학습 목표

- Editor World와 PIE World를 분리하고 여러 World의 생성·활성화·파괴 흐름을 이해한다.
- `UWorld`, `ULevel`, `AActor`, `UActorComponent`의 소유 관계와 실행 생명주기를 구현한다.
- Component Pattern을 이용해 Actor의 기능을 조합하고 확장한다.
- 에디터에서 배치한 Actor를 복제 World에서 독립적으로 실행하는 PIE를 구현한다.
- Text Render와 Billboard를 Primitive Component 및 Editor Property 시스템에 연결한다.

## 구현 요약

| 영역 | 구현 내용 |
| --- | --- |
| PIE 상태 제어 | Edit → Play → Pause/Resume → Stop 흐름과 Toolbar UI |
| 다중 World | `FWorldContext` 목록과 Handle 기반 Active World 전환 |
| PIE World 복제 | World → Persistent Level → Actor → Component 순서의 복제와 참조 재구성 |
| 실행 생명주기 | PIE 시작 시 `BeginPlay`, Play 중 `Tick`, 종료 시 `EndPlay(EndPlayInEditor)` |
| Actor/Component | Root/Owned Component, Attach 계층, 활성화, Tick, World 등록·해제 |
| Text Render | Font Atlas 기반 World-space Text, UTF-8 글자 처리, AABB와 Raycast |
| Billboard | Viewport Camera를 향하는 Texture Quad, 크기 편집, AABB와 Raycast |
| Multi-Viewport | Perspective / Top / Front / Right 4분할 및 단일 Viewport 전환 |

## 전체 구조

```text
UEngine
 ├─ WorldList : FWorldContext[]
 └─ ActiveWorldHandle
      │
      ├─ Editor Context
      │   └─ UWorld
      │       └─ Persistent ULevel
      │           └─ AActor[]
      │               └─ UActorComponent[]
      │
      └─ PIE Context
          └─ 복제된 UWorld / ULevel / Actor / Component

UEditorEngine
 ├─ EditorState : Edit / Play / Pause
 ├─ FViewportLayout : 최대 4개 FEditorViewportClient
 └─ FEditorRenderPipeline : Active World를 Viewport별로 수집·렌더링
```

`FWorldContext`는 World Type, World Pointer, 이름과 Handle을 보유합니다. `UEngine`은 Context 목록과 Active Handle을 통해 현재 Tick·Render 대상 World를 찾습니다. `UWorld`는 Persistent Level과 Spatial Index를, `ULevel`은 Actor 배열을, `AActor`는 Root Component와 Owned Component를 소유합니다.

관련 코드:

- `NipsEngine/Source/Engine/GameFramework/WorldContext.h`
- `NipsEngine/Source/Engine/Runtime/Engine.cpp`
- `NipsEngine/Source/Engine/GameFramework/World.cpp`
- `NipsEngine/Source/Engine/GameFramework/Level.cpp`
- `NipsEngine/Source/Engine/GameFramework/AActor.cpp`

## 1. PIE: Editor World와 실행 World 분리

PIE의 핵심은 편집 중인 World를 직접 Tick하지 않는 것입니다. Play를 누르면 현재 Editor World를 복제하고 Active World와 네 Viewport의 참조를 PIE World로 교체합니다. 실행 중 Transform 변화와 Component Tick은 복제본에만 반영되므로 Stop 이후 원래 Editor World로 돌아갈 수 있습니다.

```text
[Edit]
  │ Play
  ▼
Editor World 복제
  ├─ UWorld / Persistent Level 복제
  ├─ Actor / Component 복제
  ├─ Owner / Root / Attach Parent 재연결
  └─ PIE World Spatial Index 재구축
  │
  ▼
PIE Context 등록 · Active World 전환
  ├─ 모든 ViewportClient를 PIE World에 연결
  ├─ Selection 초기화
  └─ PIE World BeginPlay
  │
  ├─ Pause ──► Tick 정지 · Render 유지
  │      └─ Resume ──► 기존 PIE World Tick 재개
  │
  └─ Stop
      ├─ EndPlay(EndPlayInEditor)
      ├─ PIE World와 Context 제거
      ├─ Editor World를 다시 활성화
      └─ Viewport와 Selection 복구
```

### 상태별 실제 동작

| 상태/전환 | 실제 동작 |
| --- | --- |
| Edit → Play | Editor World를 복제하고 PIE Context를 추가한 뒤 `BeginPlay` 호출 |
| Play | `UEditorEngine::Tick`에서 Active PIE World의 `WorldTick` 실행 |
| Play → Pause | 상태만 Pause로 변경하여 World Tick 중단 |
| Pause → Play | World를 다시 복제하지 않고 기존 PIE 세션 재개 |
| Play/Pause → Stop | PIE World에 `EndPlayInEditor` 전달 후 파괴하고 Editor World로 복귀 |

Pause 중에도 Render Pipeline과 Editor UI는 계속 동작하므로 정지된 PIE World를 네 Viewport에서 확인할 수 있습니다. Play/Pause에서는 편집용 Gizmo와 Selection Mask 수집을 건너뛰며, ViewportClient는 에디터 Camera 조작과 선택 입력을 처리하지 않습니다. `Esc`로 PIE를 종료하고 `Shift+F1`로 Cursor Lock을 해제할 수 있습니다.

관련 코드:

- `NipsEngine/Source/Editor/EditorEngine.cpp`
- `NipsEngine/Source/Editor/UI/EditorPlayStreamWidget.cpp`
- `NipsEngine/Source/Editor/Viewport/EditorViewportClient.cpp`
- `NipsEngine/Source/Editor/EditorRenderPipeline.cpp`

## 2. World·Level·Actor·Component 복제

PIE World는 하나의 메모리 복사로 만들지 않고 객체 계층을 단계별로 복제합니다.

```text
UWorld::Duplicate
  └─ 새 UWorld 생성, World 속성 복사
      │
      └─ UWorld::DuplicateSubObjects
          └─ ULevel::Duplicate
              └─ 각 AActor::Duplicate
                  ├─ 각 Component의 가상 Duplicate 호출
                  ├─ Owner와 Root Component 지정
                  └─ 원본→복제 Component Map으로 Attach Parent 복원
```

`AActor::Duplicate`는 원본 Scene Component와 복제 Scene Component의 대응표를 만든 뒤 Root와 부모-자식 Attach 관계를 재구성합니다. `UGizmoComponent::Duplicate`는 `nullptr`를 반환하므로 Editor 전용 Gizmo는 PIE Actor 구성에서 제외됩니다.

복제된 Actor는 새 PIE World를 소유 World로 지정받고 Primitive Component를 새 Spatial Index에 등록합니다. 마지막에는 `RebuildSpatialIndex`를 호출해 렌더링·Frustum Culling·Picking에 사용하는 Bounds 구조를 복제 World 기준으로 다시 구성합니다.

Static Mesh, Font, Texture 등의 Asset Resource는 `FResourceManager`가 소유하므로 Component 복제본은 같은 Resource Pointer를 공유합니다. 반면 Transform, Visibility, Text, Material Override 등 실행 중 바뀔 수 있는 Component 상태는 각 `Duplicate` 구현에서 값으로 복사합니다.

## 3. Actor / Component Pattern과 생명주기

`AActor`는 위치·회전·크기의 기준이 되는 Root Scene Component와 기능을 구성하는 Owned Component 배열을 가집니다. Component를 추가하면 Owner를 설정하고 Primitive Cache와 World Spatial Index 등록 상태를 갱신합니다.

```text
Actor 생성
  → Component 생성 / Owner 지정
  → Root 지정 또는 기존 Root에 Attach
  → World 등록
  → BeginPlay
  → Actor Tick → 활성 Component ExecuteTick
  → EndPlay
  → World 등록 해제 / 객체 파괴
```

- `UActorComponent`: Active, Auto Activate, Tick Enabled와 Owner 관리
- `USceneComponent`: Relative Transform, Parent/Child Attach 계층과 World Transform 계산
- `UPrimitiveComponent`: Visibility, World AABB, Raycast와 렌더 Primitive Type 제공
- `AActor`: Component 소유, Root Transform, 생명주기 전파와 Primitive Cache 관리
- `UWorld`: Actor Spawn/Destroy, BeginPlay/Tick/EndPlay와 Spatial Index 관리

이미 `BeginPlay`가 끝난 World에서 `SpawnActor`를 호출하면 새 Actor에도 즉시 `BeginPlay`를 전달합니다. Actor 제거 시에는 `EndPlay(Destroyed)`를 먼저 호출한 뒤 Level과 Spatial Index에서 해제하고 UObject를 파괴합니다.

Property 패널의 **Add Component** 메뉴에서는 Static Mesh, SubUV, Text Render, Billboard Component를 추가할 수 있습니다. Actor에 Root가 없으면 새 Component를 Root로 지정하고, Root가 있으면 그 아래에 Attach합니다.

현재 기본 `AActor::Tick`에는 Root Component를 초당 90도씩 Z축 회전시키는 동작이 들어 있습니다. 따라서 PIE를 시작하면 활성 Actor가 회전하는 것으로 Editor World와 PIE World의 분리 및 Tick 동작을 확인할 수 있습니다.

관련 코드:

- `NipsEngine/Source/Engine/Component/ActorComponent.cpp`
- `NipsEngine/Source/Engine/Component/SceneComponent.cpp`
- `NipsEngine/Source/Engine/Component/PrimitiveComponent.cpp`
- `NipsEngine/Source/Editor/UI/EditorPropertyWidget.cpp`

## 4. Text Render Component

`UTextRenderComponent`는 Mesh Asset 없이 Font Atlas와 Dynamic Vertex/Index Buffer로 World-space Text를 렌더링하는 Primitive Component입니다.

### 렌더링 경로

```text
UTextRenderComponent
  → FRenderCollector가 Font Command 생성
  → FFontBatcher가 UTF-8 Codepoint 해석
  → Font Atlas UV와 글자별 Quad 생성
  → Alpha Blend로 Font Pass 렌더링
```

`FFontBatcher`는 UTF-8 1~4 Byte Sequence를 Codepoint로 변환합니다. 현재 Font Atlas Mapping은 ASCII 32~126과 한글 완성형 `가`~`힣` 범위를 구성하며, 문자열의 실제 문자 수를 기준으로 시작 Cursor를 조정해 텍스트를 가운데에 배치합니다.

Property 패널에서 실제 편집 가능한 값은 다음과 같습니다.

- Text
- Font
- Font Size
- Visible
- Scene Component의 Location / Rotation / Scale

Text는 Component의 World Matrix에서 Right/Up Vector를 얻어 배치되므로 Component Transform을 따릅니다. `ATextRenderActor`는 Text Render Component를 Root로 만들고, UUID를 표시하는 두 번째 Text Component를 자식으로 Attach합니다.

Picking을 위해 UTF-8 문자 수와 고정 글자 폭·높이로 World AABB를 근사하고, 같은 범위를 Local Quad로 변환해 Raycast합니다. Font Size와 실제 Glyph 폭을 정밀 측정하는 방식은 아닙니다.

관련 코드:

- `NipsEngine/Source/Engine/Component/TextRenderComponent.cpp`
- `NipsEngine/Source/Engine/Render/FontBatcher.cpp`
- `NipsEngine/Source/Engine/Render/Scene/RenderCollector.cpp`

## 5. Billboard Component

`UBillboardComponent`는 Texture를 입힌 Quad가 Camera를 향하도록 World Matrix를 재구성합니다. Texture, Width, Height를 Property로 편집할 수 있고 Plane Raycast와 Camera 방향을 반영한 AABB를 제공하므로 일반 Primitive처럼 배치하고 선택할 수 있습니다.

### Multi-Viewport에서의 Camera 기준

렌더링과 Picking은 Camera를 선택하는 경로가 다릅니다.

- **렌더링**: `FRenderCollector`가 현재 렌더 중인 Viewport의 Camera Forward/Right/Up으로 Billboard Matrix를 만듭니다. 같은 Billboard도 Perspective·Top·Front·Right Viewport에서 각각 해당 Camera를 바라봅니다.
- **AABB/Raycast**: Component가 `UWorld::ActiveCamera`를 조회합니다. Active Camera는 마지막으로 클릭하거나 조작한 Viewport Camera로 갱신됩니다.

Billboard Command는 전용 Texture SRV와 크기를 담아 SubUV Render Pass와 Batcher를 재사용합니다. Texture가 없으면 기본 White Texture를 사용합니다. `ABillboardActor`는 Billboard를 Root로 만들고 Owner UUID를 표시하는 Text Render Component를 자식으로 Attach합니다.

Property 패널에는 `Play Rate`와 `bLoop`도 노출되어 있지만, 현재 Billboard 렌더 경로는 Frame Index 0, 1×1 Atlas로 고정되어 있습니다. 실제 Sprite Sheet Animation은 `USubUVComponent` 경로에서 지원합니다.

관련 코드:

- `NipsEngine/Source/Engine/Component/BillboardComponent.cpp`
- `NipsEngine/Source/Engine/Render/SubUVBatcher.cpp`
- `NipsEngine/Source/Engine/Render/Renderer/Renderer.cpp`
- `NipsEngine/Source/Engine/GameFramework/PrimitiveActors.cpp`

## 6. 4분할 Multi-Viewport

`FViewportLayout`은 네 개의 `FEditorViewportClient`, `FSceneViewport`, Camera와 Viewport State를 관리합니다.

| Viewport | 초기 관점 | 초기 배치 |
| ---: | --- | --- |
| 0 | Perspective | 좌상단 |
| 1 | Top | 우상단 |
| 2 | Front | 좌하단 |
| 3 | Right | 우하단 |

레이아웃은 세로 Splitter와 위·아래 가로 Splitter를 조합한 2×2 구조입니다. 교차점과 Splitter를 드래그해 크기를 조절할 수 있고, 단일 Viewport와 4분할 모드를 전환할 수 있습니다. Splitter 비율, 활성 Viewport 수와 단일 Viewport Index는 Editor Settings에 저장됩니다.

각 Viewport는 독립적인 Camera·View Mode·Render Rect를 가집니다. Render Pipeline은 네 Viewport를 순서대로 순회하고, 크기가 0인 비활성 Viewport를 건너뜁니다. 각 Viewport Camera로 Frustum Culling과 Billboard 방향을 다시 계산한 뒤 하나의 Host Render Target에서 해당 Sub-Viewport 영역만 렌더링합니다.

PIE 시작과 종료 시 네 ViewportClient의 World 참조를 각각 PIE World와 Editor World로 일괄 교체합니다. Active World만 바꾸고 Viewport가 이전 World를 계속 참조하는 문제를 방지하기 위한 처리입니다.

관련 코드:

- `NipsEngine/Source/Editor/Viewport/ViewportLayout.cpp`
- `NipsEngine/Source/Editor/Viewport/EditorViewportClient.cpp`
- `NipsEngine/Source/Editor/EditorRenderPipeline.cpp`

## 기능 확인 방법

1. Edit 상태에서 Static Mesh, Text Render, Billboard Actor를 배치합니다.
2. Property 패널에서 Component를 추가하고 Root/Child Transform 관계를 구성합니다.
3. Play를 눌러 Actor의 회전과 Component Tick이 PIE World에서 실행되는지 확인합니다.
4. Pause/Resume으로 Render는 유지하면서 World Tick만 멈추고 재개되는지 확인합니다.
5. 네 Viewport에서 Billboard가 각 Camera를 향하는지 확인합니다.
6. Stop 후 Actor Transform이 Play 전 Editor World 상태로 복원되는지 확인합니다.

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

- PIE는 Editor World를 복제한 별도 World에서 실행하며, 플레이 중 변경을 Editor World에 반영하지 않습니다.
- 실제 상태 전환은 Edit, Play, Pause와 Stop/Resume 흐름입니다.
- Play 상태에서만 World Tick을 실행하고 Pause 상태에서는 같은 PIE World를 계속 렌더링합니다.
- World 복제 시 Owner, Root Component, Scene Component Attach Parent와 Actor의 World 참조를 복원합니다.
- Text Render는 현재 World-space Font Rendering, UTF-8 Codepoint 처리, AABB와 Raycast를 지원합니다.
- Billboard는 Viewport별 Camera-facing Rendering과 Active Camera 기준 AABB/Raycast를 지원합니다.
- Multi-Viewport는 Perspective와 세 Orthographic View를 4분할 또는 단일 화면으로 제공합니다.

### 제한 사항

- `EEditorState::Simulate`는 Enum에만 선언되어 있고 Toolbar 전환, 별도 Tick 정책과 실행 동작은 연결되어 있지 않습니다.
- `AActor::Duplicate`는 항상 기본 `AActor`를 생성하며 파생 Actor 클래스가 이를 Override하지 않습니다. 따라서 PIE 복제본은 Component 구성은 유지하지만 원본 파생 Actor 타입과 Actor별 Override 동작은 보존하지 못합니다.
- 복제 Fix-up은 Owner, Root와 복제 대상 Scene Component 사이의 Attach Parent에 집중되어 있습니다. 임의의 Actor 간 참조나 Component 내부의 일반 Object Pointer를 복제본으로 자동 Remap하는 범용 Reference Graph는 없습니다.
- `AActor::Tick`의 Z축 회전은 모든 활성 Actor에 적용되는 데모 동작이며, Actor 종류별 Gameplay 로직이나 별도 Movement Component로 분리되어 있지 않습니다.
- PIE는 별도 Game Window나 Player Controller를 만들지 않고 기존 Editor Viewport에 복제 World를 연결합니다. Play 중 Editor Camera 조작과 Picking 입력도 제한됩니다.
- PIE 시작 시 Cursor 숨김 호출은 비활성화되어 있어 게임 입력용 Cursor Capture가 자동으로 수행되지 않습니다.
- Text Render의 `Screen` 공간, 수평·수직 정렬과 Screen Position API는 선언·복제되지만 현재 Render Collector와 Property UI에서 사용되지 않습니다. 활성 경로는 World-space Text만 렌더링합니다.
- Text Color는 내부 데이터와 Render Command에는 반영되지만 현재 Property 패널에는 노출되어 있지 않습니다.
- Text AABB/Raycast는 고정 문자 폭·높이로 근사하므로 Font Size, Glyph별 실제 폭과 완전히 일치하지 않을 수 있습니다.
- Billboard의 `Play Rate`, `FrameIndex`, `bLoop`는 필드와 일부 Property만 존재하며 Frame 진행 로직은 없습니다. Renderer도 Billboard를 1×1 Texture의 0번 Frame으로 고정합니다.
- `UBillboardComponent::Duplicate`는 Texture, Transform, Visibility와 Billboard 여부만 복사하고 Width, Height, Play Rate, Loop, Frame 상태는 복사하지 않아 PIE에서 해당 값이 기본값으로 돌아갑니다.
- Billboard의 렌더 방향은 Viewport별 Camera를 사용하지만 AABB와 Raycast는 마지막으로 포커스한 Active Camera를 사용하므로, 다른 Viewport에서의 표시 Quad와 선택 Bound가 정확히 일치하지 않을 수 있습니다.
