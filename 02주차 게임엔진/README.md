# CustomGameEngine — Week 2 3D Math, Picking & Editor Foundation

> **Model–View–Projection · Reverse-Z · Camera · Deprojection · Triangle Picking · Gizmo · Scene Serialization · UObject**

크래프톤 정글 GameTech Lab 자체 게임엔진 제작 교육의 2주차 결과물입니다. 이 문서는 2주차 과제인 **2D에서 3D로의 공간 확장**, **Primitive 생성과 렌더링**, **Camera와 Projection**, **Deprojection과 Picking**, **Transform Gizmo**, **Scene 저장·복원**, **UObject 기반 Core System** 구현에 집중합니다.

## 학습 목표

- Local, World, View, Projection 공간과 공간 사이의 변환을 이해한다.
- Vector, Matrix, Euler Rotation과 Quaternion을 이용해 3D Transform을 구현한다.
- Perspective와 Orthographic Camera를 구현하고 키보드·마우스로 월드를 탐색한다.
- Plane, Cube, Sphere 등의 Primitive를 생성하고 Direct3D 11로 렌더링한다.
- 2D Mouse 좌표를 3D Ray로 복원해 화면의 Primitive를 선택한다.
- 선택한 Primitive를 Property Panel과 Translate·Rotate·Scale Gizmo로 편집한다.
- Scene의 Primitive와 Transform을 JSON으로 저장하고 다시 구성한다.
- UObject, 사용자 정의 RTTI, Object Factory와 객체 메모리 통계의 기반을 만든다.
- ImGui Editor, Console, Grid와 Window Resize를 Engine Loop에 통합한다.

## 구현 요약

| 과제 항목 | 구현 내용 |
| --- | --- |
| 3D Math | `FVector`, `FVector4`, `FMatrix`, `FRotator`, `FQuat`과 `Scale × Rotation × Translation` Transform |
| 좌표계 | 왼손 좌표계에서 `+X` Forward, `+Y` Right, `+Z` Up 규약 사용 |
| Primitive | Cube, Sphere, Triangle, Plane의 공유 Geometry와 Immutable Vertex Buffer 생성 |
| Renderer | Frame/Object/Grid Constant Buffer, RenderObject Dirty 동기화, Reverse-Z Depth Buffer |
| Camera | Perspective·Orthographic Projection, RMB Fly Camera, Alt+LMB Orbit, Resize 시 Aspect 갱신 |
| Picking | Mouse → NDC → View → World Ray 복원, Local-space Ray-Triangle 검사와 중첩 선택 순환 |
| Gizmo | Strategy 기반 Translate·Rotate·Scale, World/Local 축, 15도 Rotation Snapping |
| Scene | Active/Permanent Scene 분리, Primitive Type·Transform·ID를 `.Scene` JSON으로 저장·복원 |
| UObject | 순차 ID, `GUObjectArray`, 사용자 정의 `IsA`·`Cast`, Object Factory, 할당량 통계 |
| Editor | Control·Property·Stats·Console·Toolbar Panel, Shader 기반 Grid와 실시간 Resize |

## 전체 편집 흐름

```text
InputManager
  ├─ Keyboard / Mouse → Camera 이동·회전
  └─ Mouse Click
        │
        ▼
Camera::ScreenPointToRay
  Screen → NDC → Inverse Projection → Inverse View
        │
        ├─ Gizmo Triangle Picking → Transform Strategy
        └─ Primitive Triangle Picking → Editor Selection
                                      │
                                      ▼
                              Property / Gizmo 편집
                                      │
                                      ▼
Scene Component → RenderObject Dirty Sync
                                      │
                                      ▼
Direct3D 11 Vertex Buffer Draw + ImGui
                                      │
                                      └─ Scene JSON 저장·복원
```

## 1. 3D Math, 좌표계와 Transform

엔진은 행 벡터 기준의 Matrix 연산을 사용하며, `USceneComponent::GetRelativeMatrix()`에서 다음 순서로 Transform Matrix를 구성합니다.

```text
Local Vertex
  → Scale Matrix
  → Quaternion Rotation Matrix
  → Translation Matrix
  → Render World Position
```

