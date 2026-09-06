# CO-PASS Engine — Week 3 Sprite, Batch Rendering & Scene Management

> **Texture Atlas · Sprite · SubUV · Real-time Text · Batch Line · Index Buffer · AABB · FName · UObject · Scene Manager**

크래프톤 정글 GameTech Lab 자체 게임엔진 제작 교육의 3주차 결과물입니다. 이 문서는 3주차 과제인 **Texture Atlas와 Sprite 렌더링**, **SubUV 애니메이션**, **실시간 Text**, **Batch Line**, **Index Buffer**, **AABB**, **Scene 관리**, **FName과 UObject 기반 구조** 구현에 집중합니다.

![CO-PASS Logo](Editor/Resources/Tool/copass.png)

## 학습 목표

- Texture Mapping의 UV 좌표와 Vertex/Pixel Shader의 Texture Sampling 흐름을 이해한다.
- Texture Atlas의 일부 영역을 사용하는 Sprite와 카메라를 향하는 Billboard를 구현한다.
- Atlas Frame을 시간에 따라 교체하는 SubUV 애니메이션을 구현한다.
- Font Atlas로 한글을 포함한 실시간 문자열을 렌더링한다.
- 여러 Line 요청을 동적 버퍼에 모아 한 번에 제출하는 Batch Renderer를 구현한다.
- Vertex와 Index의 관계를 이해하고 Index Buffer로 중복 정점을 재사용한다.
- Primitive의 AABB를 계산해 Debug 표시와 Picking에 활용한다.
- Scene이 Actor와 Component의 수명, Tick, Render Data를 관리하도록 구성한다.
- FName, UObject, 사용자 정의 RTTI와 Factory를 엔진의 객체 구조에 연결한다.
- 좌표계, View Mode, Show Flag, 카메라 설정과 창 크기 변경을 에디터에 통합한다.

## 구현 요약

| 과제 항목 | 구현 내용 |
| --- | --- |
| 좌표계 | 왼손 좌표계를 유지하면서 `+X` Forward, `+Y` Right, `+Z` Up 규약을 엔진 전반에 적용 |
| Sprite | Texture Sprite, Grid Atlas, JSON SubUV, Billboard를 동적 Vertex/Index Buffer로 Batch Rendering |
| SubUV | TexturePacker 계열 JSON의 Frame Rectangle을 UV로 변환하고 Component FPS에 따라 Frame 갱신 |
| Real-time Text | BMFont 계열 Font Atlas, UTF-8 디코딩, 한글 Glyph, 개행·자간·행간을 지원하는 Text Batch Renderer |
| Batch Line | Grid, World Axis, AABB 등의 Line 정점을 동적 Vertex Buffer에 누적하고 용량 또는 Frame 끝에서 Flush |
| Indexed Draw | Primitive는 16-bit Index Buffer, Sprite와 Text는 32-bit 동적 Index Buffer로 `DrawIndexed` 수행 |
| AABB / Picking | World AABB 계산과 12개 Edge 표시, CPU Ray-AABB Actor 선택, Object ID 기반 Gizmo 선택 |
| Scene | Actor 소유·Tick·Render Data 수집, JSON Scene 저장·복원, Type Registry와 Unknown Placeholder 지원 |
| Core Object | FName String Table, UObject UUID/전역 배열/메모리 통계, `IsA`·`Cast`·Factory 기반 사용자 정의 RTTI |
| Editor | Scene·Editor Show Flag, Lit/Unlit/Wireframe UI, INI 설정 저장, `WM_SIZE` Resize 대응 |

## 전체 구조

```text
Actor / Component
      │
      ├─ Primitive Component ── Vertex/Index Buffer
      ├─ Sprite Component ───── Texture / Atlas / SubUV
      └─ Text Component ─────── Font Atlas / UTF-8 Text
      │
      ▼
FScene::Tick + FScene::BuildRenderData
      │
      ├─ Mesh Render Item
      ├─ Sprite Render Item
      ├─ Text Render Item
      └─ AABB 표시 상태
      │
      ▼
Renderer Module
      ├─ Primitive Pass
      ├─ Grid / Axis / AABB Line Pass
      ├─ Selection Outline Pass
      ├─ Sprite / SubUV Pass
      ├─ Text Pass
      └─ Editor Overlay / Gizmo Pass
```

