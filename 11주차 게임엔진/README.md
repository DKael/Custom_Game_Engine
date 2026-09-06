# JSEngine — Week 11 Animation Runtime, GPU Skinning & Development Tools

> **FBX Animation · Anim Sequence · Pose Blending · State Machine · GPU Skinning · Anim Notify · MiniDump · Reflection**

Windows 환경에서 동작하는 Win32 + DirectX 11 기반 자체 엔진에 Skeletal Animation 파이프라인을 구축한 프로젝트입니다.

FBX의 Bone Animation과 Curve를 엔진 애셋으로 변환하고, Animation State Machine과 Pose Blending을 거쳐 CPU 또는 GPU Skinning으로 렌더링합니다. Animation Sequence Viewer, Anim Notify, Bone Weight Heatmap, Crash Dump, Property Reflection처럼 애니메이션 제작과 엔진 개발을 지원하는 도구도 함께 구현했습니다.

이 문서는 11주차 과제의 학습 목표와 현재 코드에서 확인되는 구현 범위를 기준으로 작성했습니다.

## 학습 목표

- FBX Anim Stack, Anim Layer, Anim Curve와 Bone Animation 구조를 이해한다.
- `USkeleton`, `USkeletalMesh`, `UAnimSequence`, `UAnimInstance`의 역할을 분리한다.
- Animation Sequence의 재생, Pose Sampling과 Animation Blending을 구현한다.
- Parameter와 Transition을 사용하는 Animation State Machine을 구현한다.
- C++ 애니메이션 런타임을 Lua Script에서 제어한다.
- CPU Skinning과 GPU Skinning의 데이터 흐름과 성능 특성을 비교한다.
- Animation Sequence Viewer와 Timeline, Reverse Play, Anim Notify를 구현한다.
- Skeletal Mesh Viewer에서 선택 Bone의 Weight를 Heatmap으로 시각화한다.
- MiniDump와 PDB를 이용해 크래시 발생 위치와 Call Stack을 분석한다.
- Unreal 스타일 Property Reflection과 코드 생성 과정을 이해한다.

## 구현 요약

| 영역 | 구현 내용 |
| --- | --- |
| FBX Animation Import | Anim Stack별 Bone Transform을 프레임 단위로 샘플링하고 Attribute·Material·Morph Target Curve를 추출 |
| Animation Asset | Bone Track, Float Curve, Frame Rate, Notify Track, Skeleton 참조를 `UAnimSequence`로 관리 |
| Animation Compression | 원본 Bone Track을 ACL로 압축하고 런타임에서 현재 시점의 Pose를 복원 |
| Pose Evaluation | Translation·Scale은 Lerp, Rotation은 Slerp로 보간하고 두 Sequence 사이를 Crossfade |
| State Machine | Bool·Float·Int·Trigger·State Finished 조건, Any State, 우선순위 기반 Transition 지원 |
| Lua Animation | Lua에서 State Machine을 생성하고 State·Parameter·Transition을 제어 |
| Skinning | CPU 동적 Vertex Buffer 방식과 GPU Vertex Shader 방식을 런타임에서 전환 |
| Sequence Viewer | Preview, Skeleton Tree, Timeline, Curve, Notify 편집, Reverse Play, 재생 구간 제어 |
| Debug View | 선택 Bone 표시와 Bone Weight Heatmap 지원 |
| Crash Diagnostics | MiniDump, Crash Log, Call Stack, 현재·다른 Thread Snapshot 생성 |
| Property Reflection | `UCLASS`, `USTRUCT`, `UENUM`, `UPROPERTY`를 스캔해 런타임 메타데이터 자동 생성 |

## 전체 애니메이션 흐름

