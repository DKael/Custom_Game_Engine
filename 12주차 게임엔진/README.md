# KraftonEngine — Week 12 Particle System, Translucent Rendering & Symbol Server

> **Particle System · Module · Distribution · Sprite/Mesh/Beam/Ribbon · Translucent Pass · LOD · Collision · Particle Editor · Symbol Server**

Windows 환경에서 동작하는 Win32 + DirectX 11 기반 자체 엔진에 Particle System을 구축한 프로젝트입니다.

Particle을 데이터와 동작 Module의 조합으로 구성하고, Sprite·Mesh·Beam·Ribbon Emitter를 하나의 실행 흐름에서 시뮬레이션합니다. 대량의 Particle을 처리하기 위한 연속 메모리 구조와 Game Thread–Render Thread 데이터 전달, 반투명 렌더링 순서, 거리 기반 LOD, Collision·Event, 전용 Particle Editor도 함께 구현했습니다.

Release 결과물의 Crash Dump를 정확한 PDB 및 소스와 연결하기 위한 Symbol Server 자동화도 12주차 범위에 포함했습니다.

이 문서는 12주차 과제의 학습 목표와 현재 코드에서 확인되는 구현 범위를 기준으로 작성했습니다.

## 학습 목표

- 반투명 렌더링의 Alpha Blending과 렌더링 순서를 이해한다.
- Sprite와 Mesh Particle의 데이터 구성과 렌더링 방식을 이해한다.
- 대량의 Particle을 위한 연속 메모리, Precache, Stride와 Address Alignment를 설계한다.
- Particle의 생성·갱신·소멸을 Module 조합으로 제어한다.
- Distribution을 사용해 Particle 속성을 상수·난수·Curve로 표현한다.
- Particle System Component를 World에 배치하고 Show Flag와 Stat으로 상태를 확인한다.
- Particle System Editor에서 Emitter, LOD, Module과 Curve를 편집한다.
- Particle LOD, Collision Event, Beam과 Ribbon Emitter를 구현한다.
- PDB, Source Indexing과 Symbol Store의 역할을 이해하고 Symbol Server 흐름을 구축한다.

## 구현 요약

| 구분 | 구현 내용 |
| --- | --- |
| Particle Asset | `UParticleSystem → UParticleEmitter → UParticleLODLevel → UParticleModule` 계층 구조 |
| Runtime | `UParticleSystemComponent`와 `FParticleEmitterInstance` 기반 Spawn·Update·Collision·Kill 처리 |
| Memory | 16바이트 단위 Particle Stride·Module Payload Offset과 단일 Runtime Storage |
| Distribution | Float·Vector Constant, Uniform, Curve, Uniform Curve |
| Renderer | Sprite·Mesh GPU Instancing, Beam·Ribbon Dynamic Strip 생성 |
| Translucency | 통합 Translucent Pass, Depth Read Only, Alpha·Additive·Modulate Blend |
| LOD | 거리 기반 자동 선택, Hysteresis, Switch Delay와 LOD별 Module 구성 |
| Collision·Event | CPU Collision Query, Bounce·Stop·Kill, Spawn·Death·Collision·Burst Event |
| Editor | 독립 Preview World, Emitter·LOD·Module·Distribution Curve 편집 |
| Diagnostics | Particle Show Flag, Bounds 표시, `stat particles` 오버레이 |
| Symbol Server | Release Packaging, PDB Source Indexing, Symbol Store 업로드와 Build Metadata |

## 전체 Particle 흐름

```text
Particle System Asset (.uasset)
  └─ UParticleSystem
       └─ UParticleEmitter
            └─ UParticleLODLevel
                 ├─ Required Module
                 ├─ Spawn Module
                 ├─ TypeData Module
                 └─ Lifetime / Location / Velocity / ...
                            │
                            ▼
                 UParticleSystemComponent
                            │
                            ▼
                 FParticleEmitterInstance
                            │
              Spawn → Update → Collision → Kill
                            │
                            ▼
                  Runtime Particle Storage
                            │
                BuildDynamicData / Snapshot
                            │
                            ▼
               FParticleSystemSceneProxy
                            │
        ┌───────────────────┼───────────────────┐
        ▼                   ▼                   ▼
 Sprite / Mesh Factory   Beam Factory       Ribbon Factory
 GPU Instancing          Dynamic Strip      Dynamic Trail
        └───────────────────┴───────────────────┘
                            │
                            ▼
            DrawCommand → Translucent Pass
```

