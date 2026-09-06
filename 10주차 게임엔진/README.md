# LunaticEngine — Week 10 FBX Import, Skeletal Mesh & CPU Skinning

> **FBX SDK · Static/Skeletal Mesh Import · Reference Pose · CPU Skinning · Asset Bake · Skeletal Mesh Editor**

Windows 환경에서 동작하는 Win32 + DirectX 11 기반 자체 엔진에 Autodesk FBX SDK를 통합하고, FBX의 메시·머티리얼·스켈레톤·스킨 가중치를 엔진 애셋으로 변환한 프로젝트입니다.

10주차에는 Skeletal Mesh의 Reference Pose를 CPU Skinning으로 렌더링하는 런타임 경로와, 본 계층을 확인하고 Preview Pose를 편집할 수 있는 내부 Skeletal Mesh Editor를 구축했습니다. Static Mesh FBX Import와 바이너리 `.uasset` 베이킹도 같은 임포트 파이프라인에 연결했습니다.

이 문서는 10주차 과제의 학습 목표와 실제 구현 코드를 기준으로 작성했습니다.

## 학습 목표

- Autodesk FBX SDK를 자체 엔진 빌드 환경에 통합한다.
- FBX Scene, Node, Mesh, Control Point, Polygon Vertex의 관계를 이해한다.
- Bone Hierarchy, Skin Cluster, Bind Pose, Vertex Weight를 엔진 데이터로 변환한다.
- Skeletal Mesh의 Reference Pose를 CPU Skinning으로 렌더링한다.
- `USkeletalMeshComponent`, `ASkeletalMeshActor`, `FSkeletalMeshProxy`를 렌더 파이프라인에 연결한다.
- `SkeletalMesh` Show Flag로 뷰포트별 렌더링을 제어한다.
- 에디터 내부에 Skeleton Tree, Details, Preview Viewport, Bone Gizmo를 갖춘 Skeletal Mesh Editor를 구현한다.
- Static Mesh FBX Import를 지원하고 파싱 결과를 바이너리 애셋으로 베이킹한다.

## 구현 요약

| 영역 | 구현 내용 |
| --- | --- |
| FBX SDK | 프로젝트 내부 Header·Library 연결, FBX Scene 생성 및 파싱 |
| 공통 전처리 | Maya Z-Up 축 변환, 자동 삼각분할, 좌표·UV·노멀 변환 |
| Static Mesh | 정점·인덱스·노멀·UV·탄젠트·AABB·Material Section 생성 |
| Skeletal Mesh | Bone Hierarchy, Local Bind Transform, Inverse Bind Pose, Skin Weight 추출 |
| 스킨 가중치 | 정점당 최대 4개 영향 유지, 중복 영향 병합, 절삭 후 정규화 |
| CPU Skinning | Local Pose를 Component Space로 누적하고 위치·노멀을 CPU에서 변형 |
| 렌더링 | CPU 결과를 Dynamic Vertex Buffer로 업로드하고 Material Section 단위로 제출 |
| 런타임 | `USkeletalMeshComponent`, `ASkeletalMeshActor`, 포즈 기반 AABB, Show Flag |
| 애셋 | `UStaticMesh`·`USkeletalMesh` 바이너리 `.uasset` 저장 및 로드 캐시 |
| 에디터 | 다중 애셋 문서, Skeleton Tree, 본 선택, Details 편집, Preview Pose Gizmo |

## 전체 데이터 흐름

```text
FBX 원본
  │
  ├─ FBX Scene 파싱
  │    ├─ 좌표계 변환 및 삼각분할
  │    └─ Skin Deformer 유무 검사
  │
  ├─ Static Mesh
  │    └─ Vertex / Index / Material Section / Tangent / Bounds
  │
  └─ Skeletal Mesh
       ├─ Vertex / Index / Material Section
       ├─ Bone Hierarchy / Bind Pose
       └─ Vertex Bone Index / Weight
              │
              ▼
       UStaticMesh / USkeletalMesh
              │
              ├─ Binary .uasset Bake
              └─ USkeletalMeshComponent
                      │
                      ├─ Local Pose → Component Space Pose
                      ├─ CPU Skinning → SkinBuffer
                      └─ FSkeletalMeshProxy → Dynamic Vertex Buffer
                                      │
                                      ▼
                           Level Viewport / Asset Preview
```

