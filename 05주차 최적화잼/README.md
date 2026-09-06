# ZZupEngine — Week 5 Optimization Jam

> **BVH · Frustum Culling · Hi-Z Occlusion Culling · LOD · CPU/GPU Profiling · Render Data Caching**

크래프톤 정글 GameTechLab 자체 게임엔진 제작 교육의 5주차 최적화잼 결과물입니다. 평가는 **평균 FPS**와 **Picking 소요 시간**을 기준으로 진행됐으며, 두 지표에 집중하기 위해 4주차의 ObjViewer를 경량 실험 환경으로 활용했습니다.

기존 게임엔진에는 멀티 뷰포트, 에디터 UI, 기즈모처럼 두 평가 지표와 직접 관련 없는 비용 요인이 함께 존재합니다. 따라서 단일 뷰포트 ObjViewer로 측정 환경을 단순화하고, 렌더링 경로는 평균 FPS, 공간 질의 경로는 Picking 시간에 미치는 영향을 분리해 개선했습니다. 5+주차 PIE·멀티 뷰포트 구현은 별도 프로젝트 문서에서 다룹니다.

이 문서는 과제 명세뿐 아니라 현재 저장소의 실제 ObjViewer 진입점, 렌더링·Picking 경로와 구현 한계를 기준으로 작성했습니다.

## 학습 목표

- 프러스텀 컬링, 공간 분할, SIMD 등 최적화 기법의 적용 지점을 이해한다.
- BVH, Octree, K-d Tree의 목적과 비용을 비교한다.
- CPU/GPU 병목을 계측하고 수집 결과를 바탕으로 개선 대상을 판단한다.
- 정렬·배칭·상태 캐싱으로 불필요한 렌더링 비용을 줄인다.
- RAII를 사용해 계측과 리소스 수명을 관리한다.

## 구현 요약

| 평가 지표 | 구현 |
| --- | --- |
| 평균 FPS | BVH Frustum Culling, GPU Occlusion Culling, QEM LOD, Render Data 캐싱, Render Command 정렬·상태 캐싱 |
| Picking 시간 | Component AABB용 Top-level BVH Broad Phase와 메시 삼각형 BVH Narrow Phase |
| 평가 지표 표시 | 프레임별 FPS, Picking 횟수·누적·최근·평균 시간을 표시하는 ObjViewer 오버레이 |
| 보조 프로파일링 | RAII CPU Scope Timer, D3D11 Timestamp/Disjoint Query, Render Queue 통계 UI |
| 공통 수학 기반 | DirectXMath XMVECTOR 기반 벡터·행렬·AABB·Frustum 연산 |

## 전체 최적화 전략

ObjViewer는 최적화 성과 자체가 아니라, **평균 FPS와 Picking 시간에 영향을 주는 경로를 통제하기 위한 실험 환경**입니다.

기존 게임엔진을 그대로 사용하면 멀티 뷰포트 렌더링, 에디터 패널, 기즈모, 선택 표시처럼 평가와 직접 관계없는 작업까지 함께 측정됩니다. 이를 제거하거나 분리하는 데 드는 시간을 줄이기 위해, 단일 카메라·단일 뷰포트로 동작하는 ObjViewer에 대량 Static Mesh Scene을 구성했습니다.

이 환경에서 렌더링 후보를 줄이는 구현은 평균 FPS 개선을, Ray 기반 공간 질의 최적화는 Picking 시간 단축을 목표로 했습니다.

```text
Static Mesh Scene
  │
  ├─ FRenderDataManager
  │   ├─ Component / AABB / World Matrix 캐시
  │   └─ GPU Structured Buffer 업로드
  │
  ├─ CPU: BVH Frustum Query
  │   └─ 화면 안 후보 Visible Indices
  │
  ├─ GPU: Depth Prepass → Hi-Z mip 0 → Occlusion Cull CS
  │   └─ 최종 생존 후보
  │
  └─ Render Command 정렬 · 상태 캐싱 · Draw
```

