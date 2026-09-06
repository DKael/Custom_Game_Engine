# KraftonEngine — Week 13 PhysX Physics, Ragdoll, Vehicle, Cloth & Depth of Field

> **PhysX · Rigid Body · 6-DOF · Physics Asset · Ragdoll · Drive4W Vehicle · NvCloth · Depth of Field**

Windows 환경에서 동작하는 Win32 + DirectX 11 기반 자체 엔진에 PhysX 물리 런타임과 물리 애셋 제작 도구를 통합한 프로젝트입니다.

고정 시간 스텝으로 Rigid Body를 시뮬레이션하고, Static Mesh와 Skeletal Mesh에 물리 Body를 연결했습니다. Skeletal Mesh는 `UPhysicsAsset`에 Bone별 Body와 Constraint를 저장하고, 전용 Physics Asset Editor에서 이를 생성·편집할 수 있습니다. 이 데이터를 기반으로 전체·부분 Ragdoll과 복구 흐름을 구성했습니다.

같은 물리 기반 위에 PhysX Drive4W 차량과 NvCloth 의상 시뮬레이션을 구현했으며, 렌더링 파이프라인에는 Signed Circle of Confusion을 사용하는 Depth of Field와 Bokeh 합성 단계를 추가했습니다.

이 문서는 13주차 과제의 학습 목표와 현재 코드에서 확인되는 구현 범위를 기준으로 작성했습니다.

## 학습 목표

- Rigid Body의 위치·회전 자유도와 6축 잠금 방식을 이해한다.
- 고정 시간 스텝과 Substep을 사용하는 물리 시뮬레이션 루프를 구현한다.
- PhysX SDK를 자체 엔진의 Component·World·Collision 구조와 통합한다.
- Static Mesh와 Skeletal Mesh에 Static·Dynamic·Kinematic Body를 구성한다.
- Bone별 Body와 Constraint를 저장하는 `UPhysicsAsset`을 구현한다.
- Physics Asset Editor에서 Body·Shape·Constraint를 제작하고 Preview Simulation을 제공한다.
- Animation Pose와 Physics Pose를 결합해 전체·부분 Ragdoll을 구현한다.
- PhysX Vehicle SDK를 이용해 Suspension과 Tire 접촉을 처리하는 차량을 구현한다.
- NvCloth를 이용한 Skeletal Mesh Section 단위 Cloth Simulation을 구현한다.
- 초점 거리와 Circle of Confusion의 관계를 이해하고 Depth of Field를 구현한다.
- Show Flag, Debug Draw와 Stat으로 물리·Cloth·DoF 상태를 관찰한다.

## 구현 요약

| 영역 | 구현 내용 |
| --- | --- |
| PhysX Integration | 저장소에 포함된 PhysX Header·Debug/Release Library·DLL을 프로젝트 생성과 빌드 과정에 연결 |
| Physics Runtime | 전용 Physics Thread, Command Queue, 고정 시간 스텝, Substep, GT↔PT Transform 동기화와 Snapshot 제공 |
| Rigid Body | Static·Dynamic·Kinematic Body, Box·Sphere·Capsule Shape, 질량·중심·감쇠·CCD·6축 잠금 지원 |
| Collision | Channel·Response Filter, Hit·Overlap Event, Raycast·Sweep·Overlap Query 지원 |
| Physics Asset | Bone별 Body와 복수 Shape, Parent–Child Constraint, 선형·Twist·Swing 제한과 Binary Asset 직렬화 |
| Physics Asset Editor | Skeleton Tree, Body·Constraint Graph, Details, Gizmo, Viewport Picking, 자동 Body 생성과 Preview Simulation |
| Ragdoll | 전체·부분 Ragdoll, Animation–Physics Pose Blending, Hit·Impulse·Shockwave 반응과 기상 복구 |
| Vehicle | PhysX `PxVehicleDrive4W`, Suspension Raycast, Tire Friction, Ackermann Steering, 자동 기어와 Wheel Pose 반영 |
| Cloth | NvCloth CPU Solver, Section Binding, Max Distance Painting, 중력·바람과 Physics Asset·World Collision |
| Depth of Field | Signed CoC, 전경·배경 Blur, Highlight Bokeh와 Composite, DoF Show Flag·CoC Debug View |
| Diagnostics | Physics Body·Constraint Debug Draw, `stat physics`, `stat clothcollision`, 차량 Wheel Debug |

## 전체 물리 흐름