## 1. FBX SDK 통합과 공통 파싱

FBX SDK Header와 x64 Library를 프로젝트 내부 `ThirdParty/FBXSDK`에서 참조하도록 구성했습니다. Debug와 Release 구성은 각각 FBX SDK의 대응 Library 경로를 사용합니다.

`FFbxCommon::ParseFbx`는 다음 순서로 FBX Scene을 준비합니다.

1. `FbxManager`와 `FbxIOSettings`를 생성한다.
2. `FbxImporter`로 원본 파일을 읽어 `FbxScene`을 구성한다.
3. 원본 축 체계가 다르면 `FbxAxisSystem::MayaZUp`으로 Scene을 변환한다.
4. `FbxGeometryConverter::Triangulate`로 Scene Geometry의 삼각분할을 시도한다.
5. 변환된 Scene을 Static 또는 Skeletal Importer가 순회한다.

### 좌표와 정점 속성 변환

FBX와 엔진의 좌표 규약 차이를 한곳에서 처리하도록 공통 변환 함수를 분리했습니다.

- 위치와 방향은 FBX 좌표를 엔진 좌표로 재배치합니다.
- UV는 첫 번째 UV Set을 사용하고 V축을 뒤집습니다.
- 노멀은 Control Point, Polygon Vertex, Polygon, All Same Mapping Mode를 처리합니다.
- 노멀 인덱스를 직접 해석할 수 없는 경우 FBX SDK의 Polygon Vertex Normal 조회로 폴백합니다.
- 비균등·음수 Scale에서도 방향이 깨지지 않도록 선형 변환의 Inverse Transpose로 노멀을 변환합니다.

정점 위치만 변환하고 Bone Matrix를 그대로 두면 Reference Pose가 어긋납니다. 따라서 메시 정점과 Bind Matrix에 동일한 좌표계 변환 규칙을 적용했습니다.

### 머티리얼과 텍스처

FBX Node의 Material Mapping을 읽어 Polygon별 Material Index를 결정하고, Material 단위로 Section을 재구성합니다.

- Lambert 계열 Diffuse Color와 Diffuse Factor 추출
- Diffuse Texture 탐색 및 Texture Asset Import
- Normal Map 탐색, 없으면 Bump Texture를 대체 경로로 사용
- 임포트한 Texture와 색상 파라미터를 연결한 Opaque UberLit Material Asset 생성
- Material이 없는 메시에는 기본 Material Slot 생성

## 2. Static Mesh FBX Import

`FFbxStaticMeshImporter`는 Scene의 모든 Node를 재귀 순회하며 Mesh Node를 하나의 `FStaticMesh`로 합칩니다.

### 정점 생성

FBX의 Control Point는 위치를 공유하지만 Polygon Corner마다 노멀, UV, Material이 달라질 수 있습니다. 이를 보존하기 위해 다음 값을 조합한 키로 최종 렌더 정점을 생성합니다.

```text
ControlPointIndex + PolygonIndex + CornerIndex + MaterialIndex
```

이 방식은 하드 엣지, UV Seam, Material 경계에서 필요한 정점 분리를 유지합니다. 변환된 삼각형은 엔진의 Winding Order에 맞춰 인덱스 순서를 조정합니다.

### Section, Tangent와 Bounds

- Polygon을 Material별로 수집한 뒤 Index Buffer를 Section 순서로 다시 배치합니다.
- 각 Section은 Material Index와 시작 Index, Triangle 수를 보관합니다.
- 위치와 UV 변화량으로 Tangent·Bitangent를 누적하고 Gram-Schmidt 방식으로 Tangent를 직교화합니다.
- Bitangent 방향으로 Tangent Handedness를 계산해 `tangent.w`에 저장합니다.
- 최종 정점으로 Local Bounds를 캐시합니다.

따라서 FBX Static Mesh도 기존 OBJ 경로와 동일하게 `UStaticMesh` 애셋과 Material Section 단위의 렌더링 경로를 사용할 수 있습니다.

## 3. Skeletal Mesh Import