코드상 결합 순서는 `Scale × Rotation × Translation`입니다. Euler Rotation은 Roll(X), Pitch(Y), Yaw(Z)를 Matrix 또는 Quaternion으로 변환해 사용합니다.

축 규약은 다음과 같습니다.

- `+X`: Forward
- `+Y`: Right
- `+Z`: Up

`FMatrix::ViewMatrix()`는 Eye에서 Target으로 향하는 Forward, `Cross(Up, Forward)`로 구한 Right와 다시 직교화한 Up으로 왼손 View Matrix를 만듭니다.

현재 `USceneComponent`에는 Parent/Child 계층이 없습니다. 이름은 `RelativeLocation`, `RelativeRotation`, `RelativeScale3D`이지만 부모 Transform과 결합하지 않으므로 Primitive에서는 사실상 World Transform으로 사용됩니다.

관련 코드:

- `CustomGameEngine/Math/Vector.cpp`
- `CustomGameEngine/Math/Matrix.cpp`
- `CustomGameEngine/Math/Rotator.cpp`
- `CustomGameEngine/Math/Quaternion.cpp`
- `CustomGameEngine/Component/SceneComponent.cpp`

## 2. Primitive와 RenderObject

`ResourceManager`는 Engine 초기화 시 Cube, Sphere, Triangle, Plane과 Gizmo Geometry를 생성합니다. 각 Geometry는 CPU Picking에 사용할 Position 배열과 Direct3D 11 Immutable Vertex Buffer를 함께 보관합니다.

`PrimitiveFactory`는 선택한 `EPrimitiveType`에 따라 다음 Component를 Active Scene에 추가합니다.

- `UCubeComp`
- `USphereComp`
- `UTriangleComp`
- `UPlaneComp`

Component는 Geometry를 직접 복제하지 않고 Resource Manager의 공유 `FGeometry`를 참조합니다. `OnComponentAdded()`에서 `RenderObject`를 만들고 `FLevel`의 렌더 목록에 등록합니다.

```text
UPrimitiveComponent
  ├─ Transform / Type / Selection / Pickable
  ├─ shared FGeometry*
  └─ RenderObject*
        ├─ World Matrix
        ├─ Geometry / Material
        ├─ Color / Selection State
        └─ Depth / Primitive Topology
```

Transform이 변경되면 `MarkRenderStateDirty()`를 호출합니다. Render 직전 `UWorld::OnBeforeRender()`가 Dirty RenderObject만 Component 상태와 동기화합니다. Renderer는 하나의 Frame Constant Buffer와 하나의 Object Constant Buffer를 공유하며, Object마다 World Matrix·Color·선택 상태를 갱신해 Draw합니다.

현재 Primitive Geometry는 Index Buffer 없이 Triangle List 형태의 Vertex Buffer만 사용하며, Primitive마다 `Draw(VertexCount, 0)`을 한 번 호출합니다.

관련 코드:

- `CustomGameEngine/PrimitiveFactory.cpp`
- `CustomGameEngine/ResourceManager.cpp`
- `CustomGameEngine/ResourceManager.h`
- `CustomGameEngine/Component/PrimitiveComponent.cpp`
- `CustomGameEngine/Renderer/RenderObject.cpp`
- `CustomGameEngine/Renderer/Renderer.cpp`
- `CustomGameEngine/Renderer/Level.cpp`

## 3. Camera, Projection과 Reverse-Z

`UCameraComponent`는 Transform으로 Camera의 Direction, Side, Up Vector를 갱신하고 View와 Projection Matrix를 생성합니다.

### Camera 조작

- `RMB + W/S`: Camera Forward/Backward
- `RMB + A/D`: Camera Left/Right
- `RMB + Q/E`: Camera Down/Up
- `RMB + Mouse Move`: Pitch/Yaw 회전
- `Alt + LMB + Mouse Move`: 현재 Focus Point를 중심으로 Orbit

Pitch는 `-89.9°~89.9°`로 제한합니다. Orbit의 Focus Point는 매 Frame `Camera Position + Forward × FocusLength`로 계산하며, 현재 `FocusLength`는 5입니다. 선택된 Object를 자동으로 Focus하는 `F` 입력은 코드가 주석 처리되어 있습니다.