Asset은 효과의 구성 정보를 보유하고, Component와 Emitter Instance가 World에서 실제 재생 상태를 관리합니다. Game Thread에서 계산한 활성 Particle은 Snapshot으로 복사되며, Render Thread는 이 Snapshot만 소비해 Emitter별 Draw Section을 생성합니다.

## 1. Particle System 데이터 구조

Particle Asset은 다음 네 계층으로 구성됩니다.

| 계층 | 역할 |
| --- | --- |
| `UParticleSystem` | 여러 Emitter와 전체 LOD 거리 설정을 보유하는 `.uasset` |
| `UParticleEmitter` | 불꽃·연기·빗방울처럼 하나의 효과 요소와 여러 LOD Level을 관리 |
| `UParticleLODLevel` | 특정 거리 단계에서 사용할 Core Module과 일반 Module 구성 |
| `UParticleModule` | Spawn, Lifetime, Velocity 등 하나의 동작 또는 속성을 정의 |

각 LOD Level은 다음 Module Slot을 사용합니다.

- **Required Module**: Material, Local Space, Screen Alignment, SubUV 분할과 Sort Mode를 정의합니다.
- **Spawn Module**: 초당 생성량, Rate Scale과 Burst 목록을 정의합니다.
- **TypeData Module**: 기본 Sprite 또는 Mesh·Beam·Ribbon 중 렌더링 형태를 선택합니다.
- **일반 Module**: Lifetime, Location, Velocity, Acceleration, Color, Size, SubUV, Collision과 Event 동작을 조합합니다.

`UParticleSystem`은 Content Browser에서 생성하고 Binary `.uasset`으로 저장할 수 있습니다. `FParticleSystemManager`가 Package Header를 검사해 Particle System Asset만 로드하고, 이미 로드한 Asset은 Path 기준으로 캐시합니다.

## 2. 런타임 시뮬레이션과 메모리

### Particle Layout Precache

모든 Particle은 위치, 속도, 색상, 크기, 회전, 수명과 상태 Flag를 담는 `FBaseParticle`을 공통 Header로 사용합니다. Module이 추가 상태를 필요로 하면 `RequiredBytes()`로 Payload 크기를 알리고, Emitter가 Module별 Byte Offset을 미리 계산합니다.

```text
Particle Stride
  ├─ FBaseParticle
  ├─ Lifetime Payload
  ├─ Acceleration Payload
  ├─ Collision Payload
  └─ Module-specific Payload
```

Particle Stride와 Module Payload Offset은 16바이트 단위로 올림 처리합니다. 모든 LOD의 Module Layout을 Emitter 단위로 Precache하므로, 서로 다른 LOD에서 생성된 Particle이 하나의 Emitter Instance 안에 함께 존재할 수 있습니다.

`FParticleStorage`는 다음 영역을 하나의 연속된 Byte Block으로 소유합니다.

```text
MemBlock
  ├─ ParticleData[MaxParticles × Stride]
  ├─ ParticleIndices[MaxParticles]
  └─ Per-instance Module Data
```

이 구조는 Particle마다 별도 Heap Allocation을 수행하지 않고, `Particle + ModuleOffset`으로 Payload에 접근할 수 있게 합니다. Emitter당 활성 Particle은 안전 가드에 의해 최대 16,384개로 제한됩니다.

### Spawn, Update와 Compaction

`UParticleSystemComponent::TickComponent`가 현재 LOD를 선택한 뒤 각 `FParticleEmitterInstance`를 갱신합니다.

```text
Tick
  → Spawn Rate / Burst 계산
  → 새 Particle 초기화 및 Spawn Module 적용
  → Lifetime과 일반 Update Module 적용
  → Collision Query와 Response 처리
  → 수명이 끝났거나 Kill된 Particle 제거
  → 살아 있는 Particle을 앞으로 모아 Compaction
```

새 Particle은 생성 시점의 LOD Index를 저장합니다. 이후 Component의 현재 LOD가 바뀌더라도 이미 살아 있는 Particle은 자신이 생성된 LOD의 Simulation Module을 따라가며, 새로 생성되는 Particle부터 변경된 LOD를 사용합니다.