```text
Game Thread
  ├─ Component Transform / Physics Property 수집
  ├─ Create·Destroy·Force·Impulse 명령 생성
  └─ Physics Command Queue 제출
            │
            ▼
Physics Thread
  ├─ Engine → Physics Transform 동기화
  ├─ Fixed Step Accumulator
  ├─ PhysX Vehicle Pre-Simulate
  ├─ PxScene::simulate / fetchResults
  ├─ Physics → Engine Transform 수집
  └─ Body·Vehicle·Event Snapshot 생성
            │
            ▼
Game Thread
  ├─ Component Transform 반영
  ├─ Hit / Overlap Event Dispatch
  ├─ Skeletal Physics Pose Pull
  └─ Ragdoll Animation Pose Blending
```

물리 Backend의 Handle과 PhysX 객체는 Physics Thread가 소유합니다. Game Thread는 직접 PhysX 객체를 수정하지 않고 Command를 제출하며, 완료된 World Snapshot을 읽어 Component와 Skeletal Pose에 결과를 반영합니다.

## 1. PhysX SDK 통합과 물리 런타임

### SDK 연결

`Scripts/GenerateProjectFiles.py`는 PhysX와 NvCloth를 NuGet이나 사용자 전역 vcpkg 설정에 의존하지 않고 프로젝트 내부의 ThirdParty 경로에서 찾도록 구성합니다.

PhysX는 다음 모듈을 링크합니다.

- PhysX Foundation, Common과 Core Runtime
- Extensions와 Task System
- Cooking과 Character Kinematic
- PhysX Vehicle
- Scene Query, Low Level Dynamics와 Simulation Controller

Debug와 Release Library 경로를 분리하며, Build 후 해당 구성의 PhysX·NvCloth DLL을 실행 파일 출력 폴더로 복사합니다. PhysX SDK 초기화 시 Foundation, Physics, Extensions와 Vehicle SDK를 준비하고, World마다 `PxScene`, CPU Dispatcher와 Simulation Event Callback을 생성합니다.

### 고정 시간 스텝과 Substep

Physics Runtime은 Render Frame의 가변 `DeltaTime`을 직접 `PxScene::simulate()`에 전달하지 않습니다. Accumulator에 시간을 누적한 후 고정된 Physics Step만큼 시뮬레이션합니다.

```text
Frame DeltaTime
  → MaxFrameDeltaTime으로 제한
  → Accumulator에 누적
  → FixedTimeStep 단위로 소비
  → MaxSimulationSubstepDeltaTime 이하로 분할
  → PxScene::simulate(SubstepDt)
  → PxScene::fetchResults(true)
```

기본 설정은 60 Hz Fixed Step이며, Project Settings에서 다음 값을 조정할 수 있습니다.

- Fixed Physics Hz
- Simulation Substep Hz
- Max Frame Delta Time
- Max Substeps
- Async Physics 사용 여부

한 Frame에 처리 가능한 Substep 수를 넘으면 남은 시간을 버리고 `NumDroppedSubsteps`에 기록합니다. Accumulator 비율은 `InterpolationAlpha`로 Snapshot에 포함합니다.

### Physics Thread와 Command Queue

Physics Scene은 전용 Thread에서 시뮬레이션합니다. 동기 모드에서는 Game Thread가 제출한 Frame의 완료를 기다리고, Async Physics 모드에서는 진행 중인 물리 Frame이 있을 때 다음 `DeltaTime`과 Command를 Pending Frame에 합칩니다.

Command Queue는 다음 작업을 순서대로 전달합니다.

- Body·Shape·Constraint 생성과 제거
- Transform과 Velocity 갱신
- Mass와 Center of Mass 변경
- Force·Torque·Impulse 적용
- Linear·Angular Lock과 Gravity·CCD 상태 변경
- Vehicle 생성·입력·초기화

생성 결과에는 예약한 Handle과 성공 여부가 포함되며, Component 수명이 끝난 Body는 Deferred Destroy 방식으로 정리합니다.

## 2. Rigid Body, 6-DOF와 Collision

### Body와 동기화 모드

물리 Body는 세 가지 형태로 구성합니다.

| Body | 용도 | Transform 주도권 |
| --- | --- | --- |
| Static | 움직이지 않는 World Geometry | Engine |
| Dynamic | 중력·충돌·힘에 반응하는 물체 | Physics |
| Kinematic | 코드로 이동하면서 Dynamic Body와 상호작용하는 물체 | Engine Target |