### Perspective와 Orthographic

Control Panel에서 Perspective/Orthographic을 전환하고 Camera Location, Rotation, FOV와 Move Speed를 편집할 수 있습니다.

Projection과 Depth Buffer는 Reverse-Z 규약을 사용합니다.

```text
Near Plane → NDC Depth 1
Far Plane  → NDC Depth 0
Depth Clear Value → 0
Depth Test → GREATER
```

Perspective와 Orthographic Matrix 모두 Near/Far를 이 규약으로 변환하며, Picking에서도 Near NDC를 1, Far NDC를 0으로 설정해 같은 기준을 사용합니다.

관련 코드:

- `CustomGameEngine/Component/CameraComponent.cpp`
- `CustomGameEngine/Math/Matrix.cpp`
- `CustomGameEngine/InputManager.cpp`
- `CustomGameEngine/Renderer/Renderer.cpp`

## 4. Deprojection과 Primitive Picking

`UPicker`는 Mouse 좌표를 현재 Camera와 Viewport 기준의 World Ray로 변환합니다.

```text
Mouse Pixel
  → Pixel Center 보정
  → NDC XY
  → Near(Depth 1) / Far(Depth 0)
  → Inverse Projection
  → Perspective Divide
  → Inverse View
  → World Ray
```

선택 순서는 다음과 같습니다.

1. 현재 Gizmo의 X/Y/Z Geometry와 먼저 교차 검사합니다.
2. Gizmo가 적중하지 않으면 Active Scene의 Component를 순회합니다.
3. `UPrimitiveComponent`이며 `Pickable`인 항목만 검사합니다.
4. World Ray를 Primitive의 역 Transform으로 Local Space에 옮깁니다.
5. CPU Geometry의 연속된 세 Vertex마다 Möller–Trumbore 방식의 Ray-Triangle 검사를 수행합니다.
6. 적중 결과를 거리순으로 정렬해 선택합니다.

이미 선택된 Primitive와 여러 Geometry가 겹쳐 있으면 다음 클릭에서 거리순의 다음 후보로 이동하고 마지막 후보 이후에는 첫 번째로 돌아갑니다. Bounds 기반 Broad Phase는 없으며 매 클릭마다 모든 Pickable Primitive의 Triangle을 순차 검사합니다.

선택 여부는 Object Constant Buffer의 `bIsSelected`로 전달됩니다. 과제 설명의 Vertex Shader 변형 방식과 달리 현재 `ShaderW0.hlsl`의 Pixel Shader가 선택된 색상의 Alpha를 50%로 낮춰 Highlight합니다.

관련 코드:

- `CustomGameEngine/Component/CameraComponent.cpp`
- `CustomGameEngine/Editor/Picker.cpp`
- `CustomGameEngine/Ray.cpp`
- `CustomGameEngine/Shader/ShaderW0.hlsl`

## 5. Transform Gizmo와 Property Editing

`UGizmoComponent`는 선택된 `USceneComponent`에 Attach되고, Translate·Rotate·Scale을 각각 Strategy로 분리합니다. Mode는 Toolbar Button 또는 `Space` 입력으로 전환합니다.

### Translate

Mouse Ray와 선택 Axis 직선의 최근접 Parameter를 구해 Drag 시작점과 현재점의 차이만큼 Location을 이동합니다. World Mode에서는 고정된 World Axis를, Local Mode에서는 Object Rotation을 반영한 Axis를 사용합니다.

### Rotate

선택 Axis를 화면에 투영해 구한 접선 방향과 Mouse 이동량으로 회전각을 누적합니다. Quaternion의 곱 순서를 달리해 World/Local Rotation을 구분하고, 기본 15도 단위 Snapping을 켜고 끌 수 있습니다.

### Scale

Translate와 같은 최근접점 방식으로 선택한 Local Axis의 Scale 값 하나를 변경합니다. Scale Mode는 Toolbar에서 항상 Local로 표시되며 World/Local 전환을 지원하지 않습니다.