```text
FBX 원본
  │
  ├─ Anim Stack / Layer / Curve 탐색
  ├─ Bone Local Transform 프레임 샘플링
  └─ Attribute / Material / Morph Target Curve 추출
  │
  ▼
UAnimSequence + UAnimDataModel
  ├─ Bone Animation Track
  ├─ Float Curve
  ├─ Notify Track
  └─ ACL Compressed Runtime Data
  │
  ▼
UAnimInstance / UAnimationStateMachine
  ├─ 현재 Sequence 시간 진행
  ├─ Transition 평가
  ├─ Pose Sampling
  ├─ Pose Blending
  └─ Anim Notify Dispatch
  │
  ▼
USkinnedMeshComponent
  ├─ CPU Skinning → Dynamic Vertex Buffer
  └─ GPU Skinning → Bone Matrix SRV + Vertex Shader
  │
  ▼
Skeletal Mesh Render Pass
```

## 1. FBX Animation Import

`FFbxImporter`는 Skeletal FBX에서 Skeleton과 Mesh뿐 아니라 Animation Stack을 함께 읽습니다. 각 Stack의 Local Time Span과 Scene Frame Rate를 기준으로 재생 구간과 프레임 수를 계산하며, 유효하지 않은 시간 범위는 건너뜁니다.

각 프레임에서는 Bone의 Global Transform을 평가한 뒤 Parent Transform을 기준으로 Local Transform을 재구성합니다. 변환된 Position, Rotation, Scale은 `FBoneAnimationTrack`의 `FRawAnimSequenceTrack`에 저장됩니다.

```text
FbxAnimStack
  → Time Span / Frame Rate
  → Frame별 Bone Global Transform
  → Parent 기준 Local Transform
  → Position / Rotation / Scale Track
```

Animation Curve는 Bone Transform Track과 별도로 수집합니다.

- Custom Attribute Curve
- Material Property Curve
- Morph Target Curve
- 복수 Anim Layer와 Channel을 구분한 Curve 이름
- Curve Type과 Source Kind에 대한 Skeleton Metadata

Custom Attribute Curve 중 모든 값이 0인 데이터는 제외해 불필요한 Curve 생성을 줄입니다.

### 애니메이션 객체의 역할

| 객체 | 역할 |
| --- | --- |
| `USkeleton` | Bone 계층, Bind Transform, Curve Metadata를 공유하는 Skeleton 애셋 |
| `USkeletalMesh` | 정점·인덱스·Bone Weight·Material Section과 Skeleton 참조를 보유 |
| `UAnimSequence` | 특정 Skeleton에서 재생할 Animation Data와 Notify를 보유 |
| `UAnimDataModel` | Bone Track, Curve, Frame Rate, Frame·Key 수, Notify Track을 저장 |
| `UAnimInstance` | Sequence 재생, 시간 진행, Pose 평가, Blending과 Notify 실행을 담당 |

## 2. Animation Sequence와 Pose Evaluation

FBX에서 변환한 Sequence는 Bone Track을 ACL(Animation Compression Library)로 압축합니다. 런타임 Pose 평가에서는 ACL 데이터 복원을 먼저 시도하고, 압축 데이터가 없으면 원본 Track을 직접 보간합니다.

현재 시각을 Frame Rate에 맞춰 Key Index로 변환한 뒤 다음 방식으로 Local Pose를 계산합니다.

- Position: `FVector::Lerp`
- Rotation: `FQuat::Slerp` 후 Normalize
- Scale: `FVector::Lerp`
- Track이 없는 Bone: Reference Pose 유지

Sequence와 현재 Skeletal Mesh의 Skeleton이 다를 때에는 Bone 이름과 계층을 기준으로 Track Remap을 구성합니다. 호환되지 않는 Skeleton은 재생 준비 단계에서 거부합니다.

### Animation Blending

State 전환이나 `SetNextSequence()` 호출 시 현재 Sequence와 다음 Sequence의 Pose를 각각 평가합니다. `BlendFactor`가 증가하는 동안 Bone별 Position·Rotation·Scale을 보간하고, 값이 1에 도달하면 다음 Sequence를 현재 Sequence로 승격합니다.