`EPhysicsSyncMode`는 Engine→Physics, Physics→Engine, Kinematic Target과 Manual 동기화를 구분합니다. Dynamic Body 결과는 Snapshot을 통해 Root Component에 반영하고, Engine이 소유하는 Body는 Frame 시작 시 현재 Component Transform을 PhysX에 전달합니다.

### Shape와 Compound Body

기본 충돌 Shape는 다음 세 종류입니다.

- Box
- Sphere
- Capsule

하나의 Actor에 속한 Primitive Component는 Root Body를 공유하고 각 Component의 Shape를 Compound 형태로 추가할 수 있습니다. Shape Component는 실제 Box·Sphere·Capsule 크기를 사용하며, 일반 Static Mesh나 Skeletal Mesh Component는 World Bounds에 맞춘 Box를 기본 충돌 형상으로 사용합니다.

Mass, Center of Mass, Linear·Angular Damping, 최대 각속도와 Solver Iteration은 Body에 저장합니다. Compound Body에서는 Root Component의 Mass와 Center of Mass가 Backend에 적용됩니다.

### Degree of Freedom

Dynamic Body는 이동 3축과 회전 3축을 독립적으로 잠글 수 있습니다.

```text
Linear  : X / Y / Z Lock
Angular : X / Y / Z Lock
```

Body 생성 시 잠금 상태를 PhysX Dynamic Lock Flag로 변환하고, 실행 중 `SetLinearLock()`과 `SetAngularLock()`으로 변경할 수 있습니다. Physics Asset의 각 Body에도 같은 잠금 설정을 저장합니다.

### Collision Filter와 Event

각 Shape는 Object Type과 채널별 Block·Overlap Mask를 PhysX Filter Data로 전달합니다. 양쪽 Component의 Response를 비교해 접촉, Trigger 또는 무시 여부를 결정합니다.

Simulation Event Callback은 다음 이벤트를 Snapshot으로 변환합니다.

- Hit Begin과 End
- Overlap Begin과 End
- Contact Point, Normal과 Impulse
- Actor·Component 식별자

Raycast와 Sphere·Capsule·Box Sweep, Object Type 기반 Query도 Physics Scene 인터페이스를 통해 실행합니다. Query와 Simulation은 PhysX Scene 접근이 겹치지 않도록 Physics Thread에서 직렬화합니다.

## 3. UPhysicsAsset과 Physics Asset Editor

### Physics Asset 데이터

`UPhysicsAsset`은 Skeletal Mesh의 Bone 구조와 물리 Body를 연결하는 Binary Asset입니다.

```text
UPhysicsAsset
  ├─ Skeleton Binding
  ├─ Body Setups[]
  │    ├─ Bone Name
  │    ├─ Body Local Frame
  │    ├─ Box / Sphere / Capsule Shapes[]
  │    ├─ Mass / Center of Mass / Damping
  │    ├─ Solver / CCD / Gravity
  │    └─ Linear / Angular Lock
  └─ Constraint Setups[]
       ├─ Parent Bone / Child Bone
       ├─ Parent / Child Local Frame
       ├─ Linear X·Y·Z Motion
       ├─ Twist / Swing1 / Swing2 Motion과 Limit
       └─ 연결 Body 간 Collision 비활성화 여부
```

Constraint의 각 축은 Locked, Limited 또는 Free 상태를 가지며 PhysX D6 Joint로 생성됩니다. Twist 최소·최대 각도와 두 Swing 각도, Projection 사용 여부를 Asset에 저장합니다.

Skeletal Mesh Component는 Component Override, Mesh Default, Skeleton Default 순서로 사용할 Physics Asset을 선택합니다.

### 자동 Body 생성

Physics Asset Editor는 Skeletal Mesh의 Bone Weight를 분석해 Body와 Parent–Child Constraint를 자동 생성합니다.

지원 방식:

- PCA Analysis: Bone이 영향을 주는 정점 분포의 주축을 계산해 Primitive 방향과 크기를 맞춤
- Bone Axis: Bone과 Child Bone 방향을 기준으로 Primitive 축을 결정

지원 Primitive:

- Capsule
- Box
- Sphere

생성 옵션에서는 최소 Bone 크기, 최소 Weight, 최소 정점 수, 작은 Bone 병합, Helper Bone 제외, 기존 Body 교체와 Constraint 자동 생성을 설정할 수 있습니다. PCA 분석에 사용할 정점이 부족한 경우 설정에 따라 Bone Axis 방식으로 대체합니다.