Gizmo는 Perspective Camera에서 대상과 Camera 사이의 거리에 비례해 크기를 조정하고, Orthographic에서는 고정 비율을 사용합니다. 세 Axis의 Geometry는 CPU Triangle Picking을 사용하고 Depth 대상보다 뒤에 가려지지 않도록 일반 Primitive 이후 별도 Render Group으로 그립니다.

Property Panel에서는 선택된 Component의 Location, Rotation과 Scale을 `InputFloat3`로 직접 편집하고 삭제할 수 있습니다. Color 편집 기능은 현재 Property Panel에 연결되어 있지 않습니다.

관련 코드:

- `CustomGameEngine/Component/GizmoComponent.cpp`
- `CustomGameEngine/Editor/Gizmo/GizmoTranslationControlStrategy.cpp`
- `CustomGameEngine/Editor/Gizmo/GizmoRotationControlStrategy.cpp`
- `CustomGameEngine/Editor/Gizmo/GizmoScaleControlStrategy.cpp`
- `CustomGameEngine/Editor/EditorPropertyPanel.cpp`
- `CustomGameEngine/Editor/EditorToolbar.cpp`

## 6. Scene 저장과 복원

`UWorld`는 두 Scene을 분리해 관리합니다.

- Permanent Scene: Camera와 Gizmo처럼 Scene 교체 후에도 유지할 Editor Component
- Active Scene: 사용자가 생성·편집·저장하는 Primitive Component

Control Panel의 Scene Name은 저장 파일명으로 사용되며 Scene JSON 내부의 이름으로 저장되지는 않습니다.

`.Scene` JSON에는 다음 정보가 들어갑니다.

- `Version`
- 다음 순차 ID를 위한 `NextUUID`
- ID를 Key로 사용하는 `Primitives` Object
- Primitive의 `Type`, `Location`, `Rotation`, `Scale`

```text
UWorld::SaveScene
  → Active Scene의 NextUUID 갱신
  → UScene::ToJson
  → UUID 순서로 Component 정렬·직렬화
  → {SceneName}.Scene 저장

UWorld::LoadScene
  → JSON 파일 읽기
  → UScene Factory 생성
  → Type별 Component 생성(Object Factory)
  → Transform과 저장 ID 복원
  → 기존 Active Scene 교체
```

Camera, Gizmo, 선택 상태, Render Resource와 Material은 Scene에 저장하지 않습니다. Primitive 복원 시 Type에 맞는 Component가 공유 Resource를 다시 참조하고 RenderObject를 등록합니다.

관련 코드:

- `CustomGameEngine/World.cpp`
- `CustomGameEngine/Scene.cpp`
- `CustomGameEngine/SerializeHelper.h`
- `CustomGameEngine/Component/SceneComponent.cpp`
- `CustomGameEngine/Component/PrimitiveComponent.cpp`

## 7. UObject, RTTI와 Object Factory

`UObject`는 Engine Object의 공통 기반입니다.

### Object ID와 전역 배열

생성된 UObject는 `UEngineStatics::GenUUID()`에서 증가하는 `uint32` ID와 `GUObjectArray`의 `InternalIndex`를 받습니다. 객체가 파괴되면 해당 배열 Slot을 `nullptr`로 바꿉니다.

이 ID는 RFC 규격의 UUID가 아니라 현재 Process와 Scene에서 사용하는 순차 정수 식별자입니다. New Scene에서는 Counter를 0으로 초기화하고, Scene Load에서는 저장된 `NextUUID`로 복원합니다.

### 사용자 정의 RTTI

`DECLARE_RTTI`는 클래스별 정적 주소를 Type ID로 사용하고, 부모의 `IsA()`를 재귀 호출하는 구조를 생성합니다. `Cast<T>()`는 `IsA()` 검사 후 Pointer를 변환합니다.

`REGISTER_CLASS`는 Type ID와 기본 생성 Lambda를 `FObjectFactory`에 등록합니다. World의 Component 추가와 Scene 복원이 이 Factory를 통해 UObject 파생 객체를 생성합니다.

### 메모리 통계