`FMeshAssetManager::IsFbxSkeletalMesh`는 Scene을 순회해 Skin Deformer가 있는지 검사합니다. Skin이 있으면 `FFbxSkeletalMeshImporter`, 없으면 Static Mesh Importer로 분기합니다.

### Skeletal Vertex와 Control Point 연결

Skeletal Mesh도 Polygon Corner 단위로 최종 정점을 생성합니다. 한 Control Point가 UV·노멀·Material 경계에서 여러 렌더 정점으로 분리될 수 있으므로, 임포트 중 다음 관계를 별도로 보관합니다.

```text
FBX Control Point 1개
        │
        └─ Engine Render Vertex N개
```

이후 Cluster가 제공하는 Control Point Weight를 연결된 모든 렌더 정점에 복제합니다. 이를 통해 정점 분리 이후에도 Skin Weight가 누락되지 않습니다.

메시 정점 변환에는 Skin Cluster가 제공하는 Mesh Bind Global Matrix를 우선 사용하고, 없는 경우 Node의 평가된 Global Transform을 사용합니다. 이 기준을 Bone Bind Matrix와 맞춰 Reference Pose의 공간 일관성을 유지합니다.

### Material Section과 Tangent

Skeletal Mesh도 Static Mesh와 같은 방식으로 Polygon을 Material별로 모으고 Index Buffer를 Section 순서로 재배치합니다. UV를 기준으로 Reference Pose Tangent와 Handedness도 계산합니다.

## 4. Bone Hierarchy와 Bind Pose

### 전체 Skeleton 계층 등록

Cluster Weight가 있는 Bone만 등록하면 가중치가 없는 중간 Bone이나 End Bone이 계층에서 사라질 수 있습니다. 이를 방지하기 위해 Skin Weight를 읽기 전에 Scene의 Skeleton Node 전체를 등록합니다.

각 `FBoneInfo`는 다음 데이터를 보관합니다.

| 데이터 | 역할 |
| --- | --- |
| Name | 본 이름 |
| ParentIndex | 부모 본 인덱스, Root는 `InvalidBoneIndex` |
| LocalBindTransform | 부모 기준 Reference Pose Transform |
| InverseBindPose | Reference Pose Global Matrix의 역행렬 |

부모 관계를 따라 `BoneChildren`과 `RootBoneIndices` 캐시를 생성하고, 이 캐시는 런타임 Pose 누적과 Skeleton Tree 출력에서 공통으로 사용합니다.

### Bind Transform 재구성

Cluster의 `TransformLinkMatrix`를 Bone의 Bind Global Matrix로 사용하고, 데이터가 없으면 Bone Node의 평가된 Global Transform으로 폴백합니다.

1. 자식 Bind Global Matrix에 부모 Bind Global Matrix의 역행렬을 곱해 Local Bind Matrix를 구합니다.
2. Local Matrix를 엔진 `FTransform`으로 변환합니다.
3. 저장된 Local Transform을 계층 순서로 다시 누적해 런타임 기준 Global Matrix를 구성합니다.
4. 재구성한 Global Matrix에서 Inverse Bind Pose를 다시 계산합니다.

직렬화되는 `FTransform`과 실제 런타임 계산 결과를 기준으로 Inverse Bind Pose를 맞추기 때문에 Reference Pose에서 `InverseBindPose × CurrentBindPose`가 항등 변환에 가깝게 유지됩니다.

음수 Scale이나 Reflection이 있는 FBX도 축의 부호를 가능한 한 보존하도록 Matrix를 분해합니다.

## 5. Skin Weight 생성

`FSkinWeight`는 정점 하나당 최대 4개의 Bone Index와 Weight를 저장합니다.

가중치 처리 순서는 다음과 같습니다.

1. Cluster의 Control Point Index와 Weight를 읽는다.
2. Control Point에 연결된 모든 렌더 정점을 찾는다.
3. 동일 Bone의 중복 영향은 합산한다.
4. 빈 슬롯을 먼저 채운다.
5. 슬롯이 가득 찬 경우 현재 최소 Weight보다 큰 영향만 교체한다.
6. 최종 4개 Weight의 합이 1이 되도록 정규화한다.