### Editor 구성

Physics Asset Editor는 Skeletal Mesh Editor 안에 통합되어 있습니다.

| 영역 | 기능 |
| --- | --- |
| Skeleton Tree | Bone 계층과 Body·Constraint 연결 상태 표시 |
| Body List | Bone별 Body와 Shape 선택·추가·삭제 |
| Constraint Graph | Body Node와 Constraint 연결 관계 시각화·선택 |
| Details | Shape Transform·크기, Body 물성, Constraint Motion·Limit 편집 |
| Preview Viewport | Body·Constraint·Body Skeleton 표시와 Viewport Picking |
| Gizmo | Body Frame, Shape와 Constraint Frame의 이동·회전·크기 편집 |
| Simulation | Start·Stop·Pause·Resume, 무중력, 선택 Bone 이하만 시뮬레이션 |

Viewport에서 Body나 Constraint를 선택하면 Tree, Graph와 Details 선택이 동기화됩니다. Preview Simulation 중에는 물리 Body를 마우스로 잡아 움직여 Constraint와 Ragdoll 반응을 확인할 수 있습니다.

자동 생성 또는 수동 편집 결과는 Skeletal Mesh Editor의 저장 경로를 통해 Physics Asset에 반영됩니다. 현재 편집 상태와 Body·Constraint 설정은 `Saves/PhysicsAsset_Debug.json`으로 내보낼 수도 있습니다.

## 4. Ragdoll Simulation

### Physics Pose 생성

Ragdoll을 시작하면 `FPhysicsAssetInstance`가 Physics Asset의 Body와 Constraint를 Runtime Handle로 생성합니다. 초기 Body Transform은 현재 Skeletal Animation Pose에서 계산하므로 Ragdoll 시작 시 Reference Pose로 튀지 않고 현재 자세에서 물리 시뮬레이션으로 전환됩니다.

Physics Snapshot에서 Bone별 Body Transform을 읽은 후 Component Space와 Local Space Pose로 재구성합니다.

```text
Animation Local Pose ───────────────┐
                                    ├─ Bone별 Blend Weight → Final Pose
Physics Body World Transform        │
  → Component Space                 │
  → Bone Local Physics Pose ────────┘
```

### 전체·부분 Ragdoll

전체 Ragdoll은 모든 유효 Physics Body를 Dynamic으로 만들고 Physics Pose의 Blend Weight를 증가시킵니다.

부분 Ragdoll은 선택한 Root Bone과 Descendant에만 Physics Body와 Blend Mask를 적용합니다. 기본 Preset은 다음과 같습니다.

- Upper Body
- Left Arm
- Right Arm
- Head·Neck

부분 Ragdoll은 Blending In, Active, Blending Out 단계를 가지며 Hold Time이 끝나면 Animation Pose로 복귀합니다. 같은 부위에 연속 요청이 들어오면 유지 시간을 갱신할 수 있습니다.

### Hit Reaction과 Impulse

Hit Bone에 직접 연결된 Body가 없으면 가장 가까운 Simulated Ancestor Body를 찾아 Impulse 대상으로 사용합니다. Hitscan과 Shockwave 요청은 위치, 방향, 세기와 반경을 이용해 반응을 계산합니다.

반응 정책은 다음 동작을 선택할 수 있습니다.

- 기존 부분 Ragdoll 갱신
- Hit Bone에 맞는 부분 Ragdoll 시작
- 강한 충격을 전체 Ragdoll로 승격
- 현재 Ragdoll Body에 Impulse만 추가
- 복구 중 다시 Ragdoll로 전환

### Ragdoll 복구

전체 Ragdoll 복구는 Physics Pose를 즉시 제거하지 않고 단계적으로 처리합니다.

```text
Ragdoll Pose Snapshot
  → Physics Blend Out
  → Pelvis / Chest 방향으로 Face Up·Face Down 판정
  → Front 또는 Back Stand-Up Animation 선택
  → Animation 재생
  → 기존 Animation Mode 복원
```

기상 Animation이 설정되지 않았거나 Skeleton과 호환되지 않으면 Stand-Up 재생 단계를 건너뛰고 안전하게 기존 Animation 상태로 복귀합니다.

## 5. PhysX Drive4W Vehicle