## 1. 평균 FPS: BVH 기반 Frustum Culling

ObjViewer가 Scene을 활성화할 때 모든 `UStaticMeshComponent`의 World AABB를 `FRenderDataManager`에 저장하고, 이를 입력으로 Top-level `FBVH`를 구성합니다. BVH 구축은 세 축의 분할 후보를 정렬해 Prefix/Suffix Bounds의 SAH 비용을 비교하며, 기본 Leaf 크기는 4입니다. 매 프레임 카메라 Frustum을 질의해 화면 밖의 Component를 먼저 제외하고, Node가 Frustum 내부에 완전히 포함되면 하위 AABB 검사를 생략합니다.

```text
Viewport Camera
  → Frustum 생성
  → FBVH::FrustumQuery
  → Visible Indices
  → FRenderDataManager::SetVisibleIndices
  → 가시 Render Command
```

BVH의 Ray Query는 가까운 Child부터 순회하고 Leaf에서 교차한 Component 후보를 진입 거리 순으로 정렬합니다. Picking은 이 Top-level BVH를 Broad Phase로 사용하고, 후보 `UStaticMesh`가 보관하는 삼각형 AABB 기반 Mesh BVH를 Narrow Phase로 순회합니다.

### BVH 구현 범위

- SAH 기반 Object AABB 트리 구축과 퇴화 분할 Fallback
- 완전 포함 Node의 하위 검사를 생략하는 Frustum Query
- 가까운 Node 우선 순회와 거리순 후보 정렬을 적용한 Ray Query
- Static Mesh 로딩 시 삼각형 AABB로 생성하는 메시별 BVH
- Top-level Component BVH와 Mesh BVH를 연결한 정밀 Picking

`FBVH`에는 삼각형 질의와 `RefitBVH`, `ReBuildBVH`용 공개 인터페이스도 존재하지만 해당 함수의 구현은 완성되어 있지 않습니다. 실제 Picking은 이 미완성 Helper를 호출하지 않고 `FObjViewerRenderPipeline::RaycastStaticMeshCandidate`에서 Mesh BVH를 직접 순회합니다.

관련 코드:

- `ZZupEngine/Source/Engine/Spatial/BVH.h`
- `ZZupEngine/Source/Engine/Spatial/BVH.cpp`
- `ZZupEngine/Source/Engine/Asset/StaticMesh.cpp`
- `ZZupEngine/Source/Misc/ObjViewer/ObjViewerRenderPipeline.cpp`

## 2. 평균 FPS: GPU Occlusion Culling

Frustum Culling을 통과한 모든 오브젝트가 실제로 화면에 보이는 것은 아닙니다. 뒤쪽 오브젝트가 앞의 큰 메시로 가려진 경우를 줄이기 위해 GPU Occlusion Culling을 추가했습니다.

### 렌더링 흐름

```text
Frustum Visible Commands
  │
  ├─ Depth Prepass
  │   └─ 깊이만 기록
  │
  ├─ Hi-Z Copy
  │   └─ Depth Buffer를 Hi-Z mip 0으로 복사
  │
  ├─ Occlusion Cull Compute Shader
  │   └─ AABB와 Hi-Z를 비교해 가시성 결과 기록
  │
  ├─ Staging Buffer 비동기 readback
  │
  └─ 캐시된 생존 후보로 Static Mesh 렌더링
```

Compute Shader는 Frustum을 통과한 Render Command의 AABB를 Structured Buffer로 전달받습니다. AABB를 Camera-facing Bounding Sphere로 근사하고 Center와 Camera 축 방향의 7개 점을 투영해 Screen-space Footprint와 가장 가까운 Reverse-Z Depth를 계산합니다. 기본 9×9 Depth 표본과 거리·화면 크기에 따른 보수적 추가 표본 경로를 사용해 얇은 틈 사이에서 발생하는 False Occlusion을 줄입니다. 최대 65,536개 후보를 처리하며 한 Thread Group은 64개 후보를 담당합니다.