## 1. 좌표계와 렌더링 기반

엔진 내부 좌표계는 Direct3D의 왼손 좌표계를 유지하면서 Unreal Engine과 유사한 축 의미를 사용합니다.

- `+X`: Forward
- `+Y`: Right
- `+Z`: Up

카메라의 이동·View Matrix, Actor Transform, Billboard 기준축과 World Axis 표시가 이 규약을 공유합니다. `FScene::BuildRenderData()`가 Scene의 Actor와 Component를 순회해 렌더 항목을 만들고, Renderer Module이 Primitive부터 Editor Overlay까지 정해진 순서로 제출합니다.

View Mode UI는 `Lit`, `Unlit`, `Wireframe`을 구분합니다. 현재 Primitive Shader는 Base Color를 출력하는 Unlit 경로이며, `Lit`과 `Unlit`은 같은 결과를 냅니다. `Wireframe`에서만 Rasterizer State가 바뀌고, 선택된 Primitive는 별도 색상으로 구분됩니다.

관련 코드:

- `Engine/Source/Core/Math/Vector.h`
- `Engine/Source/Core/Math/Matrix.cpp`
- `Engine/Source/Engine/Scene.cpp`
- `Engine/Source/Renderer/RendererModule.cpp`
- `Engine/Source/Renderer/D3D11/D3D11MeshBatchRenderer.cpp`

## 2. Texture Atlas, Sprite와 SubUV

### 2.1 Sprite와 Billboard

`USpriteComponent`는 Texture 경로, 색상, Billboard 여부와 Offset을 보관합니다. Sprite Batch Renderer는 Sprite마다 4개 Vertex와 6개 Index로 Quad를 만들고 같은 Texture와 배치 방식을 사용하는 항목을 묶어 `DrawIndexed`합니다.

- 일반 Sprite는 Component의 World Forward/Up 축으로 Quad를 구성합니다.
- Billboard Sprite는 Camera의 World Right/Up 축을 사용해 항상 카메라를 향합니다.
- 투명 Sprite는 Camera와의 깊이를 기준으로 먼 항목부터 정렬합니다.
- Texture 또는 Resource가 없으면 Debug Color를 적용한 대체 Texture를 사용합니다.

`UAtlasComponent`는 Atlas를 Row와 Column의 균일한 Grid로 정의합니다. 이를 상속한 `USubUVComponent`가 Frame Index를 더하며, JSON Frame 정보가 없을 때 같은 크기로 나눈 Grid Cell에서 UV를 계산합니다.

### 2.2 JSON SubUV와 애니메이션

`FSubUVAtlasLoader`는 JSON의 `meta.image`, `meta.size`, `frames`를 읽어 Frame 이름, Pixel Rectangle과 Pivot을 보관하고 첫 번째 Texture Page를 WIC로 디코딩합니다. Scene은 Frame Rectangle을 Texture 전체 크기로 나눠 정규화 UV를 구성합니다.

```text
SubUV JSON + Texture Image
        │
        ▼
FSubUVAtlasLoader
  ├─ meta.image / meta.size
  ├─ frames.{name}.frame
  └─ Image Decode + D3D11 Texture 생성
        │
        ▼
USubUVComponent::FrameIndex
        │
        ├─ JSON Frame UV 사용
        └─ 정보가 없으면 Row/Column Grid UV 사용
```

`USubUVAnimatedComponent`는 기본 30 FPS이며, `Update()`에서 누적 시간만큼 Frame Index를 전진시킵니다. Loop가 켜져 있으면 처음으로 돌아가고, 꺼져 있으면 마지막 Frame에서 정지합니다. Frame Rate와 Loop 여부는 Component Property로 편집하고 Scene에 저장할 수 있습니다.