`UWheeledVehicleMovementComponent`는 Gameplay 입력과 Backend에 독립적인 `FVehicleDesc`를 관리합니다. 실제 PhysX 객체 생성과 시뮬레이션은 `FPhysXVehicleRuntime`이 담당하고, Wheel 시각화는 `UVehicleWheelPoseComponent`가 Snapshot을 이용해 처리합니다.

### 차량 구성

차량 설정에는 다음 값이 포함됩니다.

- Chassis Half Extent, Mass와 Center of Mass Offset
- Engine Peak Torque와 Max Omega
- Clutch Strength
- Front·Rear Wheel Drive 조합
- Wheel Radius, Width, Mass와 Tire Friction
- Suspension Compression·Droop, Spring Strength와 Damper Rate
- Steering·Brake·Handbrake 적용 여부와 최대 Torque
- Wheel이 Trace할 Collision Object Type

Wheel 위치는 Skeletal Mesh Bone, 별도 Scene Component 또는 Manual Position에서 가져올 수 있습니다. Skeleton이나 Static Mesh Component 배치에서 Wheel Setup을 자동 생성하고, 편집한 구성이 정확히 4개 Wheel인지 검사합니다.

### Drive4W 시뮬레이션

PhysX Runtime은 네 Wheel의 Sprung Mass를 계산하고 Wheel, Tire와 Suspension Data를 구성합니다. 구동축 설정에 따라 FWD, RWD 또는 4WD Differential을 선택하며 Wheel 배치로 Ackermann Geometry를 계산합니다.

한 Physics Step의 차량 처리는 다음 순서입니다.

```text
Throttle / Brake / Steering / Handbrake
  → Analog Input Smoothing
  → Forward / Reverse Gear 결정
  → Wheel Suspension Raycast
  → Tire–Surface Friction 적용
  → PxVehicleUpdates
  → Chassis / Wheel Snapshot 생성
```

차량이 낮은 속도에서 반대 방향 Throttle을 받으면 정지 상태를 확인한 뒤 1단과 후진 Gear를 전환합니다. Wheel Query Result에는 Steering Angle, Rotation Angle, Suspension Jounce, 공중 여부와 접촉점·법선을 기록합니다.

### Wheel Pose

Wheel Snapshot은 물리 Wheel과 Render Mesh를 직접 결합하지 않습니다. `UVehicleWheelPoseComponent`가 Wheel 이름과 Bone 또는 Visual Component를 찾아 Suspension 이동, Steering과 회전 Pose를 적용합니다. 이를 통해 차량 Physics와 Skeletal·Static Wheel 표현을 분리합니다.

기본 `AWheeledVehiclePawn` 입력:

| 입력 | 동작 |
| --- | --- |
| `W` / `S` | 전진·후진 Throttle |
| `A` / `D` | Steering |
| `Space` | Handbrake |

## 6. NvCloth Simulation

### Cloth Asset 데이터

Cloth는 Skeletal Mesh의 LOD와 Section에 연결합니다. 선택 Section의 Index와 정점을 Cloth Particle로 변환하고 Render Vertex와 Particle의 대응 관계를 저장합니다.

```text
Skeletal Mesh LOD / Section
  → Cloth Particle Position / Triangle Index
  → Particle ↔ Render Vertex Mapping
  → Max Distance Paint
  → NvCloth Fabric / Cloth Instance
```

`MaxDistanceValues`는 Animation으로 Skinning된 기준 위치에서 각 Particle이 이동할 수 있는 최대 거리를 의미합니다. 값이 0인 Particle은 기준 위치에 고정되고, 값이 클수록 Cloth Solver가 더 넓게 움직일 수 있습니다.

### Cloth Editor

Skeletal Mesh Editor에서 다음 Cloth 제작 기능을 제공합니다.

- LOD와 Section을 선택해 Cloth Data 생성·제거
- Cloth 활성화와 Preview Simulation
- Gravity Scale, Solver Frequency, Stiffness와 Damping 편집
- Wind Scale, Drag·Lift Coefficient와 Fluid Density 편집
- Linear·Angular Inertia Scale 편집
- Max Distance Fill, 고정·해제, Brush Painting과 Smooth
- Paint Value Overlay와 Brush Radius 표시
- Preview Wind와 Simulation Reset
- Physics Asset·World Static·World Dynamic Collision 선택
- Collision Candidate 선택·제외·Budget 결과 확인

Cloth가 활성화된 Preview는 CPU Skinning 경로에서 동작하며, Solver 결과 Position을 Skinned Vertex에 다시 기록합니다.