Hi-Z 다운샘플 Compute Shader와 mip chain 생성 경로는 구현되어 있습니다. 다만 현재 ObjViewer의 Occlusion 제출 경로는 Depth Buffer를 Hi-Z mip 0으로 복사한 뒤, Compute Shader에서도 `mipI = 0`만 사용해 판정합니다. 즉, 상위 mip chain은 확장 기반으로 남아 있으며 현재 Runtime Occlusion 판정에는 사용하지 않습니다.

### 비동기 결과 처리와 안정화

Occlusion 결과를 같은 프레임에 CPU로 즉시 읽으면 GPU 대기가 발생할 수 있습니다. 이를 피하기 위해 GPU 결과 버퍼는 하나만 두되, CPU Readback용 Staging Buffer와 Event Query를 각각 두 개씩 번갈아 사용합니다. `D3D11_ASYNC_GETDATA_DONOTFLUSH`로 완료 여부를 확인하고, 준비된 결과만 소비합니다.

- 결과가 아직 준비되지 않았으면 기존에 채택한 캐시를 사용하고, 캐시가 없으면 Frustum Culling 결과로 렌더링한다.
- Readback이 진행 중이면 새 Occlusion 작업을 중복 제출하지 않는다.
- 가시 후보가 256개 이상일 때만 Occlusion Culling을 수행한다.
- 최소 64개 또는 후보의 1/8 이상이 줄어들어야 결과를 채택한다.
- 카메라 View가 변하는 동안에는 11회 연속 가려진 후보만 숨기고, View가 안정적이면 가려진 결과를 즉시 반영한다.
- 이전 결과 이후 새로 Frustum에 들어온 후보는 보수적으로 계속 표시한다.

ObjViewer 설정 패널에서 Occlusion Culling을 켜고 끌 수 있으며, 제출 수·생존 후보·절감 수·캐시 사용 여부를 확인할 수 있습니다.

관련 코드:

- `ZZupEngine/Source/Engine/Render/Renderer/Renderer.cpp`
- `ZZupEngine/Source/Engine/Render/Renderer/Renderer.h`
- `ZZupEngine/Source/Engine/Render/Device/D3DDevice.cpp`
- `ZZupEngine/Shaders/DepthPrepass.hlsl`
- `ZZupEngine/Shaders/HiZDownsample.hlsl`
- `ZZupEngine/Shaders/OcclusionCull.hlsl`
- `ZZupEngine/Source/Misc/ObjViewer/UI/ObjViewerControlWidget.cpp`

## 3. 평균 FPS: Render Data 캐싱과 상태 변경 감소

매 프레임 Scene의 모든 Component를 다시 순회해 렌더 데이터를 만드는 대신, `FRenderDataManager`가 World 활성화 시 다음 데이터를 한 번 수집합니다.

- Primitive Component 배열
- Component별 AABB
- World Matrix와 Inverse World Matrix
- Render Command
- 메시 CPU 데이터와 Mesh BVH 참조

World Matrix 배열은 GPU Structured Buffer로 한 번 업로드하고, Vertex Shader는 Render Command가 전달하는 Component Index로 행렬을 조회합니다. 일반 `FRenderBus`에 수집된 Opaque Static Mesh Command는 제거해 중복 Draw를 막고, ObjViewer의 전용 성능 경로로 제출합니다.

가시 Render Command는 Mesh Buffer를 우선하고 Diffuse SRV를 다음 기준으로 정렬합니다. Renderer는 직전에 사용한 Command Type, Mesh Buffer, Diffuse SRV, Material Constant와 Component Index를 캐시해 값이 바뀐 경우에만 Shader, VB/IB, SRV, Constant Buffer를 갱신합니다.

```text
World 활성화
  → Component / AABB / Render Command / Matrix 수집
  → World Matrix Structured Buffer 업로드
  → Frustum·Occlusion으로 가시 Command 선택
  → Mesh Buffer·Diffuse SRV 기준 정렬
  → 변경된 GPU State만 갱신하고 Draw
```