관련 코드:

- `Engine/Source/Engine/Component/Sprite/SpriteComponent.cpp`
- `Engine/Source/Engine/Component/Sprite/AtlasComponent.cpp`
- `Engine/Source/Engine/Component/Sprite/SubUVComponent.cpp`
- `Engine/Source/Engine/Component/Sprite/SubUVAnimatedComponent.cpp`
- `Engine/Source/Asset/SubUVAtlasLoader.cpp`
- `Engine/Source/Renderer/D3D11/D3D11SpriteBatchRenderer.cpp`

## 3. Font Atlas 기반 실시간 Text

`FFontAtlasLoader`는 BMFont 계열 JSON의 `info`, `common`, `pages`, `chars` 정보를 읽고 Font Texture와 Glyph Metric을 생성합니다. `UAtlasTextComponent`는 문자열과 Font 경로, Scale, Letter Spacing, Line Spacing, Billboard 설정을 보관합니다.

Text Batch Renderer는 실행 중 문자열을 UTF-8 Code Point로 디코딩하고, 각 Glyph를 4개 Vertex와 6개 Index의 Quad로 변환합니다.

- 1~4 Byte UTF-8 Sequence와 잘못된 Sequence 검사를 지원합니다.
- 개행, 공백, 자간, 행간과 여러 줄 배치를 반영합니다.
- Font Atlas에 Glyph가 있으면 한글도 같은 경로로 렌더링합니다.
- Glyph가 없으면 `?` Glyph 또는 Debug Quad로 대체합니다.
- 같은 Font와 배치 방식을 사용하는 Text를 동적 Vertex/Index Buffer에 묶어 제출합니다.

저장소에는 NEXON Lv1, PUBG HeadLiner, 국민체조, D2Coding Nerd, Galmuri, JetBrains Mono 등 미리 생성된 `.Font` JSON과 PNG Atlas가 포함되어 있습니다. 따라서 시스템 Font 파일을 실행 중 직접 Rasterize하지 않습니다.

관련 코드:

- `Engine/Source/Asset/FontAtlasLoader.cpp`
- `Engine/Source/Engine/Component/Text/AtlasTextComponent.cpp`
- `Engine/Source/Renderer/D3D11/D3D11TextBatchRenderer.cpp`
- `Editor/Content/Shader/ShaderFont.hlsl`

## 4. Batch Line과 Index Buffer

### 4.1 Dynamic Line Batch

`FD3D11LineBatchRenderer`는 Grid, World Axis, AABB 등의 Line 요청을 Frame 동안 CPU 배열에 누적합니다. 하나의 Line은 두 개의 `FLineVertex`로 저장하며, 동적 Vertex Buffer를 갱신한 뒤 `D3D11_PRIMITIVE_TOPOLOGY_LINELIST`로 Draw합니다.

```text
BeginFrame
  → AddLine 요청 누적
  → 최대 8,192 Line에 도달하면 Flush
  → 동적 Vertex Buffer Map / Copy
  → Draw(VertexCount)
  → EndFrame에서 남은 Line Flush
```

Line마다 Draw Call을 생성하지 않고 용량 단위로 묶어 제출합니다. 다만 과제 목표에는 Vertex Buffer와 Index Buffer를 함께 사용하는 Line Batch가 제시되어 있지만, 현재 Line Renderer는 Index Buffer 없이 Vertex Buffer와 `Draw`만 사용합니다.

### 4.2 Indexed Geometry

Index Buffer는 Primitive, Sprite와 Text 경로에서 사용합니다.

- Primitive: Immutable Vertex Buffer와 `DXGI_FORMAT_R16_UINT` Index Buffer
- Sprite: Quad당 4 Vertex와 6개의 32-bit Index를 동적 버퍼에 누적
- Text: Glyph당 4 Vertex와 6개의 32-bit Index를 동적 버퍼에 누적