### Solver와 Force

NvCloth CPU Factory에서 Solver를 생성하고 Section별 Fabric과 Cloth Instance를 등록합니다. Simulation은 고정 Cloth Step과 최대 Substep 수를 사용합니다.

각 Tick에서 다음 값을 갱신합니다.

- Local Gravity
- Wind Velocity
- Motion Constraint와 Max Distance
- Stiffness, Damping과 Solver Frequency
- Drag, Lift와 Fluid Density
- Component Linear·Angular Inertia

Component가 설정된 거리·회전 임계값 이상 Teleport하거나 Scale이 변경되면 누적된 Cloth 상태를 Reset해 이전 위치로 길게 늘어나는 현상을 방지합니다.

### Cloth Collision

Cloth Collision Gatherer는 Section Bounds와 설정을 이용해 다음 Source에서 Collision 후보를 수집합니다.

- 연결된 Physics Asset의 Body Shape
- World Static Body
- World Dynamic Body

후보는 Section Bounds와 개수 Budget으로 필터링합니다. 선택된 Sphere·Capsule과 Box에서 변환한 Plane은 NvCloth Collision Data로 전달하며, 선택·제외·Budget 초과 상태를 `stat clothcollision`과 Editor Candidate 목록에서 확인할 수 있습니다.

## 7. Depth of Field와 Bokeh

### Signed Circle of Confusion

Depth Buffer에서 World Position을 복원하고 Camera와의 거리를 계산한 뒤 Signed CoC를 생성합니다.

```text
Signed CoC = clamp(
    (ViewDistance - FocusDistance) / max(FocusRange, 1),
    -1,
    1)
```

- 음수 CoC: 초점보다 가까운 Foreground
- 0 부근: Focus 영역
- 양수 CoC: 초점보다 먼 Background

CoC는 Full Resolution `R16_FLOAT` Render Target에 저장합니다. 별도의 `DoF CoC` View Mode에서는 Foreground와 Background 값을 색으로 표시해 Focus Distance와 Range를 확인할 수 있습니다.

### 렌더링 패스

DoF는 다음 Fullscreen Pass로 구성합니다.

```text
Scene Color + Depth
  → DoF Setup             : Signed CoC 생성
  → Background Blur       : 양수 CoC와 Depth를 이용한 원거리 Blur
  → Foreground Blur       : 음수 CoC를 주변으로 확장해 근거리 경계 처리
  → Bokeh Scatter         : 큰 CoC와 높은 휘도의 Highlight 추출
  → DoF Composite         : Sharp / Background / Foreground / Bokeh 합성
```

Background와 Foreground Blur는 Pixel별 회전이 다른 48개 Spiral Disk Sample을 사용합니다. Background는 현재 Pixel보다 가까운 Sample의 Weight를 낮춰 전경색이 원거리 Blur로 번지는 현상을 줄입니다. Foreground는 주변의 근거리 CoC Coverage를 모아 물체 가장자리까지 Blur Mask를 확장합니다.

Bokeh Pass는 Blur 반경과 휘도 Threshold를 모두 넘는 Pixel을 선택해 별도 HDR Layer에 기록합니다. 최종 Composite는 Focus 영역을 보호하면서 전경·배경 Blur와 Bokeh Intensity를 합성합니다.

### 제어 경로

Viewport Toolbar에서 다음 값을 조정할 수 있습니다.

- DoF Show Flag
- Focus Distance
- Focus Range
- Max Blur Radius
- Bokeh Radius Threshold
- Bokeh Luma Threshold
- Bokeh Intensity

Gameplay에서는 `APlayerCameraManager`의 `SetDepthOfField()`, `SetBokeh()`와 `ClearDepthOfField()`를 사용합니다. 같은 기능을 Lua Blueprint Camera Node에서도 호출할 수 있습니다.

## 8. Show Flag, Debug Draw와 Stat

### Show Flag와 Debug View

| 항목 | 용도 |
| --- | --- |
| Show Collision Shape | PIE·Game View에서 Physics Shape Wireframe 표시 |
| Physics Asset Shapes | Physics Asset의 Body Shape 표시 |
| Physics Asset Constraints | Parent–Child Constraint와 Frame 표시 |
| Physics Asset Body Skeleton | Physics Body 사이의 Skeleton 연결 표시 |
| Constraint Limit Angle·Surface | Twist·Swing 제한 범위 확인 |
| Vehicle Wheel Debug | 선택 Wheel의 위치, Suspension과 접촉 정보 확인 |
| DoF | Depth of Field 전체 Pass 활성화 |
| DoF CoC View | Foreground·Focus·Background CoC 시각화 |