Skin Deformer가 없는 Rigid Mesh가 Skeleton Node 아래에 배치된 경우에는 가장 가까운 부모 Bone에 Weight 1.0을 할당합니다. 캐릭터 FBX 안에 강체 부속물이 함께 들어 있어도 해당 Bone을 따라 움직일 수 있습니다.

## 6. Skeletal Mesh 애셋 구조

`USkeletalMesh`는 엔진의 `UObject` 애셋 래퍼이고 실제 기하·스켈레톤 데이터는 `FSkeletalMesh`가 보관합니다.

| 구조 | 주요 데이터 |
| --- | --- |
| `FSkeletalMesh` | Path, Vertices, Indices, SkinWeights, Bones, Sections |
| `FSkinWeight` | BoneIndices[4], BoneWeights[4] |
| `FBoneInfo` | Name, ParentIndex, LocalBindTransform, InverseBindPose |
| `FSkeletalMeshSection` | MaterialIndex, Index/Vertex 범위 |
| `USkeletalMesh` | `FSkeletalMesh` 소유, Static Material Slot, 직렬화 진입점 |

`BoneChildren`과 `RootBoneIndices`는 직렬화 데이터가 아니라 로드 후 다시 생성하는 런타임 캐시입니다.

Material Slot은 애셋 기본 Material을 보관하고, `USkinnedMeshComponent`는 인스턴스별 Override Material과 직렬화용 Material Path를 관리합니다.

## 7. CPU Skinning과 Pose

`FSkeletonPose`는 본별 Local Transform과 Component Space Matrix를 분리해서 보관합니다. Local Pose가 바뀌면 Parent Index 순서대로 Component Space를 다시 계산합니다.

이 엔진의 Row-Vector 행렬 규약에서 Bone별 Skinning Matrix는 다음 순서를 사용합니다.

```text
SkinningMatrix = InverseBindPose × CurrentComponentSpaceTransform
```

정점별 계산 흐름은 다음과 같습니다.

```text
Reference Pose Vertex
        │
        ├─ 최대 4개 Bone Influence 순회
        │      ├─ Position: Skinning Matrix 적용
        │      └─ Normal: Inverse Transpose Matrix 적용
        │
        ├─ Weight를 곱해 누적
        └─ Component별 SkinBuffer에 기록
```

원본 `FSkeletalMesh::Vertices`는 Reference Pose로 보존하고, `USkeletalMeshComponent`가 인스턴스별 `SkinBuffer`를 소유합니다. 메시를 할당하는 시점에도 Skeleton과 Skin Buffer를 초기화하므로, `BeginPlay` 이전의 에디터 Preview에서도 바로 렌더링할 수 있습니다.

### 안정성 처리

- Skin Weight 수가 정점 수보다 적으면 전체 정점을 Reference Pose로 폴백합니다.
- Pose Matrix 수가 Bone 수보다 적어도 Reference Pose로 폴백합니다.
- 범위를 벗어난 Bone Index와 0 이하 Weight는 건너뜁니다.
- 유효하지 않은 Matrix와 비정상 수치 결과는 Skin Buffer에 반영하지 않습니다.
- 노멀은 비균등 Scale을 고려해 Inverse Transpose로 변환한 뒤 정규화합니다.

Animation Sequence는 아직 없으므로 런타임 시연용으로 선택된 일부 Bone에 고정 Seed 기반 사인파 회전을 적용하는 옵션을 제공합니다. 이는 10주차 CPU Skinning 동작을 검증하기 위한 테스트 경로입니다.

## 8. Component와 렌더 파이프라인

### Runtime Component

`USkinnedMeshComponent`는 Skeletal Mesh 참조, Current Pose, Material Override, 스키닝 메시 공통 기능을 담당합니다. `USkeletalMeshComponent`는 이를 상속해 Pose 평가와 CPU Skinning을 구현합니다.

`ASkeletalMeshActor`를 월드에 배치하면 `USkeletalMeshComponent`가 Scene Proxy를 생성하고 기존 Primitive 렌더 파이프라인에 참여합니다.

### Skeletal Mesh Proxy

`FSkeletalMeshProxy`는 다음 리소스를 관리합니다.

- CPU Skinning 결과를 받는 Dynamic Vertex Buffer
- 임포트 후 변하지 않는 Static Index Buffer
- Material Section별 시작 Index와 Index Count
- 애셋 기본 Material과 Component Override Material 해석