공유 가능한 Vertex를 Index로 재사용하고 `DrawIndexed` 또는 `DrawIndexedInstanced`로 Geometry를 제출합니다.

관련 코드:

- `Engine/Source/Renderer/D3D11/D3D11LineBatchRenderer.cpp`
- `Engine/Source/Renderer/D3D11/D3D11MeshBatchRenderer.cpp`
- `Engine/Source/Renderer/D3D11/D3D11SpriteBatchRenderer.cpp`
- `Engine/Source/Renderer/D3D11/D3D11TextBatchRenderer.cpp`

## 5. AABB와 Object Selection

`UPrimitiveComponent::UpdateBounds()`는 Local Triangle의 모든 Vertex를 World Transform한 뒤 최소·최대 좌표를 구해 World AABB를 갱신합니다. Triangle 정보가 없으면 Local Bounds를 Transform하는 경로를 사용합니다.

Actor의 `bShowBounds`가 켜져 있으면 AABB의 8개 꼭짓점과 12개 Edge를 Line Batch Renderer에 제출합니다. 선택·Hover·일반 상태에 따라 Bounds 색상을 다르게 표시합니다.

Actor 선택은 Viewport 좌표로 CPU Ray를 만든 뒤 Actor의 Root Primitive Component가 가진 World AABB와 교차하는 가장 가까운 후보를 선택합니다. 반면 이동·회전·크기 Gizmo는 별도의 Object ID Render Target을 읽어 축을 판별합니다.

현재 Actor Picking 코드는 AABB 적중 시 곧바로 후보를 확정하고 Triangle 검사로 이어지지 않습니다. Triangle 교차 경로는 AABB가 빗나간 경우에만 실행되므로 일반적인 `AABB Broad Phase → Triangle Narrow Phase` 구조와 다릅니다. 이 때문에 실제 Geometry가 없는 AABB 모서리 영역도 선택될 수 있습니다.

관련 코드:

- `Engine/Source/Engine/Component/Core/PrimitiveComponent.cpp`
- `Engine/Source/Renderer/Submitter/AABBSubmitter.cpp`
- `Editor/Source/Viewport/Selection/ViewportSelectionController.cpp`
- `Engine/Source/Renderer/RendererModule.cpp`

## 6. Scene Manager, Show Flag와 Scene 저장

`FScene`은 Actor 배열의 소유권을 관리하고, 매 Frame Actor를 Tick한 뒤 Render Data를 생성합니다. Render Data 수집 시 Component 타입을 판별해 Primitive, Sprite, Text 경로로 분류합니다.

Scene Show Flag:

- Primitive
- Sprite
- Billboard Text
- UUID Text

Editor Show Flag:

- Grid
- World Axis
- Gizmo
- Selection Outline
- Object Label

AABB 표시는 전역 Show Flag가 아니라 Actor별 `bShowBounds` 상태로 제어합니다.

### Scene 직렬화

Scene Serializer는 `JungleScene` Schema Version 2 JSON으로 다음 정보를 저장합니다.

- Actor Type, UUID, Name, Pickable 상태
- Component Type, UUID, Name, Root·Parent 관계
- Relative Location, Quaternion Rotation, Scale
- Bool, Int, Float, String, Asset Path, Vector3, Color Property

`FSceneTypeRegistry`는 Actor와 Component 이름을 Factory에 연결해 복원합니다. 등록되지 않은 타입은 `AUnknownActor` 또는 `UUnknownComponent`로 생성하고 원본 JSON Payload를 보존하므로, 알 수 없는 데이터를 바로 버리지 않습니다. Asset은 Scene에 `/Game/...` 경로로 저장하고 Scene 복원 이후 다시 해석합니다.

관련 코드:

- `Engine/Source/Engine/Scene.cpp`
- `Engine/Source/SceneIO/SceneSerializer.cpp`
- `Engine/Source/SceneIO/SceneTypeRegistry.cpp`
- `Engine/Source/SceneIO/SceneAssetPath.h`