관련 코드:

- `ZZupEngine/Source/Misc/ObjViewer/Render/RenderDataManager.h`
- `ZZupEngine/Source/Misc/ObjViewer/Render/RenderDataManager.cpp`
- `ZZupEngine/Source/Misc/ObjViewer/ObjViewerRenderPipeline.cpp`
- `ZZupEngine/Source/Engine/Render/Renderer/Renderer.cpp`

## 4. 평균 FPS: LOD와 메시 단순화

`UStaticMesh`는 원본 메시 외에 최대 네 단계의 간소화 LOD를 생성해 최대 다섯 레벨을 보관합니다. 최소 6개 삼각형이 있는 메시를 대상으로 같은 위치의 Vertex를 Topology Vertex로 묶고, 각 Vertex의 Quadric과 Edge Collapse 비용을 계산합니다. UV Seam에는 큰 Penalty를 부여하고 Open Boundary Vertex는 Collapse 대상에서 제외해 형태와 Texture 경계를 보존합니다.

첫 간소화 단계는 원본 삼각형 수의 약 90%를 목표로 합니다. 한 단계가 저장될 때마다 목표 비율에 0.6을 곱하고, 그 비율을 현재 삼각형 수에 적용해 다음 목표를 계산합니다. 최소 목표는 16개 삼각형이며 Collapse가 불가능하면 더 이른 단계에서 생성을 종료합니다.

`FRenderDataManager`는 Bounding Sphere 반지름, Camera 거리와 Vertical FOV로 화면 점유율을 계산하고 `0.05`, `0.03`, `0.01`, `0.008` 임계값에 따라 LOD를 선택합니다. 선택 결과가 바뀌면 Render Command의 Mesh Buffer와 Index 범위를 갱신합니다.

관련 코드:

- `ZZupEngine/Source/Engine/Asset/StaticMesh.h`
- `ZZupEngine/Source/Engine/Asset/StaticMesh.cpp`
- `ZZupEngine/Source/Misc/ObjViewer/Render/RenderDataManager.cpp`

## 5. Picking 시간과 공통 수학 기반

### 2단계 BVH Picking

왼쪽 Mouse Button으로 Drag를 시작할 때 Viewport 좌표로 World Ray를 생성하고 다음 순서로 가장 가까운 Actor를 찾습니다.

```text
Screen Position
  → World Ray
  → Top-level BVH: Component AABB 후보 수집·거리순 정렬
  → 후보의 World Ray를 Mesh Local Space로 변환
  → Mesh BVH: Node AABB와 Triangle 교차 검사
  → World 거리 기준 가장 가까운 Actor 선택
```

Top-level BVH의 후보 진입 거리가 현재까지 찾은 최근접 Hit 거리보다 멀면 남은 후보 검사를 종료합니다. Mesh BVH에서도 가까운 Child를 먼저 순회하고 현재 Hit보다 먼 Node를 제외해 실제 Triangle 교차 검사 수를 줄입니다. Render Pipeline을 사용할 수 없는 경우에는 Component를 모두 순회하는 Fallback 경로를 사용합니다.

### K-d Tree

Picking은 먼저 월드 Component AABB를 BVH로 좁히고, 남은 후보를 메시 삼각형 BVH로 검사하는 두 단계 구조입니다. FKDTree는 이와 별도로 메시 삼각형을 대상으로 공간 분할 방식을 비교하기 위해 구현한 실험입니다. 삼각형 AABB에서 split 후보를 만들고 SAH 비용으로 분할 위치를 선택합니다. Leaf 최대 크기는 2개, 최대 깊이는 20으로 설정되어 있으며 RayCast를 제공합니다.

