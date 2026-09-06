# AnimNotify System Guide

## 1. 문서 목적

이 문서는 현재 프로젝트에 구현된 AnimNotify 시스템을 이해하고, 유지보수하거나 확장하려는 개발자를 위한 상위 가이드이다.

이 문서가 답하려는 질문:

- 이 프로젝트의 AnimNotify는 어떤 레이어로 구성되어 있는가
- payload는 어떤 규칙으로 저장되고 읽히는가
- runtime, validation, editor는 어떻게 연결되는가
- built-in notify들은 각각 무엇을 하는가
- 새 notify를 추가하거나 기존 notify를 수정할 때 어디를 봐야 하는가

이 문서는 전체 구조를 설명하는 문서이며, 개별 notify 예시나 사용법은 `Docs` 아래의 개별 가이드 문서를 함께 참고하면 된다.

---

## 2. 시스템 개요

AnimNotify는 애니메이션 타임라인의 특정 시점 또는 특정 구간에서 runtime 동작을 발생시키는 시스템이다.

현재 프로젝트의 AnimNotify는 크게 두 종류로 나뉜다.

### 2.1 One-shot Notify

한 시점에서 1회 실행되는 notify.

예:

- `UAnimNotify_PlaySFX`
- `UAnimNotify_PlayVFX`
- `UAnimNotify_CameraShake`
- `UAnimNotify_GameplayEvent`
- `UAnimNotify_FootstepSurfaceEvent`
- `UAnimNotify_SpawnDecal`

### 2.2 Notify State

시작과 종료를 가지며, 필요하면 tick 동안 상태를 유지하는 notify.

예:

- `UAnimNotifyState_GameplayEventWindow`
- `UAnimNotifyState_PlayLoopingSFX`
- `UAnimNotifyState_AttackWindow`

---

## 3. 핵심 구조

현재 구현은 다음 레이어로 나뉜다.

1. semantic field identity
2. raw payload parsing / serialization
3. typed payload interpretation
4. runtime built-in execution
5. validation
6. editor rendering / picker

핵심 파일:

- `JSEngine/Source/Engine/Animation/AnimNotifySemanticFieldNames.h`
- `JSEngine/Source/Engine/Animation/AnimNotifySemanticFieldNames.cpp`
- `JSEngine/Source/Engine/Animation/AnimNotifyPayloadParser.h`
- `JSEngine/Source/Engine/Animation/AnimNotifyPayloadParser.cpp`
- `JSEngine/Source/Engine/Animation/AnimNotifyPayloadViews.h`
- `JSEngine/Source/Engine/Animation/AnimNotifyPayloadViews.cpp`
- `JSEngine/Source/Engine/Animation/AnimNotifyBuiltins.h`
- `JSEngine/Source/Engine/Animation/AnimNotifyBuiltins.cpp`
- `JSEngine/Source/Editor/Animation/AnimationSequenceNotifyValidation.h`
- `JSEngine/Source/Editor/Animation/AnimationSequenceNotifyValidation.cpp`
- `JSEngine/Source/Editor/Animation/AnimationSequenceNotifyAssetPickerHelpers.h`
- `JSEngine/Source/Editor/Animation/AnimationSequenceNotifyAssetPickerHelpers.cpp`
- `JSEngine/Source/Editor/Animation/AnimationSequenceViewerNotifyDetailsPanel.cpp`
- `JSEngine/Source/Editor/Animation/AnimationSequencePreviewController.h`
- `JSEngine/Source/Editor/Animation/AnimationSequencePreviewScene.cpp`

---

## 4. 레이어별 역할

### 4.1 Semantic Field Layer

파일:

- `AnimNotifySemanticFieldNames.*`

역할:

- canonical payload key 정의
- legacy alias 정의
- semantic field id 기준 lookup key 제공

이 레이어는 "논리적 필드 identity"를 담당한다. runtime, validation, schema, editor가 같은 필드를 같은 이름으로 바라보도록 만드는 기준점이다.

예:

- `SoundCue`
- `SocketName`
- `EventName`
- `PayloadValue`
- `Scale`

중요한 점:

- 새 payload text를 쓸 때는 canonical key를 기준으로 쓴다
- 오래된 데이터는 legacy alias로도 읽을 수 있다
- semantic meaning은 여기서 정의하지만, 실제 parse나 runtime behavior는 여기서 결정하지 않는다

### 4.2 Payload Parser Layer

파일:

- `AnimNotifyPayloadParser.*`

역할:

- raw payload string parse
- primitive typed read
- key/value mutation
- canonical serialization

이 레이어는 notify-specific 의미를 모른다. 단순히 key/value 문자열을 다루는 parser이다.

중요한 점:

- parser는 notify-specific default를 가지지 않는다
- parser는 required 여부를 판단하지 않는다
- parser는 문자열을 float/int/bool/name으로 파싱하는 역할만 맡는다

### 4.3 Typed Payload View Layer

파일:

- `AnimNotifyPayloadViews.*`

역할:

- raw payload를 notify 의미에 맞는 typed struct로 해석
- optional field default 제공
- runtime/validation 소비를 쉽게 만드는 semantic object 제공

현재 대표 payload view:

- `FPlaySfxPayloadView`
- `FPlayVfxPayloadView`
- `FCameraShakePayloadView`

중요한 점:

- parser 위에 올라가는 semantic bridge 역할
- serialization은 하지 않는다
- validation severity는 결정하지 않는다
- runtime dispatch도 직접 하지 않는다

### 4.4 Runtime Built-in Execution Layer

파일:

- `AnimNotifyBuiltins.*`

역할:

- built-in notify class 구현
- thin entrypoint 유지
- runtime helper dispatch
- attach target resolve, trace, state bookkeeping 같은 runtime-only logic 보유

현재 구조 원칙:

- `Notify(...)`, `NotifyBegin(...)`, `NotifyTick(...)`, `NotifyEnd(...)`는 얇게 유지
- payload build
- required field guard
- helper dispatch

중요한 점:

- runtime-only helper는 `AnimNotifyBuiltins.cpp` 로컬에 둔다
- editor나 validation concern을 섞지 않는다
- fallback behavior는 여기서 바꾸면 영향 범위가 크므로 보수적으로 다뤄야 한다

### 4.5 Validation Layer

파일:

- `AnimationSequenceNotifyValidation.*`

역할:

- notify event validity 검사
- payload required/type 검사
- preview context 검사
- built-in specific validation

현재 구조:

1. generic validation
2. preview/context-sensitive validation
3. built-in notify-specific validation

중요한 점:

- preview validation은 현재 preview 기준 경고이다
- preview mismatch는 runtime invalid와 동일하지 않다
- runtime에서 유효한 payload라도 현재 preview mesh에서는 warning이 날 수 있다

### 4.6 Editor Rendering / Picker Layer

파일:

- `AnimationSequenceViewerNotifyDetailsPanel.cpp`
- `AnimationSequenceNotifyAssetPickerHelpers.*`

역할:

- structured payload editor rendering
- primitive field rendering
- asset picker rendering
- attach target / component picker rendering
- raw payload fallback editor

중요한 점:

- details panel은 draw/apply/sync orchestration에 가깝다
- stored value / display label / fallback label semantics는 asset picker helper가 담당한다

---

## 5. 데이터 흐름

AnimNotify payload가 editor와 runtime에서 소비되는 흐름은 다음과 같다.

1. animation sequence에 notify event가 저장된다
2. payload는 raw string 형태로 저장된다
3. editor/runtime/validation이 필요할 때 parser가 raw payload를 읽는다
4. semantic lookup이 canonical key와 legacy alias를 함께 해석한다
5. 필요 시 typed payload view가 semantic object를 만든다
6. runtime built-in, validation, editor가 이를 각각 소비한다

요약:

`NotifyEvent.Payload`
-> `FAnimNotifyPayloadParser`
-> semantic lookup
-> typed payload view 또는 validation/editor helper
-> runtime execution / validation / editor rendering

---

## 6. Payload 규칙

### 6.1 기본 저장 형식

payload는 key/value 문자열로 저장된다.

예:

```txt
SoundCue=Asset/Audio/Slash1.WAV;SocketName=hand_r;VolumeMultiplier=1.0;Spatialized=true
```

### 6.2 Canonical Key와 Legacy Alias

- 새로 serialize할 때는 canonical key를 쓴다
- 읽을 때는 legacy alias도 허용한다

의미:

- 오래된 데이터도 읽을 수 있다
- 저장은 점차 canonical form으로 정리된다

### 6.3 Required / Optional Field

schema에서 required/optional 여부를 정의한다.

- required: 없으면 validation error 가능
- optional: payload view나 runtime helper에서 default 사용 가능

### 6.4 Primitive Parsing

parser가 제공하는 기본 타입:

- string
- name
- float
- int
- bool

주의:

- parser는 parse만 한다
- semantic default는 payload view나 상위 소비자에서 처리한다

### 6.5 Raw Payload 직접 수정 시 주의점

raw payload editor를 사용하면:

- structured editor가 제공하는 picker 제약 없이 직접 입력 가능
- inventory 밖 값도 유지 가능
- manual override 값도 저장 가능

하지만:

- typo 위험이 높다
- validation warning이 더 중요해진다
- runtime 소비 방식까지 이해하고 수정해야 한다

---

## 7. Attach Target 규칙

이 프로젝트의 attach target은 socket-only가 아니라 socket + bone 둘 다 허용한다.

중요한 규칙:

- runtime은 `USkinnedMeshComponent::HasSocket(...)` / `GetSocketTransform(...)` semantics를 따른다
- preview validation도 같은 해석 규칙을 사용한다
- picker/enumeration도 같은 의미를 공유한다

즉:

- runtime
- preview validation
- editor authoring

이 세 레이어가 같은 attach target interpretation을 보도록 맞춰져 있다.

target miss 시 fallback 예:

- audio/vfx는 component world location fallback
- footstep/decal trace는 resolved-or-fallback location에서 trace 시작

---

## 8. Built-in Notify Reference

### 8.1 UAnimNotify_PlaySFX

목적:

- 사운드 큐 재생

대표 필드:

- `SoundCue`
- `SocketName`
- `VolumeMultiplier`
- `Spatialized`

특징:

- typed payload view 사용
- `Spatialized=true`면 attach target location 기반 재생
- `Spatialized=false`면 일반 SFX 재생

### 8.2 UAnimNotifyState_PlayLoopingSFX

목적:

- looping sound 시작/유지/종료

대표 필드:

- `SoundCue`
- `SocketName`
- `VolumeMultiplier`
- `Spatialized`

특징:

- `PlaySFX` payload shape 재사용
- begin에서 audio handle 생성
- tick에서 spatialized 위치 업데이트
- end에서 handle 정리

### 8.3 UAnimNotify_PlayVFX

목적:

- 시각 효과 재생

대표 필드:

- `Effect`
- `SocketName`
- `Attached`
- `Scale`

특징:

- typed payload view 사용
- attach target location 사용 가능
- attached 여부와 scale 의미가 분리됨

### 8.4 UAnimNotify_CameraShake

목적:

- 카메라 쉐이크 실행

대표 필드:

- `Shake`
- `Scale`

특징:

- typed payload view 사용
- shake key와 scale만 다룸

### 8.5 UAnimNotify_GameplayEvent

목적:

- generic gameplay event 전달

대표 필드:

- `EventName`
- `PayloadValue`

특징:

- string event channel 성격
- runtime는 `EventName`과 `PayloadValue`를 `AnimInstance`로 전달
- 팀별 확장용 special-case 로직이 이 family에 붙을 수 있음

주의:

- C++ 소비 측에서 `if (EventName == "...")` 분기가 커지기 쉬움
- 고정된 종류의 gameplay action이 늘어나면 구조적으로 취약해진다

### 8.6 UAnimNotifyState_GameplayEventWindow

목적:

- begin/end gameplay event window 전달

대표 필드:

- `EventName`
- `EndEventName`
- `PayloadValue`

특징:

- begin 시 `EventName`
- end 시 `EndEventName`이 있으면 우선 사용
- 없으면 `EventName` 재사용