### Physics Stat

Console에서 `stat physics`를 실행하면 다음 정보를 확인할 수 있습니다.

- 전체·Static·Dynamic·Kinematic Body 수
- Shape·Constraint 수
- Active·Sleeping Body 수
- Contact·Trigger Pair 수
- Raycast·Sweep·Overlap Query 수
- Physics Command와 Deferred Destroy 수
- Substep과 Dropped Substep 수
- Accumulator와 Interpolation Alpha
- Command 적용, 동기화, Simulate, Fetch, Snapshot과 Event Dispatch 시간
- Vehicle·Wheel 수, 공중 Wheel 수와 Raycast·Update 시간

`stat clothcollision`은 Cloth Tick, 활성 Component·Section, Collision 후보와 선택·제외 원인, Source·Primitive별 통계를 표시합니다.

## 빌드 및 실행

### 요구 환경

- Windows 10/11
- Visual Studio 2022와 MSVC v143 Toolset
- C++ 데스크톱 개발 도구와 Windows SDK
- DirectX 11 지원 GPU
- NuGet Package Restore가 가능한 환경

프로젝트는 C++20을 사용합니다. PhysX와 NvCloth의 Header, Library와 DLL은 `KraftonEngine/ThirdParty`에 포함되어 있으며, DirectXTK는 `packages.config`를 통해 복원합니다.

### 실행 방법

1. 프로젝트 루트의 `GenerateProjectFiles.bat`를 실행한다.
2. 생성된 `KraftonEngine.sln`을 Visual Studio에서 연다.
3. DirectXTK NuGet Package Restore가 완료됐는지 확인한다.
4. `Debug | x64` 또는 `Release | x64` 구성으로 빌드한다.
5. `KraftonEngine/Bin/<Configuration>/KraftonEngine.exe`를 실행한다.

Build 전 `Scripts/GenerateHeaders.py`가 Reflection Header를 갱신합니다. Build 후 현재 구성에 맞는 PhysX, NvCloth, RmlUi, FMOD, Lua와 FBX DLL을 실행 폴더로 복사합니다.

### 기능 확인

1. Skeletal Mesh Asset을 열고 Physics Asset을 생성하거나 기존 Asset을 지정한다.
2. Physics Asset Editor에서 Body를 자동 생성하고 Shape·Constraint를 편집한다.
3. Preview Simulation에서 Pause, 무중력, 선택 Body Simulation과 Drag를 확인한다.
4. Ragdoll Test Component 또는 Gameplay 요청으로 전체·부분 Ragdoll과 복구를 확인한다.
5. Vehicle Pawn을 배치하고 Wheel Setup Validation 후 `WASD`와 `Space`로 주행한다.
6. Skeletal Mesh Section에 Cloth를 생성하고 Max Distance를 Painting한다.
7. Preview Wind와 Physics Asset·World Collision을 활성화해 Cloth 반응을 확인한다.
8. Viewport에서 DoF를 활성화하고 Focus Distance·Range·Blur·Bokeh 값을 조정한다.
9. DoF CoC View, Physics Show Flag, `stat physics`와 `stat clothcollision`을 확인한다.

## 구현 범위

- PhysX를 독립적인 Physics Scene·Runtime 인터페이스 뒤에 통합했습니다.
- 전용 Physics Thread, Command Queue, Fixed Step·Substep과 World Snapshot을 제공합니다.
- Static·Dynamic·Kinematic Body와 Box·Sphere·Capsule Compound Shape를 지원합니다.
- Collision Channel·Response, Hit·Overlap Event와 Raycast·Sweep·Overlap Query를 제공합니다.
- `UPhysicsAsset`에 Bone별 Body·Shape·Constraint를 저장합니다.
- Physics Asset Editor에서 자동 Body 생성, 수동 편집, Graph와 Preview Simulation을 지원합니다.
- Animation Pose와 Physics Pose를 결합한 전체·부분 Ragdoll과 복구를 제공합니다.
- PhysX Drive4W 기반 4륜 차량과 Wheel Snapshot 기반 시각 동기화를 제공합니다.
- NvCloth CPU Solver와 Section 단위 Cloth 제작·Painting·Collision을 제공합니다.
- Signed CoC 기반 전경·배경 Blur, Bokeh와 Composite를 제공합니다.
- Physics·Cloth Stat과 Collision·Constraint·CoC Debug View를 제공합니다.