월드 Component를 대상으로 하는 BVH와 달리 K-d Tree는 삼각형 수준의 가속 구조를 학습하기 위한 구현입니다. 최종 ObjViewer의 Picking 경로는 Mesh BVH를 사용하므로, K-d Tree는 통합 경로가 아닌 독립 실험으로 문서화합니다.

### SIMD

벡터·행렬·AABB·Plane·Frustum 계산에는 DirectXMath의 XMVECTOR 계열 연산을 사용합니다. AABB 병합, Plane 거리 계산, Frustum 평면 추출, Transform 연산을 SIMD 친화적 수학 계층 위에서 수행합니다.

SIMD 전후만을 분리해 비교한 벤치마크 결과는 포함하지 않아, 수치 성과 대신 적용 범위만 기술합니다.

관련 코드:

- `ZZupEngine/Source/Misc/ObjViewer/Viewport/ObjViewerViewportClient.cpp`
- `ZZupEngine/Source/Misc/ObjViewer/ObjViewerRenderPipeline.cpp`
- `ZZupEngine/Source/Engine/Spatial/KDTree.h`
- `ZZupEngine/Source/Engine/Spatial/KDTree.cpp`
- `ZZupEngine/Source/Engine/Geometry/AABB.cpp`
- `ZZupEngine/Source/Engine/Geometry/Frustum.cpp`
- `ZZupEngine/Source/Engine/Math/Vector.h`

## 6. 평균 FPS와 Picking 시간 측정

ObjViewer 좌측 상단 오버레이는 현재 Frame의 `1 / DeltaTime`으로 계산한 순간 FPS와 Frame Time을 표시합니다. 이는 평가에 사용된 구간 평균 FPS를 자체 누적하는 값은 아니므로, 평균 성능 비교에는 같은 Scene·Camera·측정 구간을 고정한 별도 기록이 필요합니다.

Picking은 왼쪽 클릭 처리 전체를 `FScopeCycleCounter`로 감싸 `Cycles64` 차이를 Millisecond로 변환합니다. 누적 횟수, 누적 시간, 최근 시간과 평균 시간을 `FObjViewerStatWidget`에서 바로 확인할 수 있습니다.

일반 CPU 프로파일링은 별도의 `FScopedTimer`가 `QueryPerformanceCounter`를 RAII 방식으로 수집해 `FStatManager`에 누적합니다. 이 기능과 Render Queue 통계는 `STATS`가 활성화되는 Debug Build에서만 동작합니다.

GPU 통계는 D3D11 Timestamp Query와 Disjoint Query를 이용합니다. 두 Frame Slot을 교차해 결과를 읽고, 준비되지 않은 Query는 대기 없이 건너뛰어 CPU Stall을 피합니다. ObjViewer의 `Render Queue Stats` 창은 Pass별 Command, Shader Type 변경, Mesh·SRV 재바인딩, Constant Buffer 갱신 횟수와 GPU 시간을 표시합니다.

평가 결과를 재현하려면 다음 조건을 고정해야 합니다.

- 동일한 Scene과 Camera Transform
- 동일한 해상도·Viewport 크기와 Debug/Release 구성
- Occlusion Culling On/Off 및 충분한 Readback 안정화 시간
- 동일한 Picking 위치·횟수와 최초 클릭 전 Warm-up 조건

관련 코드:

- `ZZupEngine/Source/Engine/Runtime/Stats/ScopeCycleCounter.h`
- `ZZupEngine/Source/Engine/Core/Logging/Stats.h`
- `ZZupEngine/Source/Engine/Core/Logging/GPUProfiler.h`
- `ZZupEngine/Source/Engine/Core/Logging/GPUProfiler.cpp`
- `ZZupEngine/Source/Misc/ObjViewer/UI/ObjViewerStatWidget.cpp`
- `ZZupEngine/Source/Misc/ObjViewer/UI/ObjViewerRenderQueueWidget.cpp`

## 빌드 및 실행

### 요구 환경