`TickInterval`을 지정하면 Particle Simulation 호출 빈도를 낮출 수 있으며, 누적된 시간을 사용해 설정된 간격마다 갱신합니다.

### Game Thread에서 Render Thread로 전달

Game Thread의 Runtime Storage를 Render Thread가 직접 참조하지 않습니다. `BuildDynamicData()`가 활성 Particle과 Emitter별 렌더링 설정을 Replay Snapshot으로 복사해 Scene Proxy에 전달합니다.

```text
RuntimeStorage (Game Thread)
  → 활성 Particle Snapshot 복사
  → Material / SortMode / LocalToWorld / TypeData 기록
  → FParticleSystemSceneProxy::SetDynamicData
  → VertexFactory::BuildDraw
```

Game Thread는 Simulation 결과의 작성 권한을 유지하고, Render Thread는 전달받은 Snapshot을 읽기 전용으로 소비합니다. Emitter별 Dynamic Vertex·Index Buffer Slot을 따로 사용해 여러 Emitter가 서로의 렌더링 데이터를 덮어쓰지 않게 했습니다.

## 3. Module과 Distribution

Particle의 동작은 상속으로 고정하지 않고 Module을 조립해 구성합니다.

| 분류 | 구현 Module |
| --- | --- |
| 생성 | Spawn Rate, Rate Scale, Burst |
| 수명 | Lifetime |
| 위치·운동 | Initial Location, Velocity, Acceleration |
| 외형 | Initial Color, Color Over Life, Initial Size, Size By Life |
| Animation | SubUV, SubUV Movie |
| 충돌 | Collision |
| Event | Event Generator, Receiver Spawn, Receiver Kill All |
| Beam | Source, Target, Noise |

Module의 수치는 `UDistribution` 계층으로 표현합니다.

- **Constant**: 고정된 값을 반환합니다.
- **Uniform**: Min–Max 범위에서 무작위 값을 선택합니다.
- **Curve**: 시간에 따른 값을 Curve Key로 평가합니다.
- **Uniform Curve**: 시간에 따라 변하는 Min–Max Curve 범위에서 값을 선택합니다.

Float와 Vector Distribution을 모두 제공하며, Lifetime·Location·Velocity·Acceleration·Color·Size·SubUV·Beam 속성에 연결합니다. 같은 Module도 Distribution 구성을 바꾸면 서로 다른 효과를 만들 수 있어 절차적인 Particle 제작이 가능합니다.

## 4. Sprite, Mesh, Beam과 Ribbon 렌더링

`FParticleVertexFactory`의 `BuildDraw()` 인터페이스 아래에서 Emitter Type별로 Draw Buffer를 구성합니다.

| Type | Geometry 생성 | Draw 방식 | 주요 기능 |
| --- | --- | --- | --- |
| Sprite | 정적 Unit Quad + Instance Data | `DrawIndexedInstanced` | Billboard, Rotation, Color, SubUV |
| Mesh | Static Mesh LOD 0 + Instance Transform | `DrawIndexedInstanced` | Mesh Transform, Color, SubUV |
| Beam | Source–Target 기반 동적 Quad Strip | `DrawIndexed` | Hermite Curve, Noise, Taper, UV Tiling |
| Ribbon | 활성 Particle을 연결한 동적 Trail Strip | `DrawIndexed` | Tangent, Tessellation, Trail UV |

### Sprite와 Mesh Instancing

Sprite는 입자마다 네 개의 완성된 정점을 CPU에서 만들지 않습니다. 정적 Unit Quad를 Slot 0에 두고 Center, Velocity, Size, Rotation, Color, SubImage Index를 Instance Buffer로 전달합니다. Vertex Shader가 Screen Alignment에 맞춰 Quad를 확장합니다.

Mesh Particle도 Static Mesh Vertex·Index Buffer를 공유하고 Particle별 World Transform, Color와 SubImage Index만 Instance Buffer로 전달합니다. 따라서 하나의 Emitter는 활성 Particle 수와 관계없이 하나의 Instanced Draw Section으로 제출할 수 있습니다.

### Beam과 Ribbon Dynamic Strip

