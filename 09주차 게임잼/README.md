# NipsEngine — Week 9 Drift Salvage Game Jam & Lua Scripting

> **DirectX 11 · PUC Lua 5.4.8 · sol2 · Script Component · Delegate · Collision Event · Drift Salvage**

크래프톤 정글 GameTechLab 자체 게임엔진 제작 교육의 9주차 게임잼 결과물입니다. 기존 엔진의 Actor·Component·Collision·UI 구조에 Lua 스크립팅 계층을 연결하고, 보트를 조종해 해상 수집물을 회수하는 **Drift Salvage**를 구현했습니다.

Lua는 보트 입력과 이동 파라미터, 타이틀·HUD·게임 오버·기록 UI를 담당합니다. C++은 Script 생명주기, 엔진 API 바인딩, 충돌 판정, 수집·폭발 시스템과 게임 상태를 제공합니다.

## 학습 목표

- 기존 Component를 조합해 시작·종료·재시작·점수 흐름을 갖춘 게임을 구현한다.
- Lua 런타임을 엔진에 통합하고 Actor·Component·입력·UI를 Script에서 제어한다.
- Component의 Hit·Overlap Event를 게임 로직과 Lua Callback에 연결한다.
- Delegate와 Variadic Template을 이용해 재사용 가능한 이벤트 전달 구조를 구현한다.
- Box·Sphere·Capsule Collision Component를 구현한다.
- 게임 규칙을 C++ 엔진 시스템과 Lua 게임 스크립트로 분리해 구성한다.