### 제한 사항

- **PhysX SDK 빌드 재현 범위**: 저장소에는 직접 빌드한 PhysX Header·Library·DLL이 포함되어 있지만 PhysX 원본 Source와 SDK 자체를 다시 빌드하는 Script는 포함되어 있지 않습니다. 문서의 빌드 절차는 준비된 SDK 산출물을 엔진에 링크하는 과정입니다.
- **일반 Mesh Collision 정확도**: Shape Component는 실제 Primitive를 사용하지만 일반 Static Mesh와 Skeletal Mesh Component의 Component-level Body는 World Bounds 기반 Box를 사용합니다. Triangle Mesh·Convex Hull 기반 정밀 충돌은 제공하지 않습니다.
- **Physics Asset Shape 범위**: Physics Asset은 Box, Sphere와 Capsule만 지원합니다. Convex, Tapered Capsule과 임의 Mesh Shape는 포함하지 않습니다.
- **Compound Body 물성**: 같은 Actor의 자식 Primitive는 Root Body에 Shape로 결합되며 Mass와 Center of Mass는 Root Component 값만 적용됩니다.
- **Physics 시간 손실**: Frame 지연으로 필요한 Step이 `MaxSubsteps`를 넘으면 Accumulator의 남은 시간을 버립니다. 안정성을 위한 제한이지만 매우 낮은 Frame Rate에서는 실제 시간보다 Simulation이 느려질 수 있습니다.
- **Physics Asset Validation UI**: Body·Constraint Validation 코드는 구현되어 있지만 현재 Physics Asset Editor의 Validation Panel과 실행 버튼은 비활성화되어 있습니다.
- **Ragdoll Asset 의존성**: Ragdoll은 Skeleton과 Bone 이름이 호환되는 Physics Asset이 필요합니다. 자동 생성 결과도 Bone Weight와 정점 수 임계값에 영향을 받으므로 모든 Mesh에 동일한 품질을 보장하지 않습니다.
- **Ragdoll 복구**: 자연스러운 기상 전환을 위해서는 해당 Skeleton과 호환되는 Face-Up·Face-Down Animation을 별도로 지정해야 합니다.
- **Ragdoll Stat 범위**: 전용 `stat ragdoll`은 제공하지 않습니다. Ragdoll Body와 Constraint는 일반 `stat physics` 항목에 포함됩니다.
- **Vehicle Wheel 수**: PhysX Backend는 `PxVehicleDrive4W`를 사용하므로 정확히 네 개의 Wheel Setup만 시뮬레이션합니다. 2륜, 6륜, Tank와 Trailer 구조는 지원하지 않습니다.
- **Vehicle 접지 방식**: Wheel은 Geometry Shape 충돌이 아니라 Suspension Raycast로 지면을 판정합니다. 복잡한 Wheel–Obstacle 접촉을 실제 회전 Collider로 해결하는 구조는 아닙니다.
- **Cloth Backend**: NvCloth CPU Factory만 사용하며 DX11·CUDA GPU Solver와 GPU-resident 결과 경로는 구현하지 않았습니다.
- **Cloth Skinning 비용**: Cloth 결과 Position을 CPU Skinned Vertex에 반영하므로 Cloth Section의 정점 수에 비례한 CPU 갱신과 Vertex Buffer Upload 비용이 발생합니다.
- **Cloth Fabric Cooking**: Asset에 `CookedFabricData` 필드가 있지만 Runtime 초기화에서는 Fabric을 다시 Cook합니다. Cooked 결과를 재사용하는 Pipeline은 후속 과제입니다.
- **Cloth Step 제한**: Cloth Simulation도 Frame당 최대 Substep 수를 제한합니다. 큰 `DeltaTime`이 지속되면 누적 시간을 잘라내므로 정밀도보다 안정성을 우선합니다.
- **DoF 광학 모델**: 현재 CoC는 Focus Distance와 Focus Range로 정규화한 화면 효과입니다. 실제 Camera의 F-Stop, Focal Length와 Sensor Size를 사용하는 Thin-Lens 광학 모델은 아닙니다.
- **DoF 비용**: 전경·배경 Blur는 Full Resolution에서 각각 48개 Sample을 사용하며, Bokeh 탐색 비용은 Max Blur Radius가 커질수록 증가합니다.