Beam은 Source와 Target, Tangent, Noise 설정을 이용해 Render Thread에서 Strip 정점과 인덱스를 생성합니다. Camera 방향에 수직인 폭을 계산하고, Hermite Curve와 Noise를 적용해 굽거나 흔들리는 Beam을 표현합니다.

Ribbon은 활성 Particle을 Relative Time 순으로 정렬해 하나의 Control Point Chain을 구성합니다. Tangent를 계산하고 구간을 Tessellation한 뒤 Camera Facing Strip으로 변환합니다. 과도한 Tessellation으로 정점 수가 증가하지 않도록 Runtime Sample Budget도 적용합니다.

## 5. 반투명 렌더링과 Material

### 통합 Translucent Pass

반투명 색상은 기존 Frame Buffer의 색상과 합성되므로 렌더링 순서에 따라 결과가 달라집니다. 불투명 Geometry를 먼저 그린 후 Font, Billboard, SubUV와 Particle 같은 반투명 Geometry를 통합 Translucent Pass에서 처리합니다.

Translucent Pass는 깊이 테스트를 수행하지만 깊이 Buffer에는 값을 쓰지 않는 `DepthReadOnly` 상태를 사용합니다. Material별 Blend Mode는 DrawCommand의 Render State로 적용됩니다.

| Blend Mode | 개념적 합성 | Depth Write |
| --- | --- | --- |
| Translucent | `Src × Alpha + Dst × (1 - Alpha)` | 사용하지 않음 |
| Additive | `Src + Dst` | 사용하지 않음 |
| Modulate | `Src × Dst` | 사용하지 않음 |

### 두 단계의 정렬

엔진은 반투명 정렬을 두 계층으로 나눕니다.

1. **Draw Section 정렬**: 모든 Translucent DrawCommand에 Camera Distance 기반 Sort Key를 부여하고 먼 Section부터 그립니다. Particle은 Emitter의 평균 위치를 Section 대표 위치로 제공합니다.
2. **Emitter 내부 정렬**: Required Module의 `SortMode`가 지정된 Sprite·Mesh Emitter는 Particle Instance 순서를 View Depth, View Distance 또는 Age 기준으로 다시 배치합니다.

Translucent Sort Key는 `Pass | Depth Bucket | Shader | User Bits`로 구성됩니다. Distance를 반전한 Bucket으로 저장해 오름차순 정렬 시 먼 Object가 먼저 제출되며, 같은 Bucket에서는 Shader 기준으로 묶습니다.

### Material Domain과 Blend Mode

Material이 낮은 수준의 Render Pass, Blend, Depth, Rasterizer State를 각각 직접 지정하지 않도록 `EMaterialDomain`과 `EBlendMode`를 단일 설정 원천으로 사용합니다.

```text
Material Domain + Blend Mode
  → ResolveMaterialRenderState
  → Render Pass / Blend / Depth / Rasterizer State
```

`Surface + Translucent·Additive·Modulate`는 통합 Translucent Pass와 `DepthReadOnly` 상태로 연결됩니다. `DrawCommandBuilder`는 Material, Vertex Factory, Domain, Pass와 View Mode 조합에 맞는 Shader를 선택합니다.

Material은 Binary `.uasset`으로 저장하며, D3D Shader Reflection에서 Constant Buffer Offset과 Texture Slot을 가져옵니다. Material Editor에서 Domain, Blend Mode, Shader, Texture Slot과 Two-Sided를 편집할 수 있고, `UMaterialInstance`는 Parent Material의 Parameter와 Texture를 Instance 단위로 재정의합니다.

## 6. Particle LOD

`UParticleSystemComponent`는 Camera와의 거리를 기준으로 현재 LOD Index를 선택합니다. 각 Emitter는 LOD별로 Required, Spawn, TypeData와 일반 Module 구성을 가질 수 있습니다.

LOD 경계에서 Camera가 움직일 때 단계가 빠르게 왕복하지 않도록 두 가지 안정화 설정을 제공합니다.

- **Distance Hysteresis**: LOD 진입과 이탈 경계에 간격을 둡니다.
- **Switch Delay**: 목표 LOD가 일정 시간 유지된 경우에만 전환합니다.