`UObject::operator new/delete`가 UObject 계열의 현재 Allocation Count와 Byte를 `UEngineStatics`에 기록하고 Stats Panel이 이를 표시합니다. 별도 Heap Allocator가 아니라 기본 `::operator new/delete`를 감싼 통계 기능입니다.

관련 코드:

- `CustomGameEngine/Object.h`
- `CustomGameEngine/Object.cpp`
- `CustomGameEngine/ObjectFactory.cpp`
- `CustomGameEngine/EngineStatics.cpp`
- `CustomGameEngine/EngineTypes.h`
- `CustomGameEngine/Editor/EditorStatPanel.cpp`

## 8. ImGui Editor와 Console

Editor는 다음 Panel로 구성됩니다.

- Control Panel: FPS, Primitive 생성 수량, Scene New/Save/Load, Camera 설정
- Property Panel: 선택 Component의 Transform 편집과 삭제
- Stats Panel: UObject Allocation Count와 Byte
- Toolbar: Gizmo Mode, World/Local Mode, Rotation Snapping
- Console: Engine Log, Text Filter, Auto Scroll, Copy, Command History와 자동 완성

Console은 `CLEAR`, `HELP`, `HISTORY` 명령을 처리합니다. `CLASSIFY`는 자동 완성 목록에는 있지만 실행 분기가 없어 입력하면 Unknown Command로 처리됩니다. `Logger::Bind()`로 전달된 Engine Log도 Console 항목에 추가됩니다.

관련 코드:

- `CustomGameEngine/Editor/Editor.cpp`
- `CustomGameEngine/Editor/EditorControlPanel.cpp`
- `CustomGameEngine/Editor/EditorPropertyPanel.cpp`
- `CustomGameEngine/Editor/EditorStatPanel.cpp`
- `CustomGameEngine/Editor/EditorToolbar.cpp`
- `CustomGameEngine/Editor/EditorConsole.cpp`
- `CustomGameEngine/Logger.cpp`

## 9. Shader Grid와 Window Resize

`ShaderInfiniteGrid.hlsl`은 Vertex Buffer 없이 `SV_VertexID`로 18개 Vertex를 생성합니다.

- 6 Vertex: Camera의 XY 위치를 따라가는 Z=0 바닥 Quad 두 개의 Triangle
- 6 Vertex: Z축을 표현하는 첫 번째 얇은 수직 Quad
- 6 Vertex: Z축을 다른 각도에서도 보이게 하는 두 번째 수직 Quad

Pixel Shader는 `frac`, `fwidth`, `smoothstep`으로 XY Grid Line과 X/Y Axis를 만들고, Camera 거리와 화면 미분값으로 Alpha를 감쇠합니다. Grid Quad의 범위는 Camera 중심에서 축별 약 1,000 Unit이며, Camera를 따라 이동해 시각적으로 계속 이어지는 Grid를 표현합니다.

이 구현은 전체 화면을 덮는 단일 대형 Triangle 방식이 아닙니다. 바닥은 두 Triangle으로 만든 큰 World-space Quad이고, 나머지 Triangle은 Z Axis 표시에 사용됩니다.

Win32 `WM_SIZE`가 발생하면 Camera와 Renderer에 새 Client 크기를 전달합니다.

1. Camera의 Screen Width, Height와 Aspect Ratio를 갱신합니다.
2. 기존 Render Target과 Depth Stencil Resource를 해제합니다.
3. Swap Chain Buffer를 Resize합니다.
4. Render Target과 Depth Stencil을 다시 생성합니다.
5. D3D11 Viewport를 새 크기로 갱신합니다.

관련 코드:

- `CustomGameEngine/Shader/ShaderInfiniteGrid.hlsl`
- `CustomGameEngine/Renderer/Renderer.cpp`
- `CustomGameEngine/EngineApp.cpp`
- `CustomGameEngine/Component/CameraComponent.cpp`

## 빌드 및 실행

### 요구 환경

- Windows 10/11
- Visual Studio 2022와 MSVC v143 Build Tools
- Windows 10 SDK
- Direct3D 11 지원 GPU
- C++20: x64 구성 기준
- Direct3D Debug Layer: 현재 Device 생성 Flag에 항상 포함

### 실행 방법