```text
Current Sequence Pose ─┐
                       ├─ Bone별 Lerp / Slerp → Final Local Pose
Next Sequence Pose ────┘
```

## 3. Animation State Machine과 Lua

`UAnimationStateMachine`은 State, Parameter, Transition을 분리해 관리합니다.

지원 Parameter:

- Bool
- Float
- Int
- Trigger

지원 Transition 조건:

- Bool Equals
- Float Greater
- Float Less or Equal
- Int Equals
- Trigger
- State Finished
- C++ Native Predicate

현재 State와 `Any` State에서 출발하는 Transition을 후보로 수집하고, 높은 Priority부터 조건을 평가합니다. 조건을 만족하면 Target State의 Sequence, Loop 여부, Play Rate와 Blend Speed를 `UAnimInstance`에 전달합니다.

`UAnimInstanceAsset`은 State와 Transition 구성을 애셋으로 저장하며, 에디터의 Anim Instance 편집 UI에서 해당 구성을 다룰 수 있습니다.

### Lua State Machine

`ULuaAnimInstance`는 Lua Script를 로드해 `NativeInitializeAnimation`과 `NativeUpdateAnimation`을 호출합니다. Lua Binding을 통해 다음 작업을 수행할 수 있습니다.

- State Machine 생성과 조회
- 경로 또는 소유 Mesh의 Index로 State 추가
- Entry State 설정
- Parameter 등록·변경·조회
- 조건별 Transition 추가
- 현재 State 이름 조회

`Asset/Script/LuaAnimation.lua`는 이동 방향을 Int Parameter로 전달하고, `Any` State에서 Idle·Forward·Left·Backward·Right State로 전환하는 실제 사용 예제입니다.

## 4. 재생 제어, Reverse Play와 Anim Notify

`UAnimInstance`는 Play, Pause, Stop, Loop, Play Rate와 Current Time을 관리합니다. 음수 Play Rate에서는 시간이 역방향으로 진행하며, Loop 경계를 넘을 때도 재생 구간 안으로 시간을 보정합니다.

Anim Notify 수집은 재생 방향과 Loop Wrap을 함께 고려합니다.

- 정방향: Previous Time부터 Current Time 사이의 Event 수집
- 역방향: Current Time부터 Previous Time 사이의 Event 수집
- Loop 경계 통과: 끝 구간과 시작 구간을 나눠 수집
- Notify State: Begin, Tick, End 생명주기 처리

기본 제공 Notify:

- `UAnimNotify_PlaySFX`
- `UAnimNotify_GameplayEvent`
- `UAnimNotify_CameraShake`
- `UAnimNotify_FootstepSurfaceEvent`
- `UAnimNotify_PlayVFX`
- `UAnimNotify_SpawnDecal`
- `UAnimNotifyState_GameplayEventWindow`
- `UAnimNotifyState_PlayLoopingSFX`
- `UAnimNotifyState_AttackWindow`

Notify는 Payload를 해석해 Audio, Gameplay Event, Camera Shake, VFX, Decal 기능으로 전달할 수 있으며, `ULuaAnimInstance`에서도 일반·이름 기반 Notify Callback을 받을 수 있습니다.

## 5. Animation Sequence Viewer

Animation Sequence Viewer는 기존 에디터 안에서 `.sequence` 애셋을 여는 문서형 도구입니다.

### Preview

- 독립 Preview Scene과 Skeletal Mesh Component
- Perspective·Orthographic Camera
- Skeleton Tree와 Bone 선택
- Bone 전체 또는 선택 Bone 표시
- 선택 Bone의 Weight Heatmap
- View Mode와 Show Flag

### Timeline과 재생 제어