매 프레임 `SkinBuffer`를 Dynamic Vertex Buffer에 업로드하고 Section 단위 Draw Command를 생성합니다. Section 정보가 없거나 손상된 예외 데이터에는 전체 Index Buffer를 사용하는 단일 Fallback Draw를 생성합니다.

### 포즈 기반 Bounds

Skeletal Mesh는 포즈에 따라 정점 범위가 바뀌므로 Static Asset Bounds를 그대로 사용할 수 없습니다. `USkinnedMeshComponent::UpdateWorldAABB`는 현재 CPU Skinning 정점에서 Local AABB를 다시 계산한 뒤 World Transform을 적용합니다.

### SkeletalMesh Show Flag

`EPrimitiveProxyFlags::SkeletalMesh`로 Skeletal Mesh Proxy를 식별하고, Viewport의 `bSkeletalMesh` Show Flag가 꺼져 있으면 Render Collector가 해당 Proxy의 Draw Command 생성을 제외합니다.

- Level Viewport Show 메뉴에서 Skeletal Mesh 표시 제어
- Skeletal Mesh Preview Toolbar에서 독립적으로 표시 제어
- 뷰포트마다 다른 Show Flag 상태 적용

## 9. `.uasset` 베이킹과 캐시

FBX를 매 실행마다 파싱하지 않도록 변환 결과를 엔진의 바이너리 `.uasset` 형식으로 저장합니다.

`FAssetFileSerializer`는 Magic, Version, Asset Class, 이름과 Body Offset이 포함된 Header를 기록하고, Root `UObject`의 `Serialize`를 호출해 실제 데이터를 저장합니다. Static Mesh와 Skeletal Mesh는 같은 컨테이너를 사용하지만 Class ID로 타입을 구분합니다.

### 로드와 갱신 흐름

```text
원본 FBX 요청
    │
    ├─ 대응 .uasset 없음 ───────────┐
    │                              │
    ├─ 원본 수정 시각이 더 최신 ───┤
    │                              ▼
    │                         FBX 재임포트
    │                              │
    │                              ▼
    │                         .uasset 저장
    │
    └─ .uasset이 최신 ────────────> Binary Asset 로드
```

Skeletal Mesh `.uasset`에는 다음 데이터가 저장됩니다.

- Reference Pose 정점과 인덱스
- Skin Weight
- Bone 이름, 부모, Local Bind Transform, Inverse Bind Pose
- Material Section과 Material Slot

로드 후에는 Bone Hierarchy Cache를 다시 만들고, 잘못된 Index나 Section 범위가 있으면 Preview 안정성을 위해 유효한 Triangle만 남기거나 단일 Fallback Section을 구성합니다.

## 10. Skeletal Mesh Editor

Skeletal Mesh Editor는 별도 실행 파일이 아니라 기존 Editor의 Asset Editor 문서로 동작합니다. Content Browser에서 `USkeletalMesh` `.uasset`을 열면 각 애셋이 독립 문서 탭과 Preview 상태를 가집니다.

Viewer가 원본 FBX를 직접 열지 않고 베이킹된 `.uasset`을 사용하도록 Import와 Asset Editing 책임을 분리했습니다.

### Preview Viewport

- Perspective, Top, Bottom, Left, Right, Front, Back, Free Ortho 카메라
- Lit, Unlit, Wireframe, World Normal, Scene Depth 등 View Mode
- Grid, World Axis, Bones, Gizmo, Mesh Stats, SkeletalMesh Show Flag
- Orbit, Pan, Zoom, Frame Selected 입력
- Bone Sphere와 부모-자식 연결선 Debug Draw
- 선택 Bone과 연결 Bone Highlight

Preview Scene은 Level Scene과 분리되어 있고, Asset Editor가 활성화된 동안에만 입력과 Gizmo Context를 연결합니다. 문서 탭을 전환할 때 이전 탭의 선택이나 Gizmo 조작이 다른 애셋에 적용되지 않도록 Context Epoch와 활성 상태를 검사합니다.

### Skeleton Tree와 선택

Skeleton Tree는 `RootBoneIndices`와 `BoneChildren`을 이용해 실제 계층 구조를 표시합니다.