1. `CustomGameEngine.sln`을 Visual Studio에서 엽니다.
2. `Debug | x64` 또는 `Release | x64`를 선택합니다.
3. `CustomGameEngine` 프로젝트를 빌드하고 실행합니다.
4. Working Directory를 `CustomGameEngine` 폴더로 설정합니다.

Shader와 Scene을 상대 경로로 읽으므로 다음 파일들이 Working Directory 기준으로 접근 가능해야 합니다.

- `Shader/ShaderW0.hlsl`
- `Shader/ShaderInfiniteGrid.hlsl`
- `{SceneName}.Scene`

## 샘플 Scene

- `CustomGameEngine/Default.scene`: Cube, Triangle, Sphere로 구성한 기본 Scene
- `CustomGameEngine/Test.scene`: 여러 Cube·Sphere·Triangle과 Plane으로 구성한 편집 예제

## 구현 범위와 제한 사항

- Scene Component 계층과 Parent Transform이 없어 `Relative Transform`이 사실상 World Transform으로 사용됩니다.
- Primitive는 Index Buffer와 Draw Batching 없이 Object별 Vertex Buffer와 Draw Call을 사용합니다.
- Picking은 Bounds 또는 공간 가속 구조 없이 모든 Pickable Primitive의 모든 Triangle을 순차 검사합니다.
- 선택 Highlight는 과제 설명의 Vertex Shader 방식이 아니라 Pixel Shader Alpha 감소로 구현되어 있습니다.
- Camera Orbit은 선택 Object가 아니라 현재 Camera의 고정 Focus Length 지점을 중심으로 동작하며, `F` Focus 기능은 구현되어 있지 않습니다.
- Perspective Projection은 `M[0][0] = 1/tan(FOV/2)`, `M[1][1] = M[0][0] × Aspect`로 계산합니다. 일반적인 Aspect 보정식과 달라 비정방형 Viewport의 화면 비율은 추가 검증이 필요합니다.
- `WorldToScreen()`은 Homogeneous `w`를 유지하지 않고 3차원 `TransformPoint()` 결과를 `z`로 나눕니다. 이를 사용하는 Rotation Gizmo의 화면 접선 계산은 Camera 각도에 따라 오차가 생길 수 있습니다.
- Grid는 이름과 달리 무한 Geometry가 아니라 Camera를 따라가는 약 2,000×2,000 Unit의 두 Triangle Quad이며, Full-screen Single Triangle 방식은 적용하지 않았습니다.
- Scene은 Primitive Type과 Transform만 저장합니다. Camera, Gizmo, 선택 상태, Color, Material과 Component 계층은 저장하지 않습니다.
- `UScene::Version`은 새 Scene 생성 시 명시적으로 초기화하지 않고, Load 시에도 Version별 호환 분기가 없습니다.
- Object ID는 Process-local 순차 `uint32`이며 New Scene마다 0으로 초기화되므로 전역적으로 유일한 UUID가 아닙니다.
- `GUObjectArray`는 삭제된 Slot을 `nullptr`로 남기며 배열 압축이나 Index 재사용을 수행하지 않습니다.
- UObject 메모리 통계는 UObject 계열 Allocation만 집계하며 별도의 Heap 관리·Memory Pool·Leak 검사는 포함하지 않습니다.
- `RenderObject::bIsDirty`는 생성 시 명시적으로 초기화되지 않아 첫 Render 전 상태가 결정적이지 않습니다.
- Console의 `CLASSIFY` 명령은 목록에만 등록되어 있고 실행 기능은 없습니다.
- D3D11 Device 생성에 Debug Flag를 항상 사용하고 Shader Compile·Device 생성 실패를 충분히 검사하지 않으므로, Debug Layer가 없거나 HLSL 경로가 잘못되면 초기화에 실패할 수 있습니다.
- Undo/Redo, Multi-selection과 Scene Hierarchy 편집은 구현 범위에 포함되지 않습니다.

## Third-party licenses

ImGui와 `fifo_map` 관련 라이선스 고지는 [CustomGameEngine/LICENSE.txt](CustomGameEngine/LICENSE.txt)에 포함되어 있습니다.