낮은 LOD에서는 Spawn량과 Collision Query Budget을 줄이거나 Collision Event를 제한할 수 있습니다. 이미 생성된 Particle은 Spawn 시점 LOD의 Simulation 계약을 유지하고, 현재 LOD는 새 Particle과 외부 Collision 비용 정책에 사용됩니다.

## 7. Collision과 Particle Event

### CPU Particle Collision

Particle의 이전 위치와 현재 위치 사이를 World Collision Query로 검사합니다. 충돌이 확인되면 법선과 충돌 속도를 바탕으로 다음 Response를 적용합니다.

- **Bounce**: 법선·접선 방향 감쇠를 적용해 속도를 반사합니다.
- **Stop**: 표면에서 Particle 이동을 정지합니다.
- **Kill**: Particle을 즉시 제거합니다.

`MaxCollisions` 이후 동작과 반복 충돌 억제, 낮은 속도에서의 안정화 처리를 지원합니다. 대량의 Particle Query를 제어하기 위해 Emitter Bounds 기반 사전 검사, 주변 Collider Cache, 후보 우선순위와 LOD별 Query Budget을 적용합니다.

### Event Generator와 Receiver

Spawn, Death, Collision, Burst 시점의 Event Payload를 Emitter Queue에 기록합니다. `UParticleSystemComponent`가 Queue를 수집해 다음 대상으로 전달합니다.

```text
Particle Event
  ├─ 같은 Particle System의 Receiver Module
  │    ├─ Event 위치에서 추가 Particle 생성
  │    └─ 현재 Emitter의 모든 Particle 제거
  └─ AParticleEventManager
       └─ Spawn / Death / Collision / Burst Multicast Delegate
```

외부 Gameplay Code는 `AParticleEventManager`의 Delegate에 연결해 Particle Event를 받을 수 있습니다. Event Manager가 없어도 Particle Simulation과 Rendering은 계속되지만 외부 Event는 전달되지 않습니다.

## 8. Particle System Editor

Particle System Asset을 열면 전용 Editor에서 결과를 확인하며 구조와 속성을 편집할 수 있습니다.

| 영역 | 기능 |
| --- | --- |
| Preview Viewport | 독립 Editor Preview World에서 Particle 재생 |
| Emitter Strip | Emitter와 LOD별 Required·Spawn·TypeData·일반 Module 표시 |
| Details | System, Emitter, LOD와 Module 속성 편집 |
| Curve Editor | Distribution Curve의 Key, 보간과 Tangent 편집 |

Editor에서 다음 작업을 지원합니다.

- Emitter 추가·삭제 및 순서 변경
- LOD 선택과 Preview 적용
- Module 추가·복제·삭제·활성화 및 Drag & Drop 순서 변경
- Sprite, Mesh, Beam, Ribbon TypeData 선택
- Distribution 종류와 값 편집
- Curve Key 추가·이동·삭제와 보간 방식 변경
- Preview Simulation 재시작
- Particle Bounds 표시
- 변경된 Particle Asset 저장

Preview는 실제 `UParticleSystemComponent`를 사용하므로 Runtime과 동일한 Simulation·Rendering 경로를 확인할 수 있습니다.

## 9. Show Flag와 Particle Stat

Viewport Toolbar에서 다음 Show Flag를 제공합니다.

- `bParticles`: Particle System 렌더링을 켜거나 끕니다.
- `bParticleBounds`: Particle System의 동적 AABB를 표시합니다.

Console에서 `stat particles`를 실행하면 다음 정보를 확인할 수 있습니다.

- Particle System Component와 Emitter Instance 수
- 전체·Sprite·Mesh Particle 수와 Session Peak
- 프레임별 Spawn·Kill 수
- Runtime Storage의 전체·활성·예약 Memory
- 제출된 Particle Draw Section 수
- Sprite·Mesh Instance와 Vertex·Index 수
- Beam Strip과 Ribbon Tessellation 결과
- Particle Simulation 및 Draw 준비 구간의 CPU Stat

Particle Bounds는 활성 Particle을 기준으로 갱신되며 View Frustum Culling에 사용됩니다. 반투명 Billboard가 갑자기 사라지는 현상을 피하기 위해 Particle Proxy는 Occlusion Culling 대상에서 제외합니다.

## 10. Symbol Server와 Release Pipeline