- Bone 이름 검색
- 계층 접기·펼치기
- 단일 선택
- Ctrl 다중 선택
- Shift 범위 선택
- 전체 선택
- Tree, Details, Preview Viewport의 선택 상태 공유

Preview Viewport에서도 Bone Sphere와 연결선을 Ray Picking해 같은 Selection Manager에 반영합니다. 다중 선택 시 가장 최근에 선택한 Bone을 Primary Bone으로 사용해 Gizmo가 의도하지 않은 Root Bone에 연결되는 문제를 피했습니다.

### Preview Pose 편집

Pose Edit Mode에서 선택 Bone의 Local Transform을 변경하면 다음 순서로 즉시 결과를 확인할 수 있습니다.

```text
Skeleton Tree / Viewport Bone 선택
            │
            ▼
Details 또는 Transform Gizmo
            │
            ▼
Preview Pose Local Transform 변경
            │
            ▼
Component Space 재구성 → CPU Skinning → Preview 갱신
```

Details 패널에서는 Local Location, Rotation, Scale을 편집할 수 있고 Reference와 Mesh Relative Transform은 읽기 전용으로 확인합니다. Gizmo의 World Transform을 Bone의 Parent Space Local Transform으로 역변환해 Preview Pose에 적용합니다.

Asset Details에서는 Material Slot을 교체하고 Undo/Redo 및 `.uasset` 저장을 수행할 수 있습니다. Preview Pose는 스키닝 확인을 위한 임시 상태이며 원본 Skeleton의 Reference Pose를 수정하지 않습니다.

## 빌드 및 실행

### 요구 환경

- Windows 10/11 64-bit
- Visual Studio 2022
- MSVC v143 Toolset
- Windows 10 SDK
- DirectX 11 지원 GPU
- 저장소에 포함된 Autodesk FBX SDK Header·x64 Library

### 프로젝트 파일 생성

루트의 Batch 파일을 실행합니다.

```bat
GenerateProjectFiles.bat
```

Batch 파일은 저장소에 포함된 Python Runtime으로 `Scripts/GenerateProjectFiles.py`를 실행합니다. 시스템 Python을 사용할 경우 다음 명령도 가능합니다.

```powershell
python Scripts/GenerateProjectFiles.py
```

### 빌드

생성된 `LunaticEngine.sln`을 Visual Studio에서 열어 x64 구성으로 빌드하거나 Developer PowerShell에서 실행합니다.

```powershell
msbuild LunaticEngine.sln /p:Configuration=Debug /p:Platform=x64
```

실행 파일은 구성에 따라 `LunaticEngine/Bin/<Configuration>/LunaticEngine.exe`에 생성됩니다.

### 기능 확인

1. Editor를 실행한다.
2. Asset Import에서 Static 또는 Skeletal FBX를 선택한다.
3. Content Browser에 생성된 `SM_*.uasset` 또는 `SK_*.uasset`을 확인한다.
4. Skeletal Mesh Asset을 열어 메시, Material Section, Skeleton Tree를 확인한다.
5. Pose Edit Mode를 켜고 Bone을 선택해 Details 또는 이동 Gizmo로 Preview Pose를 변경한다.
6. Level에 Skeletal Mesh Actor를 배치하고 `SkeletalMesh` Show Flag 동작을 확인한다.

## 구현 범위

| 구분 | 10주차 구현 상태 |
| --- | --- |
| FBX Static Mesh Import | 구현 |
| FBX Skeletal Mesh Import | 구현 |
| Material Section과 Texture 연결 | 구현 |
| Bone Hierarchy와 Bind Pose | 구현 |
| 정점당 최대 4개 Skin Weight | 구현 |
| Reference Pose 렌더링 | 구현 |
| CPU Skinning | 구현 |
| Skeletal Mesh Component·Actor·Proxy | 구현 |
| 포즈 기반 AABB | 구현 |
| SkeletalMesh Show Flag | 구현 |
| Binary `.uasset` Bake와 Cache | 구현 |
| Skeleton Tree와 Bone Picking | 구현 |
| Details 기반 Preview Pose 편집 | 구현 |
| Translation Gizmo Preview 편집 | 구현 |
| Animation Sequence와 Blending | 미구현 |
| GPU Skinning | 미구현 |