## 7. FName, UObject와 사용자 정의 RTTI

### 7.1 FName

`FNameSubsystem`은 문자열을 Table에 한 번 저장하고, `FName`은 두 Index로 이름을 참조합니다.

- Comparison Index: ASCII 영문을 소문자로 정규화한 비교용 문자열
- Display Index: 입력된 원래 표기를 유지하는 표시용 문자열
- 같은 문자열은 Table Entry를 재사용
- UObject와 Actor·Component 이름에 적용

이를 통해 이름 비교 시 문자열 전체 대신 Comparison Index를 비교하면서 원래 표기는 유지합니다.

### 7.2 UObject, RTTI와 Factory

`UObject` 생성 시 UUID와 `GUObjectArray`의 Internal Index를 부여하고, 소멸 시 해당 Slot을 `nullptr`로 바꿉니다. 사용자 정의 `new/delete`는 UObject 계열의 현재 할당 Byte와 개수를 `UEngineStatics` 통계에 반영합니다.

`DECLARE_RTTI` Macro는 클래스별 고유 ID, Type Name과 부모를 따라 올라가는 `IsA()`를 생성합니다. `Cast<T>()`는 `IsA()` 확인 후 Pointer를 변환하며, `REGISTER_CLASS`는 클래스 ID와 생성 함수를 `FObjectFactory`에 등록합니다. 이는 C++ RTTI와 별도로 구성한 가벼운 객체 타입 판별·생성 기반입니다.

관련 코드:

- `Engine/Source/Core/Misc/Name.cpp`
- `Engine/Source/Core/Misc/NameSubsystem.cpp`
- `Engine/Source/CoreUObject/Object.cpp`
- `Engine/Source/CoreUObject/ObjectFactory.cpp`

## 8. Editor 설정과 Window Resize

Control Panel과 Console에서 Camera Move Speed와 Rotation Speed를 변경할 수 있습니다. Grid Spacing과 Content Browser 왼쪽 Pane 너비도 함께 `Editor/Saved/Config/editor.ini`에 저장하고 다음 실행에서 복원합니다.

Win32 `WM_SIZE`가 발생하면 다음 Resize 경로를 수행합니다.

1. 기존 Render Target과 Depth Stencil View를 해제합니다.
2. Swap Chain Buffer를 새 Window 크기로 Resize합니다.
3. Render Target, Depth Stencil과 Object ID Picking Resource를 다시 생성합니다.
4. Viewport 크기와 Camera Aspect Ratio를 갱신합니다.
5. Selection Controller가 변경된 Viewport 크기를 사용하도록 갱신합니다.

관련 코드:

- `Engine/Source/ApplicationCore/Windows/WindowsWindow.cpp`
- `Engine/Source/Renderer/D3D11/D3D11RHI.cpp`
- `Editor/Source/Editor/EditorSettings.cpp`
- `Editor/Source/Editor/Editor.cpp`

## 9. Content Browser와 Asset Workflow

Content Browser는 `Editor/Content`를 탐색하고 Texture, Font Atlas와 SubUV Atlas를 Asset Manager에 연결합니다.

```text
Content Source File
  → Source Cache: 경로·크기·수정 시각·Source Hash
  → Asset Type별 Loader
  → CPU Asset + D3D11 Resource
  → Source Hash 기반 Cache 재사용
  → Component가 /Game/... 경로로 참조
```

Texture Image는 WIC로 RGBA8 데이터로 디코딩하며, Font와 SubUV Loader는 같은 Source Hash의 Decode 결과와 GPU Resource를 재사용합니다. Scene은 GPU Pointer가 아니라 Asset 경로를 저장하므로 저장·복원 이후에도 Asset Manager를 통해 Resource를 다시 연결할 수 있습니다.

관련 코드:

- `Engine/Source/Asset/AssetManager.cpp`
- `Engine/Source/Asset/AssetManager.h`
- `Engine/Source/Asset/FontAtlasLoader.cpp`
- `Engine/Source/Asset/SubUVAtlasLoader.cpp`