- Play, Pause, Stop
- Forward·Reverse Play
- 첫 Frame·마지막 Frame 이동
- 이전·다음 Frame 이동
- Loop와 Playback Speed
- Playback Range와 Visible Range 분리
- Frame Snap과 Timeline Scrubbing

### Curve와 Notify 편집

- Bone Track, Float Curve, Attribute, Notify Track Outliner
- Curve Type별 필터와 선택 Curve 시각화
- Notify Track 추가·이름 변경·삭제
- Notify 생성·이동·복사·붙여넣기·삭제
- Notify와 Notify State의 Time·Duration 편집
- Class별 Payload Schema와 Validation
- 최근 발생한 Notify와 Event Log 확인

Notify 편집 결과는 Sequence 애셋에 저장되며, Stable ID를 사용해 Track 이동이나 편집 후에도 선택 상태를 추적합니다.

## 6. CPU와 GPU Skinning

`USkinnedMeshComponent`는 두 Skinning 경로에서 공통으로 Current Global Pose와 Skinning Matrix를 계산합니다.

### CPU Skinning

`SkinVerticesCPU()`가 정점별 최대 4개 Bone Matrix와 Weight를 합산해 Position, Normal, Tangent를 변환합니다. 결과 정점은 매 갱신 시 Dynamic Vertex Buffer로 업로드합니다.

### GPU Skinning

GPU 경로에서는 Bind Pose 원본 정점을 유지하고 Skinning Matrix 배열을 Structured Buffer SRV로 전달합니다. `FSkeletalVertexFactory`가 SRV를 Vertex Shader의 `t16`에 바인딩하며, `Skinning.hlsli`의 `ApplySkinning()`이 Position·Normal·Tangent를 변환합니다.

Opaque, Depth, Shadow, Selection 등 Skeletal Mesh를 사용하는 Pass에서도 Bone Index와 Weight를 포함한 Vertex Layout을 사용합니다.

```text
CPU: Bone Matrix + 원본 정점 → CPU 정점 변환 → Dynamic VB → Draw
GPU: Bone Matrix → Structured Buffer ─┐
     원본 정점 → Immutable VB ────────┴→ Vertex Shader Skinning → Draw
```

에디터 콘솔의 `skinning.cpu 0`은 GPU Skinning, `skinning.cpu 1`은 CPU Skinning을 활성화합니다. 기존 `USkinnedMeshComponent` 인스턴스와 이후 생성되는 인스턴스에 모드를 함께 적용합니다.

Animation과 Skeletal Mesh Stat은 Pose 평가, Pose Blend, CPU Skinning, 정점 수, Bone Matrix 수와 Draw Section을 기록합니다.

## 7. Bone Weight Heatmap

Skeletal Mesh 또는 Animation Sequence Preview에서 Bone을 선택하면 해당 Bone이 각 정점에 주는 Weight를 Heatmap으로 확인할 수 있습니다.

Vertex Shader는 정점의 네 Bone Index 중 선택 Bone을 찾아 Weight를 전달하고, Pixel Shader는 Weight 크기를 색상으로 변환합니다. Skeleton Tree의 선택 상태와 Preview Viewport의 `SelectedBoneIndex`가 같은 Debug Constant Buffer를 공유합니다.

이를 통해 Weight가 누락된 정점, 영향 범위가 지나치게 넓은 Bone, 인접 Bone 사이의 급격한 Weight 변화를 시각적으로 확인할 수 있습니다.

## 8. MiniDump와 Crash Diagnostics

엔진 시작 시 `SetUnhandledExceptionFilter`로 Crash Handler를 설치합니다. 처리되지 않은 예외가 발생하면 현재 Thread의 Exception Context를 수집하고 `MiniDumpWriteDump`를 이용해 `.dmp` 파일을 생성합니다.

동시에 다음 내용을 담은 Text Crash Log를 기록합니다.

- Process·Thread ID
- Exception Code와 Address
- Crash Context Message
- 최대 64 Frame의 Call Stack
- PDB가 제공될 경우 Symbol, Source File과 Line