### 8.7 UAnimNotify_FootstepSurfaceEvent

목적:

- footstep event + surface name + hit location 전달

대표 필드:

- `EventName`
- `SocketName`
- `TraceDistance`
- `PayloadValue`

특징:

- attach target location에서 downward trace
- hit surface를 material/component/actor 기준으로 resolve
- miss 시 `"Default"` surface fallback 사용

### 8.8 UAnimNotify_SpawnDecal

목적:

- 데칼 spawn

대표 필드:

- `Decal`
- `SocketName`
- `TraceDistance`
- `Size`
- `Lifetime`
- `AlignToHitNormal`

특징:

- attach target location에서 downward trace
- hit 위치와 normal 사용
- miss 시 origin fallback / up-vector fallback 유지

### 8.9 UAnimNotifyState_AttackWindow

목적:

- 특정 primitive component의 overlap generation을 window 동안 활성화
- 필요 시 attack tag 상태를 함께 관리

대표 필드:

- `ComponentName`
- `AttackId`

특징:

- notify state
- component overlap state ref-count 관리
- actor tag acquire/release 관리
- release 순서가 민감함

---

## 9. Asset-backed Field 규칙

asset picker helper는 stored value와 display label을 분리해서 다룬다.

### 9.1 SoundCue

- stored value: project-relative audio asset path
- display label: friendly filename stem

### 9.2 VFX

- stored value: asset-service path string
- display label: editor display name 또는 path

### 9.3 CameraShake

- stored value: registered pattern class name
- display label: leading `U`를 제거한 friendly name

중요한 점:

- payload에는 stored value가 들어간다
- display label은 editor convenience일 뿐 runtime lookup meaning을 바꾸지 않는다
- picker inventory 밖 값도 fallback label로 계속 보이게 한다

---

## 10. Validation 규칙

validation은 세 층으로 보면 이해하기 쉽다.

### 10.1 Generic Validation

- notify class/type compatibility
- required field check
- malformed primitive value check
- generic payload sanity

### 10.2 Preview / Context-sensitive Validation

- attach target 존재 여부
- preview primitive component 존재 여부
- current preview 기준 warning

### 10.3 Built-in Specific Validation

- `Scale > 0`
- `TraceDistance > 0`
- notify별 custom warning

주의:

- preview mismatch는 warning일 뿐, 반드시 runtime invalid는 아니다
- runtime mesh나 actor가 달라지면 같은 payload가 정상일 수 있다

---

## 11. Editor 사용 흐름

### 11.1 Structured Payload Editor

기본 사용 경로이다.

field kind:

- text/name
- float/int
- bool
- asset picker
- attach target picker
- component picker

### 11.2 Asset Picker

asset-backed field용 UI.

대상:

- `SoundCue`
- `VFX`
- `CameraShake`

### 11.3 Preview-aware Picker

preview controller에서 후보를 가져온다.

대상:

- attach target
- primitive component

### 11.4 Raw Payload Editor

advanced fallback 경로.

언제 쓰는가:

- picker inventory에 없는 값 사용
- manual override 유지
- 실험적 payload 입력

---

## 12. GameplayEvent 계열에 대한 별도 주의

`GameplayEvent`와 `GameplayEventWindow`는 typed notify라기보다 generic string event bus에 가깝다.

장점:

- 빠르게 확장 가능
- Lua/스크립트 브리지와 잘 맞음

단점:

- 문자열 오타에 취약함
- rename safety 부족
- payload meaning이 이벤트 종류별로 흩어질 수 있음
- C++ 소비 측이 `if (EventName == "...")` 체인으로 커질 위험이 있음

권장:

- truly generic bridge가 필요할 때만 사용
- 고정된 gameplay action이 많아지면
  - 새 built-in notify를 추가하거나
  - `FName` 기반 dispatch
  - handler registration map
  중 하나를 고려하는 것이 좋다

---

## 13. 새 Notify 추가 절차

새 built-in notify를 추가할 때 권장 순서:

1. one-shot인지 state인지 결정
2. payload fields 정의
3. 기존 semantic field 재사용 여부 판단
4. 새 semantic field가 필요하면 `AnimNotifySemanticFieldNames.*` 추가
5. raw parse만으로 충분한지, typed payload view가 필요한지 판단
6. runtime built-in 구현
7. validation rule 추가
8. editor schema와 field renderer 연결
9. asset picker / preview picker 필요 여부 확인
10. canonical serialization과 legacy read compatibility 확인

실무 원칙:

- parser에 notify 의미를 넣지 않는다
- payload view에 runtime dispatch 책임을 넣지 않는다
- `Notify(...)` body를 두껍게 만들지 않는다
- editor helper에 runtime semantics를 섞지 않는다

---

## 14. 디버깅 가이드

### 14.1 Notify가 아예 안 불릴 때

확인 순서:

1. sequence에 notify가 실제로 있는가
2. notify class가 맞는가
3. `AnimInstance` update에서 notify dispatch까지 오는가
4. state notify라면 duration/timing이 맞는가

### 14.2 Payload가 안 읽힐 때

확인 순서:

1. canonical key 이름이 맞는가
2. legacy alias와 혼동이 없는가
3. required field가 비어 있지 않은가
4. parser primitive parse가 실패하지 않는가

### 14.3 Attach target 관련 문제

확인 순서:

1. socket/bone 이름이 정확한가
2. preview mesh와 runtime mesh가 다른가
3. miss fallback이 의도대로 동작하는가

### 14.4 GameplayEvent가 소비되지 않을 때

확인 순서:

1. `EventName`이 비어 있지 않은가
2. `DispatchGameplayAnimNotifyEvent(...)`까지 도달하는가
3. 하위 소비자(native override, Lua)가 그 이름을 처리하는가

### 14.5 AttackWindow가 이상할 때

확인 순서:

1. `ComponentName`이 실제 primitive component를 가리키는가
2. overlap generation original state가 복원되는가
3. tag acquire/release 순서가 꼬이지 않았는가

---

## 15. 추천 읽기 순서

처음 읽는 개발자에게 추천하는 순서:

1. `AnimNotifySemanticFieldNames.h`
2. `AnimNotifyPayloadParser.h`
3. `AnimNotifyPayloadViews.h`
4. `AnimNotifyBuiltins.h`
5. `AnimNotifyBuiltins.cpp`
6. `AnimationSequenceNotifyValidation.cpp`
7. `AnimationSequenceNotifyAssetPickerHelpers.*`
8. `AnimationSequenceViewerNotifyDetailsPanel.cpp`

이 순서로 보면:

- field identity
- raw payload
- semantic interpretation
- runtime execution
- validation
- editor

순으로 자연스럽게 이해할 수 있다.

---

## 16. 유지보수 원칙 요약

이 시스템을 건드릴 때 지켜야 할 핵심 원칙:

- parser에 notify 의미를 넣지 않는다
- payload view는 semantic interpretation까지만 담당한다
- runtime built-in entrypoint는 얇게 유지한다
- validation과 runtime semantics를 섞지 않는다
- attach target semantics를 runtime/editor/validation마다 다르게 만들지 않는다
- fallback behavior를 함부로 바꾸지 않는다
- generic string event dispatch를 남용하지 않는다

---

## 17. 참고 문서

개별 사용 예시나 notify별 실무 가이드는 다음 문서를 함께 보면 좋다.

- `Docs/AnimationSequenceViewer_AnimNotify_Guide.md`
- `Docs/AnimNotify_PlayVFX_Guide.md`
- `Docs/AnimNotify_CameraShake_Guide.md`
- `Docs/AnimNotify_FootstepSurfaceEvent_Guide.md`
- `Docs/AnimNotify_GameplayEvent_Guide.md`
- `Docs/AnimNotify_SpawnDecal_Guide.md`
- `Docs/AnimNotifyState_PlayLoopingSFX_Guide.md`
- `Docs/AnimNotifyState_GameplayEventWindow_Guide.md`
- `Docs/AnimNotifyState_AttackWindow_Guide.md`

이 문서는 상위 구조 문서이고, 위 문서들은 개별 기능 참고 문서로 보는 것이 좋다.