### 제한 사항

- **Windows x64 전용 FBX 경로**: FBX Importer 구현과 연결 Library는 `_WIN64` 및 x64 구성을 기준으로 작성되어 있습니다.
- **정점당 Bone 영향 수**: `FSkinWeight`는 최대 4개의 Bone Index와 Weight만 저장합니다. 4개를 넘는 영향은 큰 Weight를 우선해 남기고 나머지는 제거한 뒤 정규화합니다.
- **삼각분할 결과 의존**: Import 전에 FBX SDK로 자동 삼각분할을 시도합니다. 변환 이후에도 Triangle이 아닌 Polygon으로 남은 면은 건너뜁니다.
- **단일 UV Set**: Mesh가 가진 첫 번째 UV Set만 사용하며 다중 UV Channel은 저장하지 않습니다.
- **제한된 Material 변환**: Diffuse Color·Diffuse Texture·Normal/Bump Texture 중심으로 Opaque UberLit Material을 생성합니다. FBX의 전체 PBR Parameter, Layered Texture, 투명 Blend 설정을 보존하지 않습니다.
- **최대 LOD0**: `FSkeletalMeshProxy`는 현재 LOD0만 렌더링하며 거리나 화면 크기에 따른 Skeletal Mesh LOD 전환은 지원하지 않습니다.
- **CPU와 전송 비용**: 모든 정점의 위치와 노멀을 CPU에서 계산하고 Dynamic Vertex Buffer로 업로드합니다. 고해상도 메시, 다수 인스턴스, 다중 뷰포트일수록 비용이 커집니다.
- **포즈 기반 Bounds 비용**: 정확한 컬링 범위를 위해 현재 스키닝된 전체 정점을 순회해 AABB를 다시 계산합니다.
- **Tangent 미변형**: CPU Skinning은 위치와 노멀을 갱신하지만 Tangent는 Reference Pose 값을 유지합니다. 변형이 큰 Pose에서는 Tangent Space Normal Mapping이 부정확할 수 있습니다.
- **유효 영향 재정규화 없음**: Import 시 Weight는 정규화하지만 런타임에서 잘못된 Bone 영향이 제외된 뒤 남은 Weight를 다시 정규화하지 않습니다. 손상된 데이터에서는 정점 위치가 축소될 수 있습니다.
- **Animation 미지원**: FBX Animation Clip, Animation Sequence, Blending, State Machine은 10주차 범위에 포함되지 않습니다. 런타임 Bone 진동은 CPU Skinning 검증용 시연 기능입니다.
- **GPU Skinning 미지원**: Bone Palette를 GPU에 전달하는 Vertex Shader Skinning 경로가 없으며 CPU Skinning만 사용합니다.
- **Gizmo 기능 제한**: Toolbar에는 이동·회전·크기 모드가 표시되지만 Preview Pose Controller가 실제 Delta를 적용하는 경로는 Translation만 구현되어 있습니다. Rotation과 Scale은 Details 수치 입력으로 편집합니다.
- **분할 Viewport 미연결**: One Pane, 2분할, 2×2 Layout 선택 UI는 있으나 실제 Skeletal Mesh Preview 렌더링은 단일 Pane만 연결되어 있습니다.
- **Reference Pose 표시 토글 미연결**: Toolbar에 Reference Pose Show 항목은 존재하지만 별도 Reference Pose Overlay 렌더링 경로에는 연결되지 않았습니다.
- **Preview 편집 범위**: Bone 편집은 현재 Preview Pose에만 적용됩니다. Skeleton 계층, Skin Weight, Reference Pose를 수정하고 리깅 결과를 원본 FBX나 Skeletal Mesh Asset에 저장하는 도구는 아닙니다.
- **Reimport 메뉴 미구현**: Editor 메뉴에 Reimport 항목은 존재하지만 비활성화되어 있습니다. 다시 임포트하려면 Asset Import 경로를 사용해야 합니다.
- **Transform 표현 한계**: Bind Matrix를 Translation·Rotation·Scale의 `FTransform`으로 분해하므로 Shear가 포함된 FBX Transform은 완전히 보존되지 않을 수 있습니다.