- Windows 10/11
- Visual Studio 2022와 MSVC v143 C++ 도구 집합
- Windows 10 SDK
- Direct3D 11 지원 GPU와 드라이버
- NuGet 패키지 복원: `directxtk_desktop_win10` 2025.10.28.2

### 실행 방법

1. `GenerateProjectFiles.bat`을 실행해 Visual Studio 프로젝트 파일을 생성합니다.
2. `ZZupEngine.sln`을 Visual Studio 2022에서 열고 NuGet 패키지를 복원합니다.
3. `Debug | x64` 또는 `Release | x64` 구성을 선택해 `ZZupEngine`을 빌드·실행합니다.

별도의 ObjViewer Build Configuration은 없습니다. `EngineLoop.cpp`가 `UObjViewerEngine`을 생성하므로 일반 Debug/Release 실행이 ObjViewer로 시작됩니다. Debugger Working Directory는 프로젝트의 `ZZupEngine` 폴더로 설정되어 있습니다.

> 현재 `ReleaseBuild.bat`은 존재하지 않는 4주차 프로젝트명 `W4_Jungle_Team2`를 참조합니다. 이 스크립트를 사용하려면 Solution, Project, EXE와 복사 경로를 현재 `ZZupEngine` 구조에 맞게 수정해야 합니다.

## 구현 범위와 제한 사항

- ObjViewer는 평균 FPS와 Picking 시간을 분리해 관찰하기 위한 단일 뷰포트 실험 환경입니다. 다중 뷰포트 PIE 환경의 비용 특성은 이 문서 범위에 포함하지 않습니다.
- Render Data와 Top-level BVH는 World 활성화 시 구축되며 Runtime Transform·Mesh·Material 변경에 맞춰 자동 갱신되지 않습니다. 정적 대량 Mesh Scene을 전제로 한 최적화 경로입니다.
- Frustum BVH는 Component Index를 반환하지만 현재 `SetVisibleIndices`는 이를 Render Command Index로 직접 사용합니다. Component 하나가 여러 Section Command를 생성하면 1:1 대응이 깨지므로 명시적인 Component-to-Command 변환이 필요합니다.
- Occlusion Culling은 상위 Hi-Z mip을 사용하지 않고 mip 0을 다점 Sampling합니다. Depth Prepass·Compute Dispatch·Readback 비용이 추가되므로 Frustum 후보가 256개 미만이거나 최소 절감량을 만족하지 못하면 결과를 채택하지 않습니다.
- Occlusion의 GPU 입력은 최대 65,536개입니다. 이를 넘는 후보는 해당 제출에서 검사하지 않고 Temporal Survivor 구성 시 보수적으로 유지됩니다.
- QEM LOD에는 전환 Hysteresis나 Dither가 없어 임계값 부근에서 Pop이 발생할 수 있습니다. LOD 1 이상은 모든 Section을 하나로 합치므로 다중 Material Mesh에서는 첫 번째 Render Command의 Material로 그려집니다.
- `FBVH::RayQueryTriangle`, `RayQueryTriangleClosest`, `RefitBVH`, `ReBuildBVH`는 완성된 실행 경로가 아닙니다. 현재 Picking은 별도 Mesh BVH 순회를 사용하고 Top-level BVH는 동적 Refit을 하지 않습니다.
- K-d Tree는 구현되어 있으나 최종 Picking 경로에는 연결되지 않았습니다.
- Octree 관련 구현은 프로젝트 코드에서 확인되지 않았습니다.
- `Render Queue Stats`는 전용 Static Mesh Render 뒤에 호출되는 일반 `Render(FRenderBus)` 시작 시 통계 배열을 초기화합니다. 따라서 현재 UI의 Opaque 수치와 GPU Pass 시간은 최적화 전용 Static Mesh 경로 전체를 온전히 대표하지 않습니다.
- 체크인된 코드만으로는 일관된 성능 향상 수치를 재현할 벤치마크 환경·측정 표가 부족합니다. 따라서 임의의 FPS 향상 수치는 기재하지 않았습니다.