## 빌드 및 실행

### 요구 환경

- Windows 10/11
- Visual Studio 2022, MSVC v143과 Windows 10 SDK
- C++20
- Python 3: Visual Studio 프로젝트 재생성 시 필요
- NuGet Package `directxtk_desktop_win10` `2025.10.28.2`

### 실행 방법

1. Solution의 NuGet Package를 복원합니다.
2. `Kraftonjungle_Team2.sln`을 Visual Studio에서 엽니다.
3. `Debug | x64` 또는 `Release | x64`로 빌드합니다.
4. `Editor`를 시작 프로젝트로 실행합니다.

Debug 실행 파일은 `Editor/Bin/Debug/Editor.exe`에 생성됩니다. 프로젝트 구성을 다시 만들어야 한다면 저장소 Root에서 `GenerateProjectFiles.bat`을 실행합니다.

> NuGet 복원 없이 빌드하면 `Engine/packages.config`에 선언된 `directxtk_desktop_win10.targets`를 찾지 못해 `EnsureNuGetPackageBuildImports` 단계에서 실패합니다.

## 샘플 에셋

`Editor/Content/Scenes/Sample.Scene`에는 Primitive, 일반·Billboard Sprite, Effect SubUV Animation과 여러 Font의 Text Actor가 배치되어 있습니다.

- `Editor/Content/Texture/Character/sample_character.json`
- `Editor/Content/Texture/Effect/sample_effect.json`
- `Editor/Content/Texture/Panel/sample_panel.png`
- `Editor/Content/Font/*/*.Font`

## 구현 범위와 제한 사항

- `Lit`과 `Unlit` View Mode는 UI에서 구분되지만 현재 같은 Unlit Primitive Shader를 사용합니다.
- Line Batch Renderer는 동적 Vertex Buffer만 사용하며, 과제에서 제시한 Line용 Index Buffer는 구현하지 않았습니다.
- Actor Picking은 Root Primitive Component만 검사하고 AABB 적중을 최종 후보로 사용합니다. 정상적인 AABB 이후 Triangle 정밀 검사 흐름은 연결되어 있지 않습니다.
- Drag Selection은 Actor Bounds가 아니라 Actor Origin을 화면에 투영해 포함 여부를 판단합니다.
- SubUV Loader는 첫 번째 Texture Page와 Object 형태의 `frames`만 사용합니다. Multi-page, Array Frame, Rotated/Trimmed Frame과 Frame별 Duration은 반영하지 않습니다.
- SubUV JSON의 Frame 목록은 하나의 `Default` Sequence로만 구성하며, 실제 재생은 Component의 FPS와 Loop 설정을 사용합니다.
- Scene Type Registry에서 `USubUVAnimatedComponent` 이름에 `USubUVComponent` Factory가 등록되어 있습니다. 기본 Animated Component를 재사용하는 Actor는 복원될 수 있지만, 새로 생성해야 하는 경우 정확한 파생 타입이 유지되지 않을 수 있습니다.
- Font Loader는 첫 번째 Atlas Page만 사용합니다. 실행 중 시스템 Font를 Rasterize하지 않으므로 필요한 한글 Glyph가 포함된 `.Font`와 PNG Atlas가 필요합니다.
- Asset Loading과 GPU Resource 생성은 동기 방식이며 Background Streaming은 구현하지 않았습니다.
- FName의 대소문자 정규화는 ASCII 영문에 한정됩니다. 현재 `wchar_t*` 생성자의 비어 있음 조건과 정렬 연산자의 비엄격 비교(`<=`, `>=`)도 보완이 필요합니다.
- `GUObjectArray`는 소멸한 객체의 Slot을 `nullptr`로 남기며 배열을 압축하거나 Index를 재사용하지 않습니다.
- Undo/Redo와 범용 Property Reflection System은 구현 범위에 포함되지 않습니다.

## License

This project is licensed under the [MIT License](LICENSE).