과제 명세에는 LuaJIT, Coroutine 스케줄러, 템플릿 기반 Script 생성과 자동 Hot Reload도 포함되어 있습니다. 현재 코드에서 확인되는 구현은 **PUC Lua 5.4.8 + sol2**, 수동 Reload, Lua 표준 Coroutine 라이브러리 제공까지이며, 차이는 문서 하단의 [구현 범위와 제한 사항](#구현-범위와-제한-사항)에 정리했습니다.

## 구현 요약

| 영역 | 구현 내용 |
| --- | --- |
| Lua Runtime | Lua 5.4.8 소스를 프로젝트에 직접 포함해 빌드하고 sol2로 C++ API 바인딩 |
| Script Instance | 하나의 공유 Lua VM과 Component별 독립 `sol::environment` |
| Script Lifecycle | Start·Update·Enable·Disable·Destroy·Overlap·Hit Callback 전달 |
| Input Script | Pawn별 Controller Script, 키·마우스 입력 전달, 보트 이동·궤도 카메라 제어 |
| Editor 연동 | Script Path 선택, Script 목록 갱신, 선택 Component 수동 Reload |
| Delegate | Singlecast·Multicast, Variadic Argument, Handle 제거, Weak UObject 바인딩 |
| Collision | Box·Sphere·Capsule, 공간 인덱스 Broad Phase, Shape별 Narrow Phase, Begin·End·Hit Event |
| 게임플레이 | 범위 수집, 적재 중량·점수·체력, Rock Knockback, Hazard 연쇄 폭발, Lighthouse 도착 |
| UI와 상태 | Lua 타이틀·HUD·미니맵·게임 오버·재시작, Top 3 점수 파일 저장 |

## 전체 실행 구조

```text
UGameEngine
  ├─ Scene_Game.Scene 로드
  ├─ FLuaScriptSubsystem
  │   ├─ shared sol::state
  │   ├─ LuaBinder: Engine API 등록
  │   └─ UScriptComponent
  │       └─ FLuaScriptInstance + isolated environment
  │
  ├─ FGameInputController
  │   └─ PlayerController.lua
  │       ├─ W/S/A/D 입력
  │       ├─ Pawn::UpdateBoatMovement(...)
  │       └─ Mouse Orbit Camera
  │
  ├─ UWorld
  │   ├─ FCollisionSystem
  │   ├─ FCollectionSystem
  │   └─ FExplosionSystem
  │
  └─ LogoHud Actor + UScriptComponent
      └─ LogoHud.lua
          ├─ Title / Record
          ├─ HUD / Minimap
          └─ Game Over / Restart
```

## 게임 개요: Drift Salvage

플레이어는 보트를 조종해 바다의 수집물을 회수하고 점수를 얻습니다. 수집물의 무게가 늘수록 보트의 유효 질량이 커져 가속과 조향이 둔해집니다. Rock과 Hazard를 피하면서 체력을 유지하고, Lighthouse에 도달해 현재 점수를 기록하는 것이 목표입니다.

### 조작

| 입력 | 동작 |
| --- | --- |
| `W` / `S` | 전진·후진 및 제동 |
| `A` / `D` | 좌·우 조향 |
| 마우스 이동 | 보트를 중심으로 카메라 회전 |
| `Space` 누르기 | 보트 주변 회수 범위 확장 |
| `Space` 놓기 | 범위 안의 수집물을 보트로 회수 |

### 핵심 플레이 흐름

```text
Title
  → START
  → Stats Reset + Gameplay Input 활성화
  → 이동 / 회수 / 충돌
      ├─ 수집물 → Weight·Money 증가
      ├─ Rock → Health -1 + Knockback
      ├─ Hazard → Health -2 + Explosion
      └─ Lighthouse 또는 Health 0 → Game Over
  → 현재 Money를 Score로 기록
  → RESTART 또는 TITLE
```

## 1. Lua 5.4 Runtime 통합

`ThirdParty/Lua`의 Lua 5.4.8 C 소스를 `NipsEngine.vcxproj`에서 직접 컴파일하며, `ThirdParty/sol2`를 C++ 바인딩 계층으로 사용합니다. 별도 Lua DLL에 의존하지 않고 엔진 실행 파일 안에 VM을 포함하는 구조입니다.

`FLuaScriptSubsystem`은 엔진 전역에서 하나의 `sol::state`를 관리하고 다음 표준 라이브러리를 엽니다.

- `base`
- `package`
- `math`
- `table`
- `string`
- `coroutine`

Script 파일은 절대 경로로 정규화한 뒤 바이너리 모드로 읽고, UTF-8 BOM을 제거한 다음 `sol::load`와 `sol::protected_function`으로 실행합니다. Load·Runtime·Callback 오류는 구분해 로그로 남깁니다.

관련 코드:

- `NipsEngine/Source/Engine/Scripting/LuaScriptSubsystem.h`
- `NipsEngine/Source/Engine/Scripting/LuaScriptSubsystem.cpp`
- `NipsEngine/Source/Engine/Scripting/LuaScriptInstance.h`
- `NipsEngine/ThirdParty/Lua`
- `NipsEngine/ThirdParty/sol2`

## 2. Script Component와 Instance 격리

`UScriptComponent` 하나는 `FLuaScriptInstance` 하나를 소유합니다. 모든 Instance가 Lua VM과 Engine Binding을 공유하지만 각자 별도의 `sol::environment`를 사용하므로, 같은 Lua 파일을 여러 Actor에 연결해도 일반 Script 전역 변수는 Component 단위로 분리됩니다.

```text
FLuaScriptSubsystem
  └─ shared sol::state / Engine API
      ├─ Actor A / ScriptComponent
      │   └─ Environment A
      └─ Actor B / ScriptComponent
          └─ Environment B
```

각 Environment에는 다음 실행 Context가 제공됩니다.

| 이름 | 내용 |
| --- | --- |
| `Self`, `Owner` | Script Component를 소유한 Actor |
| `Component` | 현재 `UScriptComponent` |
| `DestroySelf()` | Owner Actor 파괴 요청 |

### Script Lifecycle

`UScriptComponent`는 Callback 존재 여부를 먼저 확인하고, 아래 순서로 첫 번째로 발견한 함수를 호출합니다. Callback은 선택 사항이므로 구현하지 않아도 오류로 처리하지 않습니다.

| 엔진 시점 | Lua Callback | 전달 인자 |
| --- | --- | --- |
| Play 시작 | `OnStart` 또는 `BeginPlay` | `self` |
| 매 프레임 | `OnUpdate` 또는 `Tick` | `self, deltaTime` |
| Component 활성화 | `OnEnable` | `self` |
| Component 비활성화 | `OnDisable` | `self` |
| Play 종료·Reload 전 | `OnDestroy` | `self` |
| Overlap 시작 | `OnOverlapBegin` 또는 `OnOverlap` | `self, otherActor` |
| Overlap 종료 | `OnOverlapEnd` | `self, otherActor` |
| Blocking Hit | `OnHit` | `self, otherActor, hitInfo` |

Load 실패는 해당 Component를 즉시 비활성화합니다. 실행 중 Callback 오류는 연속 횟수를 기록하고 세 번 연속 실패하면 그 Script Component의 Tick과 Event 전달만 중단해 다른 Actor의 Script 실행을 유지합니다.

### Reload와 Editor 연동

Property 패널에서 `Script Path`와 `Script Enabled`를 편집할 수 있습니다.

- `Asset/Scripts`를 재귀 탐색해 `.lua` 목록 구성
- `Refresh Script List`로 목록 캐시 갱신
- `Reload Script`로 선택한 Component 수동 Reload
- Play 중 Script Path 변경 시 해당 Instance Reload
- Reload 전 `OnDisable`·`OnDestroy` 전달
- 기존 Environment를 폐기하고 새 Environment에서 Script 재실행
- Reload 후 활성 상태라면 `OnStart`·`OnEnable` 재호출

이 기능은 저장 시점을 감시하는 자동 Hot Reload가 아니라 명시적 Reload입니다.

관련 코드:

- `NipsEngine/Source/Engine/Component/Script/ScriptComponent.h`
- `NipsEngine/Source/Engine/Component/Script/ScriptComponent.cpp`
- `NipsEngine/Source/Editor/UI/EditorPropertyWidget.cpp`

## 3. 입력 전용 Lua Controller

Actor에 부착하는 `UScriptComponent` 외에, `FGameInputController`가 Pawn의 Controller Script를 실행하는 별도 경로가 있습니다. 두 경로는 같은 Lua VM과 Engine Binding을 공유하지만 서로 다른 Environment와 호출 규약을 사용합니다.

`FGameInputController`는 World에서 첫 번째 Pawn을 찾고 Pawn의 `ControllerScriptPath`를 읽습니다. 기본 Pawn은 `Asset/Scripts/PlayerController.lua`를 사용합니다.

| 입력 Controller Callback | 전달 인자 |
| --- | --- |
| `OnUpdate` | `deltaTime` |
| `OnKeyDown` | `keyName, keyCode` |
| `OnKeyUp` | `keyName, keyCode` |
| `OnMouseMove` | `deltaX, deltaY, mouseX, mouseY` |
| `OnMouseClick` | `buttonName, pressed, mouseX, mouseY` |

Environment에는 현재 Pawn이 `Pawn` 전역으로 주입됩니다. Pawn이 없을 때 사용하는 기본 Controller에는 Viewport Camera 조작용 `Camera` 테이블도 제공합니다.

`PlayerController.lua`는 적재 무게를 용량으로 정규화해 유효 질량을 계산하고, 입력값과 이동 파라미터를 `Pawn:UpdateBoatMovement(...)`에 전달합니다. 실제 속도 적분과 Actor Transform 갱신은 C++ Pawn이 수행합니다. 마우스 입력은 Camera Component의 상대 위치를 구면 좌표처럼 갱신한 뒤 보트를 바라보게 합니다.

Lua HUD가 `SetUIMode(true)`를 요청하면 `FGameInputController`는 Gameplay 입력을 중단하고 커서 잠금을 해제합니다. START를 누르면 UI Mode를 해제해 보트 입력을 다시 활성화합니다.

관련 코드:

- `NipsEngine/Source/Engine/Input/GameInputController.h`
- `NipsEngine/Source/Engine/Input/GameInputController.cpp`
- `NipsEngine/Source/Engine/GameFramework/Pawn.cpp`
- `NipsEngine/Asset/Scripts/PlayerController.lua`

## 4. Lua Engine API Binding

`LuaBinder`는 Script가 엔진 기능에 접근할 수 있도록 타입과 전역 함수를 등록합니다.

| 범주 | 주요 바인딩 |
| --- | --- |
| Math·Collision | `Vec3`, `HitInfo` |
| Actor | 유효성·이름·Tag, 위치·회전, 방향 벡터, Component 검색, Actor 파괴 |
| Scene Component | World·Relative Transform 조회 및 변경 |
| Pawn·Camera | 보트 이동, 속도 조회, Camera LookAt·회전 입력 |
| Input | `GetKeyDown`, `GetKey`, `GetKeyUp` |
| UI | Image·Text·Progress Bar 생성, 속성 변경, Hover·Click Callback |
| World Query | Tag 기반 단일·다중 Actor 검색 |
| Game State | 체력·Money·적재 중량 조회, Stats Reset, Game Over 소비, 재시작 요청 |
| File·Logging | 텍스트 읽기·쓰기, Log·Warning·Error, 시간·Frame 조회 |

텍스트 파일 API는 절대 경로와 `..`를 거부하고 엔진 Root 아래의 상대 경로만 허용합니다. `ScoreManager.lua`는 이 API로 `Saves/ScoreRecords.txt`를 관리합니다.

관련 코드:

- `NipsEngine/Source/Engine/Scripting/LuaBinder.h`
- `NipsEngine/Source/Engine/Scripting/LuaBinder.cpp`

## 5. Delegate와 Variadic Argument

`TDelegate<Ret(Args...)>`와 `TMulticastDelegate<void(Args...)>`를 함수 Signature 기반으로 특수화해 인자 개수와 타입을 일반화했습니다.

### Singlecast Delegate

- 하나의 `std::function` 바인딩
- `Bind`, `UnBind`, `IsBound`, `Execute`
- `BindUObject`에서 `TWeakObjectPtr` 사용
- `void` 반환형용 `ExecuteIfBound`

### Multicast Delegate

- 함수·람다를 여러 개 등록하고 `Broadcast`
- 등록 시 `FDelegateHandle` 발급
- Handle 단위 `Remove`와 전체 `RemoveAll`
- `AddUObject`로 소멸된 UObject Callback 건너뛰기
- Broadcast 전에 Handler 목록을 복사해 Callback 중 등록·해제가 원본 순회에 영향을 주지 않도록 처리

실제 사용처는 다음과 같습니다.

- Shape Component의 Begin Overlap·End Overlap·Hit Event
- UI Element의 Hover Enter·Hover Exit·Click Event
- Explosion System의 폭발 Event

관련 코드:

- `NipsEngine/Source/Engine/Core/Delegate/Delegate.h`
- `NipsEngine/Source/Engine/Core/Delegate/MulticastDelegate.h`
- `NipsEngine/Source/Engine/Core/Delegate/DelegateHandle.h`
- `NipsEngine/Source/Engine/Core/Delegate/DelegateMacros.h`

## 6. Collision Shape와 Event 전달

`UShapeComponent`를 기반으로 `UBoxComponent`, `USphereComponent`, `UCapsuleComponent`를 구현했습니다. 공통 설정인 Overlap Event 생성, Block, Movable, Debug Color를 직렬화하고 Property 패널에 노출합니다.

### Shape 구성

| Shape | 주요 데이터 |
| --- | --- |
| Box | Local Half Extent와 Transform 기반 World AABB |
| Sphere | Radius와 World Scale 기반 World AABB |
| Capsule | Radius·Half Height·회전축·World Scale 기반 Segment와 AABB |

`FitToStaticMesh`는 Static Mesh의 Local Bounds를 기준으로 Box Extent, Bounding Sphere Radius, Capsule의 장축·Radius·Half Height를 계산합니다.

### Collision Pipeline

```text
활성 Shape 수집
  → WorldSpatialIndex Sphere Query로 후보 축소
  → AABB Broad Phase
  → Shape 조합별 Narrow Phase
  → CurrentOverlaps 구성
  → PreviousOverlaps와 비교
      ├─ 신규 Pair → BeginOverlap / Hit
      ├─ 유지 Pair → Blocking Resolve
      └─ 제거 Pair → EndOverlap
  → Shape Multicast Delegate
  → Actor의 ScriptComponent Callback
```

Narrow Phase는 Sphere–Sphere, Sphere–Box, Box–Box, Capsule–Capsule, Sphere–Capsule, Box–Capsule 조합을 분기합니다. `FCollisionEvent`에는 Self·Other Component와 Actor, Tag, 접촉 위치·법선, 겹침 Bounds, Blocking 여부가 포함됩니다.

Begin·End Event는 각 Shape의 `GenerateOverlapEvents` 설정에 따라 전달됩니다. Hit Event는 두 Shape가 모두 Block일 때 신규 겹침 시 발생하며, Movable 설정과 Movement Component 여부에 따라 침투 해소 대상을 결정합니다.

관련 코드:

- `NipsEngine/Source/Engine/Component/ShapeComponent.h`
- `NipsEngine/Source/Engine/Component/ShapeComponent.cpp`
- `NipsEngine/Source/Engine/Collision/CollisionSystem.h`
- `NipsEngine/Source/Engine/Collision/CollisionSystem.cpp`
- `NipsEngine/Source/Engine/Spatial/WorldSpatialIndex.h`

## 7. 수집·중량·점수 시스템

수집물은 Actor Tag에 따라 적재 무게와 Money가 다릅니다.

| Tag | 무게 | Money |
| --- | ---: | ---: |
| `Trash` | 10 | 1 |
| `Resource` | 10 | 5 |
| `Recyclable` | 5 | 5 |
| `Premium` | 3 | 20 |

최대 적재량은 150입니다. 최대치를 넘기는 수집은 거부되며, `PlayerController.lua`는 현재 적재량 비율을 보트 질량에 반영합니다.

수집 경로는 두 가지입니다.

### Boat Overlap 수집

Boat가 일반 수집물과 새로 겹치면 Collision System이 적재 가능 여부를 확인하고, Stats 반영 후 Actor를 파괴합니다. Boat나 수집물이 폭발 Knockback 중이면 의도하지 않은 자동 수집을 막기 위해 처리하지 않습니다.

### Space 범위 수집

1. `Space`를 누르면 보트 중심 Ring을 최소 반경에서 생성합니다.
2. 누르는 동안 반경을 시간에 따라 최대치까지 확장합니다.
3. 키를 놓으면 범위 안의 수집 가능 Actor를 찾습니다.
4. 이미 이동 중인 수집물의 예약 무게까지 포함해 적재 한도를 검사합니다.
5. 선택된 Actor를 2차 Bézier 경로로 보트까지 이동시킵니다.
6. 도착 시 Stats를 적용하고 Actor를 제거합니다.

회수 Ring은 Render Collector가 `FCollectionSystem`의 중심·반경·색을 읽어 그립니다.

관련 코드:

- `NipsEngine/Source/Engine/DriftSalvage/CollectionSystem.h`
- `NipsEngine/Source/Engine/DriftSalvage/CollectionSystem.cpp`
- `NipsEngine/Source/Engine/Render/Collector/RenderCollector.cpp`
- `NipsEngine/Source/Engine/Scripting/LuaBinder.cpp`

## 8. Rock·Hazard와 연쇄 폭발

Collision Begin 시 Tag 조합에 따라 Drift Salvage 전용 상호작용을 실행합니다.

| 상호작용 | 결과 |
| --- | --- |
| Boat–Rock | 체력 1 감소, 이동 속도 초기화, 충돌 반대 방향 Knockback, 효과음 |
| Boat–Hazard | 체력 2 감소, 더 강한 Knockback, Hazard 위치에서 폭발 |
| Boat–Lighthouse | Game Over 요청과 체력 0 설정 |

`FExplosionSystem`은 폭발 반경 내 수집물에 거리 기반 지연과 힘을 적용합니다. 다른 Hazard는 거리에 비례한 시간 뒤 다시 폭발하도록 예약해 연쇄 반응을 만들고, 밀려난 Actor는 매 프레임 이동·감쇠됩니다. Rock 충돌에서는 침투를 해소하고 법선 방향 속도를 제거해 미끄러지는 동작을 만듭니다.

관련 코드:

- `NipsEngine/Source/Engine/DriftSalvage/ExplosionSystem.h`
- `NipsEngine/Source/Engine/DriftSalvage/ExplosionSystem.cpp`
- `NipsEngine/Source/Engine/Collision/CollisionSystem.cpp`
- `NipsEngine/Source/Engine/Core/SoundManager.cpp`

## 9. Lua HUD·게임 상태·재시작

`UGameEngine`은 `Scene_Game.Scene`을 활성화한 뒤 HUD Actor와 `UScriptComponent`를 생성하고 `LogoHud.lua`를 연결합니다.

`LogoHud.lua`가 구성하는 화면은 다음과 같습니다.

- START·RECORD가 있는 타이틀 메뉴
- 체력 Heart Animation
- Money와 적재 중량 Progress Bar
- 방향과 주변 Actor를 표시하는 미니맵
- Steering Wheel Animation
- Game Over와 최종 점수
- RESTART·TITLE 선택

게임 오버는 Lighthouse 도착 요청 또는 체력 0으로 전환됩니다. 최종 Money는 `ScoreManager.lua`에 전달되며, 내림차순으로 정렬한 상위 3개 기록을 `Saves/ScoreRecords.txt`에 저장합니다.

RESTART와 TITLE은 모두 `RequestGameRestart()`로 현재 World Context를 파괴하고 `Scene_Game.Scene`을 다시 불러옵니다. Lua 공유 전역 `_G.LogoHudNextStartMode`에 다음 시작 모드를 기록해, 재생성된 HUD가 Gameplay 또는 Title 상태로 진입합니다.

관련 코드:

- `NipsEngine/Source/Game/GameEngine.cpp`
- `NipsEngine/Source/Engine/UI/UIManager.h`
- `NipsEngine/Source/Engine/UI/UIElement.h`
- `NipsEngine/Asset/Scripts/LogoHud.lua`
- `NipsEngine/Asset/Scripts/ScoreManager.lua`

## 빌드 및 실행

### 요구 환경

- Windows 10/11
- Visual Studio 2022와 MSVC v143 C++ 도구 집합
- Windows 10 SDK
- Direct3D 11 지원 GPU와 드라이버
- NuGet Package Restore: `directxtk_desktop_win10 2025.10.28.2`

FMOD와 Lua·sol2·ImGui 소스 및 라이브러리는 `NipsEngine/ThirdParty`에 포함되어 있습니다.

### 프로젝트 파일 생성

1. 저장소 루트의 `GenerateProjectFiles.bat`을 실행합니다.
2. 생성된 `NipsEngine.sln`을 엽니다.
3. Visual Studio에서 NuGet Package Restore를 수행합니다.

### 빌드 구성

| 구성 | `WITH_EDITOR` | 실행 형태 |
| --- | ---: | --- |
| Debug / Release x64 | 1 | Editor Engine |
| ShippingDebug / Shipping x64 | 0 | Standalone Game Engine |

완성된 Drift Salvage 흐름은 `UGameEngine`이 실행되는 **ShippingDebug x64** 또는 **Shipping x64** 구성에서 확인할 수 있습니다. 실행 시 `Asset/Scene/Scene_Game.Scene`을 자동으로 로드하고 Lua HUD를 생성합니다.

Editor와 Script Component의 Property·Reload 기능을 확인하려면 Debug 또는 Release x64 구성을 사용합니다.

## 주요 파일

```text
NipsEngine/
├─ Asset/
│  ├─ Scene/Scene_Game.Scene
│  └─ Scripts/
│     ├─ PlayerController.lua
│     ├─ LogoHud.lua
│     └─ ScoreManager.lua
├─ Source/
│  ├─ Engine/
│  │  ├─ Collision/CollisionSystem.*
│  │  ├─ Component/Script/ScriptComponent.*
│  │  ├─ Component/ShapeComponent.*
│  │  ├─ Core/Delegate/
│  │  ├─ DriftSalvage/
│  │  │  ├─ CollectionSystem.*
│  │  │  └─ ExplosionSystem.*
│  │  ├─ Input/GameInputController.*
│  │  └─ Scripting/
│  │     ├─ LuaBinder.*
│  │     ├─ LuaScriptInstance.h
│  │     └─ LuaScriptSubsystem.*
│  └─ Game/GameEngine.*
└─ ThirdParty/
   ├─ Lua/
   └─ sol2/
```

## 구현 범위와 제한 사항

### 구현된 범위

- Lua 5.4.8 Runtime과 sol2 기반 Engine API Binding
- Component별 Script Environment와 Lifecycle Callback
- 입력 전용 Lua Controller와 Pawn·Camera 제어
- Script Path 편집, Script 목록 갱신, 수동 Reload
- Singlecast·Multicast Delegate와 Variadic Argument
- Box·Sphere·Capsule Shape 및 Shape 조합별 Collision
- Overlap·Hit Event의 Delegate와 Lua Callback 전달
- Drift Salvage 시작·종료·재시작·점수 저장 흐름
- 범위 수집·중량·보트 조작·Rock·Hazard·Lighthouse 상호작용

### 제한 사항

- 과제 명세의 LuaJIT 대신 **PUC Lua 5.4.8**을 사용합니다.
- Lua `coroutine` 표준 라이브러리는 열려 있지만, Delta Time 기반 Wait·Resume를 관리하는 엔진 Coroutine Scheduler는 구현되어 있지 않습니다.
- `template.lua` 복제, Scene·Actor 이름 기반 Script 파일 생성, 외부 편집기 실행 기능은 확인되지 않습니다.
- 파일 변경 감시 기반 자동 Hot Reload가 없으며 Property 패널의 Reload 또는 Play 중 Script Path 변경으로 다시 로드합니다.
- Reload는 Environment를 새로 만들기 때문에 기존 Script의 Local·Environment 상태가 유지되지 않습니다.
- Instance 격리는 Environment 수준입니다. `_G`와 Engine Binding은 하나의 Lua VM에서 공유되므로 완전한 VM·보안 Sandbox 분리는 아닙니다.
- `FGameInputController`의 `OnKeyDown` 전달은 현재 `GetKeyDown`이 아니라 `GetKey`를 사용해 키를 누르고 있는 동안 매 프레임 호출됩니다.
- C++의 `FCollisionEvent`는 접촉 정보를 보유하지만 Script Hit 전달 경로는 `OtherActor`만 넘겨 기본 `FHitResult`를 생성하므로 Lua의 `hitInfo`에는 실제 접촉 정보가 채워지지 않습니다.
- 게임 규칙 전체가 Lua에 있는 구조는 아닙니다. 입력·이동 파라미터와 HUD·상태 전환은 Lua, 충돌·수집·폭발·Stats 계산은 C++에 구현되어 있습니다.
- `ReleaseBuild.bat`은 현재 `Release x64`를 빌드하므로 Standalone Game이 아니라 Editor 구성을 패키징합니다. 완성 게임 실행 파일을 패키징하려면 Shipping 구성을 사용하도록 스크립트 조정이 필요합니다.
- 9+주차에서 추가된 시네마틱·카메라 연출·피격 화면 효과는 이 문서의 범위에 포함하지 않습니다.