Crash Dump를 다른 PC에서 분석하려면 실행 파일과 정확히 일치하는 PDB가 필요합니다. 12주차에서는 Release Build, PDB Source Indexing, Symbol Store 업로드와 Build Metadata 생성을 하나의 흐름으로 구성했습니다.

```text
PackageRelease.bat
  → BuildInfo.h 생성
  → Release x64 빌드
  → 실행 파일·DLL·Content·Shader 패키징
  → PDB에 srcsrv Stream 삽입
  → PDB Source 정보 검증
  → PDB·EXE·DLL을 Symbol Store에 업로드
  → ReleaseBuild/BuildInfo.txt 생성
```

| 도구 | 역할 |
| --- | --- |
| `srctool.exe` | PDB가 참조하는 Source 목록과 미인덱싱 파일 확인 |
| `pdbstr.exe` | PDB의 `srcsrv` Stream 읽기·쓰기 |
| `symstore.exe` | PDB, EXE와 DLL을 Version별 Symbol Store에 등록 |

`Scripts/GenerateSrcSrvStream.ps1`은 현재 Git Commit과 GitHub Remote를 기준으로 프로젝트 Source의 Raw URL을 만들고 PDB에 기록합니다. Crash 시 생성되는 Dump 옆에는 Build Version, Git Commit, Symbol Path와 Executable 정보가 담긴 Build Info 파일도 생성됩니다.

자세한 운영 및 Dump 분석 절차는 `Docs/SymbolServerGuide.md`에 정리되어 있습니다.

## 빌드 및 실행

### 요구 환경

- Windows 10/11
- Visual Studio 2022와 MSVC v143 Toolset
- C++ 데스크톱 개발 도구와 Windows 10 SDK
- DirectX 11 지원 GPU
- NuGet Package Restore가 가능한 환경
- Symbol Server 기능 사용 시 Debugging Tools for Windows

프로젝트는 C++20을 사용합니다. PhysX 4.1.2는 NuGet으로 복원하며, RmlUi, FMOD, Lua, FBX SDK 등 Runtime DLL은 Build 후 출력 폴더로 복사됩니다.

### 실행 방법

1. 프로젝트 루트의 `GenerateProjectFiles.bat`를 실행한다.
2. 생성된 `KraftonEngine.sln`을 Visual Studio에서 연다.
3. NuGet Package Restore가 완료됐는지 확인한다.
4. `Debug | x64` 또는 `Release | x64` 구성으로 빌드한다.
5. `KraftonEngine/Bin/<Configuration>/KraftonEngine.exe`를 실행한다.

Build 전 `Scripts/GenerateHeaders.py`가 자동으로 실행되어 Reflection Header를 갱신합니다.

### 기능 확인

1. Content Browser에서 Particle System Asset을 생성하거나 기존 `.uasset`을 연다.
2. Emitter와 LOD를 추가하고 Required·Spawn·TypeData Module을 설정한다.
3. Lifetime, Velocity, Color, Size 등의 Module과 Distribution을 조합한다.
4. Preview Viewport에서 Sprite·Mesh·Beam·Ribbon 결과를 확인한다.
5. Particle System Component를 World에 배치하고 Play한다.
6. Viewport의 `Particles`, `Particle Bounds` Show Flag와 `stat particles`를 확인한다.

### Release와 Symbol 업로드

```bat
.\PackageRelease.bat Week12_Release_20260527_2100
```

위 명령은 Release Build와 Package 생성 후 Symbol Upload까지 수행합니다. Symbol Store 공유 경로와 Windows Debugging Tools 설치 여부를 먼저 확인해야 합니다.

## 구현 범위

- Particle System, Emitter, LOD와 Module 계층을 Binary Asset으로 저장합니다.
- Module별 Payload Layout을 Precache하고 연속된 Runtime Storage에서 Particle을 관리합니다.
- Spawn, Update, Collision, Kill과 Compaction으로 Particle Lifetime을 처리합니다.
- Constant, Uniform, Curve와 Uniform Curve Distribution을 제공합니다.
- Sprite, Mesh, Beam과 Ribbon Emitter를 렌더링합니다.
- 통합 Translucent Pass에서 Alpha, Additive와 Modulate Blend를 처리합니다.
- 거리 기반 LOD와 Collision Query Budget을 제공합니다.
- Particle Event를 Receiver Module과 외부 Event Manager로 전달합니다.
- Particle Editor에서 Preview, Emitter·LOD·Module과 Curve를 편집합니다.
- Show Flag, Bounds Debug와 Particle Stat을 제공합니다.
- Release Package, Source-indexed PDB와 Symbol Store 업로드 흐름을 제공합니다.