지원 콘솔 명령:

| 명령 | 동작 |
| --- | --- |
| `causecrash` | 의도적인 Crash를 발생시켜 Dump 생성 경로 검증 |
| `dumpcurrentthread` | 프로그램을 종료하지 않고 현재 Thread Snapshot 생성 |
| `listthreads` | 현재 Process가 소유한 Thread 목록 출력 |
| `dumpcapturedthread <id>` | 다른 Thread를 일시 중단해 Context와 Snapshot 생성 |

Crash와 Snapshot 결과는 `.dmp`와 `.txt` 파일로 저장됩니다. Dump를 정확히 분석하려면 실행 파일과 같은 빌드에서 생성된 PDB가 필요합니다.

## 9. Unreal 스타일 Property Reflection

`Scripts/GenerateReflection.py`는 빌드 전에 `JSEngine/Source`의 Header를 스캔하고 다음 선언을 수집합니다.

- `UCLASS`
- `USTRUCT`
- `UENUM`
- `UPROPERTY`
- `GENERATED_BODY`

Header별 `*.generated.h`와 전체 등록 코드인 `Reflection.generated.cpp`를 생성합니다. 생성된 코드는 `StaticClass()`, `GetClass()`, `StaticStruct()`, Enum Metadata, Property Metadata와 Object Factory 등록을 제공합니다.

```text
Source Header
  → GenerateReflection.py
  → *.generated.h + Reflection.generated.cpp
  → UClass / UScriptStruct / UEnum / FProperty
  → Details / Serialize / Duplicate / Factory
```

`FProperty`는 실제 멤버 주소 대신 객체 시작점으로부터의 Offset을 저장합니다. 같은 Property Metadata를 여러 인스턴스에 재사용하며, Edit·Visible 여부와 Display Name, Serialize Name, Min·Max·Speed, Animatable 등의 Metadata를 Details와 직렬화 과정에 전달합니다.

## 빌드 및 실행

### 요구 환경

- Windows 10/11
- Visual Studio 2022
- C++ 데스크톱 개발 도구와 Windows SDK
- DirectX 11 지원 GPU
- NuGet Package Restore가 가능한 환경

프로젝트에는 FBX SDK, ACL, LuaJIT, SoLoud, RmlUi와 리플렉션 생성용 Python Runtime이 포함되어 있습니다. DirectXTK는 NuGet Package Restore로 준비합니다.

### 실행 방법

1. 루트의 `GenerateProjectFiles.bat`를 실행한다.
2. `JSEngine.sln`을 Visual Studio에서 연다.
3. NuGet Package Restore가 완료됐는지 확인한다.
4. `Debug | x64` 또는 `Release | x64` 구성으로 빌드한다.
5. `JSEngine/Bin/<Configuration>/JSEngine.exe`를 실행한다.

빌드 전 `Scripts/GenerateReflection.py`가 자동으로 실행되어 Reflection 코드를 갱신합니다.

### 기능 확인

1. Content Browser에서 Animation이 포함된 FBX를 Import한다.
2. 생성된 `.sequence` 애셋을 열어 Preview, Timeline, Curve와 Notify를 확인한다.
3. Skeleton Tree에서 Bone을 선택하고 Bone Weight 표시를 활성화한다.
4. 콘솔 명령으로 CPU와 GPU Skinning을 전환하고 Stat을 비교한다.

| 명령 | 용도 |
| --- | --- |
| `stat anim` | Animation Update·Evaluate·Blend Stat 표시 |
| `stat skeletalmesh` | CPU/GPU Skinning과 Skeletal Mesh Counter 표시 |
| `stat gpu` | GPU Pass Timing 표시 |
| `skinning.cpu 0` | GPU Skinning 사용 |
| `skinning.cpu 1` | CPU Skinning 사용 |
| `dumpcurrentthread` | 비파괴 방식으로 Dump와 Log 생성 확인 |
| `causecrash` | 실제 Crash Handler 검증. 실행 중인 프로그램이 종료되므로 주의 |

## 구현 범위

- FBX Anim Stack을 Bone Track과 Float Curve로 변환합니다.
- Animation Sequence를 애셋으로 저장하고 ACL 압축 데이터를 런타임에서 사용합니다.
- Sequence 재생, Reverse Play, Loop, Pose Sampling과 두 Sequence 간 Blending을 지원합니다.
- C++과 Lua에서 Animation State Machine을 구성할 수 있습니다.
- CPU와 GPU Skinning을 런타임에서 전환하고 관련 Stat을 표시합니다.
- Animation Sequence Viewer에서 Preview, Timeline, Curve 확인과 Notify 편집을 지원합니다.
- 선택 Bone 표시와 Bone Weight Heatmap을 제공합니다.
- MiniDump, Crash Log와 Thread Snapshot을 생성합니다.
- 코드 생성 기반 Property Reflection을 Details·Serialize·Duplicate·Factory 흐름에 사용합니다.

### 제한 사항

- **Bone Track 재샘플링**: Bone Animation은 FBX의 원본 Key와 Tangent를 그대로 보존하지 않고 Scene Frame Rate에 맞춰 프레임 단위로 Bake합니다. 원본의 비균일 Key 배치나 보간 특성이 단순화될 수 있습니다.
- **Bone 영향 수**: `FSkeletalMeshVertex`는 정점당 최대 4개의 Bone Index와 Weight만 저장합니다. 더 많은 영향은 Import 과정에서 모두 유지되지 않습니다.
- **Bone Index 범위**: Bone Index가 `uint8`이므로 표현 범위는 `0~255`이며, 256개를 초과하는 Bone을 직접 인덱싱할 수 없습니다.
- **Blending 범위**: 현재 Blending은 두 Local Pose 사이의 선형 Crossfade입니다. Additive Animation, Per-Bone Mask, Blend Space, Montage 계층은 포함하지 않습니다.
- **Skeleton 호환성**: 다른 Skeleton의 Sequence는 Bone 이름과 Parent 계층으로 Remap할 수 있을 때만 재생할 수 있습니다. 구조가 다른 Skeleton을 위한 Retargeting 시스템은 아닙니다.
- **GPU Skinning Stat**: Skeletal Mesh Stat의 GPU Skinning 시간은 순수 Vertex Skinning 명령만 분리한 값이 아니라 `Skeletal Pass` 전체 GPU 시간입니다.
- **GPU 경로 Bounds**: CPU Skinning은 변형된 정점으로 Bounds를 계산할 수 있지만, GPU Skinning 경로는 Bone 기반 Bounds를 사용하므로 실제 정점 경계보다 보수적일 수 있습니다.
- **Heatmap 범위**: Heatmap은 선택된 Bone 하나와 정점에 보존된 최대 4개 Weight만 시각화합니다.
- **Curve 편집 범위**: Animation Sequence Viewer의 Curve 기능은 Import된 Curve의 확인·선택·필터링 중심입니다. 범용 Curve Key·Tangent 편집기는 아닙니다.
- **Reflection Parser 제약**: Reflection Generator는 정규식 기반이며 `UPROPERTY` 다음의 지원되는 한 줄 멤버 선언만 처리합니다. 지원 타입과 Metadata도 Generator에 등록된 범위로 제한됩니다.
- **`UFUNCTION` 미지원**: 현재 Reflection은 Class, Struct, Enum과 Property Metadata를 대상으로 하며 Function Reflection은 구현하지 않았습니다.
- **Crash Dump 플랫폼 제약**: Crash Handler와 Call Stack 수집은 Windows와 DbgHelp에 종속됩니다. 정확한 Symbol 분석에는 실행 파일과 일치하는 PDB가 필요합니다.