### 제한 사항

- **GPU Instancing 범위**: Sprite와 Mesh만 GPU Instancing을 사용합니다. Beam과 Ribbon은 CPU에서 동적 Strip 정점·인덱스를 생성합니다.
- **GT–RT Snapshot 비용**: 활성 Particle의 전체 Stride 데이터를 매 갱신마다 Replay Storage로 복사하므로 Particle 수와 Payload 크기에 비례한 복사 비용이 발생합니다.
- **Memory Alignment 범위**: Particle Stride와 Module Payload Offset은 16바이트 단위로 맞추지만, 별도의 16바이트 Aligned Allocator로 `MemBlock` 시작 주소까지 명시적으로 보장하는 구조는 아닙니다.
- **Particle 수 제한**: 하나의 Emitter가 가질 수 있는 활성 Particle은 최대 16,384개이며, 프레임당 Burst 추가량에도 4,096개의 안전 제한이 있습니다.
- **Mesh Particle 범위**: 현재 Mesh Particle은 Static Mesh의 LOD 0을 사용하며, Mesh의 여러 Section과 Material을 개별 Particle Draw로 완전히 확장하지 않습니다.
- **Mesh Alignment**: Mesh Alignment와 `bOverrideMaterial` 설정은 Replay에 전달되지만, 현재 Render Thread의 Instance Orientation과 Material 선택에는 완전히 반영되지 않습니다.
- **Beam 구조**: Beam Replay는 Emitter 단위 Source·Target 형상을 사용합니다. 활성 Particle 각각이 독립적인 Endpoint를 갖는 Multi-Beam 구조는 아닙니다.
- **Ribbon 구조**: 한 Emitter의 활성 Particle을 하나의 Trail Chain으로 구성합니다. Trail ID 기반 Multi-Trail Grouping은 지원하지 않습니다.
- **Ribbon 비용 제한**: 요청한 Tessellation이 Runtime Sample Budget을 넘으면 실제 Tessellation 단계가 낮아질 수 있습니다.
- **반투명 정렬 단위**: DrawCommand 간 정렬은 Object 또는 Emitter의 대표 위치를 사용합니다. 서로 교차하는 복잡한 반투명 Mesh의 Triangle 단위 정렬 문제까지 해결하지는 않습니다.
- **Collision 비용과 정확도**: Collision은 CPU Query 기반이며, LOD·Query Budget·Emitter 사전 검사에 따라 일부 Particle의 Query가 생략될 수 있습니다.
- **Collision Filter 범위**: Collision 대상 선택은 경량 Channel 기반 정책이며 복잡한 Object Type·Response Matrix를 완전히 제공하지 않습니다.
- **Event Manager 의존성**: Particle 재생에는 Event Manager가 필요하지 않지만, 외부 Gameplay Delegate로 Event를 전달하려면 Level에 `AParticleEventManager`가 등록되어야 합니다.
- **Occlusion Culling 제외**: 반투명 Particle의 Popping을 줄이기 위해 Particle Proxy는 Occlusion Culling에서 제외되며, 가려진 효과도 Render 준비 비용이 발생할 수 있습니다.
- **Draw Call Stat 의미**: `stat particles`의 Draw Call 값은 실제 RHI 호출 완료 횟수가 아니라 DrawCommand로 제출된 Particle Section 수입니다.
- **자식 객체 정리 정책**: Emitter·LOD·Module을 제거할 때 참조에서는 제외되지만, 일부 자식 UObject의 즉시 파괴와 소유권 정리는 후속 과제로 남아 있습니다.
- **Symbol Server 환경 의존성**: 기본 Symbol Store 경로가 Team7 내부 공유 경로로 설정되어 있어 다른 환경에서는 Batch 설정을 수정해야 합니다.
- **Source Indexing 제약**: Source Indexing은 GitHub Remote와 Git에 추적된 프로젝트 Source를 대상으로 합니다. 외부 SDK·Library Source는 자동으로 인덱싱하지 않습니다.
