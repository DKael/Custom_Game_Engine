# Animation Sequence Viewer AnimNotify 사용 가이드

이 문서는 `Animation Sequence Viewer`에서 `AnimNotify`를 처음 사용하는 사람을 위한 문서입니다.

이 문서를 읽고 나면 아래 작업을 할 수 있게 만드는 것이 목표입니다.

1. 타임라인에 `Notify`를 추가한다.
2. `Notify`와 `Notify State`의 차이를 이해한다.
3. 내장 `AnimNotify` 클래스에 맞는 `Payload`를 입력한다.
4. `Validation`으로 문제를 확인하고 수정한다.

## 1. AnimNotify가 무엇인가

`AnimNotify`는 애니메이션의 특정 시점 또는 특정 시간 구간에서 이벤트를 발생시키는 기능입니다.

예를 들면 아래처럼 사용합니다.

- 발소리 재생
- 공격 판정 시작/종료
- 카메라 흔들림 실행
- VFX 재생
- 게임플레이 이벤트 전송

즉, "이 애니메이션의 몇 초 지점에서 무엇을 할지"를 데이터로 심는 기능입니다.

## 2. 가장 먼저 알아야 할 개념

### 2.1 Notify

한 시점에서 한 번 실행되는 이벤트입니다.

예:

- 발이 바닥에 닿는 순간 효과음 재생
- 검이 맞는 순간 카메라 흔들림 실행
- 특정 프레임에서 게임플레이 이벤트 전송

### 2.2 Notify State

시작 시점과 종료 시점을 가지는 구간형 이벤트입니다.

예:

- 공격 판정이 켜져 있는 구간
- 루프 사운드가 유지되는 구간
- 무적 상태가 유지되는 구간

### 2.3 Payload

`Notify`와 함께 전달되는 추가 데이터입니다.

현재 시스템에서는 보통 아래 문자열 형식을 사용합니다.

```txt
Key=Value;Key=Value;Key=Value
```

예:

```txt
SoundCue=Footstep_Stone;SocketName=foot_l;VolumeMultiplier=0.9;Spatialized=true
```

## 3. Animation Sequence Viewer에서 어디를 보면 되는가

`AnimNotify` 작업은 주로 아래 3개 영역에서 진행합니다.

### 3.1 타임라인의 `Notifies`

여기서 `Notify`를 추가, 선택, 이동, 삭제합니다.

- `Notify Track` 안에 마커가 표시됩니다.
- 일반 `Notify`는 짧은 마커처럼 보입니다.
- `Notify State`는 길이가 있는 막대처럼 보입니다.

### 3.2 하단 왼쪽 `Selected Notify`

선택한 `Notify`의 상세 정보를 편집하는 곳입니다.

여기서 자주 수정하는 항목:

- `Name`
- `Type`
- `Notify Class`
- `Time`
- `Duration`
- `Payload`
- `Color`

### 3.3 하단 오른쪽 `Notify Debug`

검증과 디버그 확인용 영역입니다.

탭 구성:

- `Validation`
- `Recent Fired`
- `Event Log`

처음에는 `Validation`을 가장 자주 보면 됩니다.

## 4. 기본 사용 순서

처음에는 아래 순서로 작업하면 됩니다.

1. 애니메이션 시퀀스를 연다.
2. `Notifies` 섹션을 펼친다.
3. 필요하면 `Notify Track`를 추가한다.
4. 원하는 시간 위치에 `Notify`를 만든다.
5. 생성된 마커를 선택한다.
6. `Selected Notify`에서 `Type`, `Notify Class`, `Payload`를 설정한다.
7. `Validation`에서 오류와 경고를 확인한다.
8. 저장한다.

## 5. Notify Track 관리

### 5.1 Track 추가

`Notifies` 헤더를 우클릭하면 `Add Track` 메뉴가 있습니다.

추가하면 기본 이름은 아래처럼 만들어집니다.

- `Track 1`
- `Track 2`
- `Track 3`

### 5.2 Track 이름 변경

트랙 행 우클릭:

- `Rename Track`

### 5.3 Track 순서 변경

트랙 행 우클릭:

- `Move Up`
- `Move Down`

### 5.4 Track 삭제

비어 있는 트랙만 삭제할 수 있습니다.

즉, 트랙 안에 `Notify`가 하나라도 있으면 먼저 마커를 비워야 합니다.

## 6. Notify 추가 방법

### 6.1 더블 클릭

가장 쉬운 방법입니다.

`Notify Track`의 빈 공간을 더블 클릭하면 해당 위치에 `Notify`가 생성됩니다.

생성 직후 기본값:

- `Name = Notify`
- `Type = Notify`
- `Duration = 0`
- `Notify Class = UAnimNotify`

### 6.2 우클릭 메뉴

트랙 빈 공간 우클릭:

- `Add Notify Here`
- `Paste Notify Here`

### 6.3 `Selected Notify` 툴바 버튼

하단 `Selected Notify` 영역 상단의 `Add Notify` 버튼을 눌러도 현재 시간 기준으로 추가할 수 있습니다.

## 7. 생성 후 할 수 있는 편집

### 7.1 Basic

- `Name`
- `Type`
- `Notify Class`

권장:

- 이름은 기능이 보이게 짓는 것이 좋습니다.
- 예: `Footstep_L`, `ComboOpen`, `SwordTrail`, `AttackWindow`

### 7.2 Timing

- `Time`
- `Duration`

주의:

- 일반 `Notify`는 보통 `Duration = 0`
- `Notify State`는 보통 `Duration > 0`

### 7.3 마커 드래그

- 마커를 드래그하면 시간 위치를 바꿀 수 있습니다.
- `Notify State`는 오른쪽 끝을 드래그해서 길이도 조절할 수 있습니다.

### 7.4 Visual

`Color`를 바꿔서 마커 색을 구분할 수 있습니다.

예:

- 오디오 계열: 파랑
- 공격 계열: 빨강
- 이동/발소리 계열: 초록
- 연출 계열: 노랑

## 8. 우클릭 메뉴 정리

### 8.1 Notify 마커 우클릭

- `Duplicate`
- `Copy`
- `Delete`
- `Move To Track`

### 8.2 Track 행 우클릭

- `Rename Track`
- `Paste Notify Here`
- `Move Selected Notify Here`
- `Move Up`
- `Move Down`
- `Delete Empty Track`

## 9. Copy / Paste / Duplicate 차이

### 9.1 Duplicate

선택한 `Notify`를 같은 트랙에 복제합니다.

- 새 `StableId`가 발급됩니다.
- 시간은 현재 프레임 레이트 기준으로 약간 뒤쪽에 복제됩니다.

### 9.2 Copy

선택한 `Notify`를 내부 클립보드에 복사합니다.

### 9.3 Paste

복사된 `Notify`를 현재 시간 또는 우클릭한 위치에 붙여 넣습니다.

복사되는 내용:

- `Type`
- `Name`
- `Duration`
- `Color`
- `NotifyClassName`
- `Payload`

붙여 넣을 때 새로 정해지는 것:

- `Time`
- `StableId`

## 10. Type과 Notify Class를 어떻게 고를까

먼저 `Type`을 정하고, 그 다음 맞는 `Notify Class`를 고르면 쉽습니다.

### 10.1 `Type = Notify`

한 번 실행되는 클래스용입니다.

대표 클래스:

- `UAnimNotify_PlaySFX`
- `UAnimNotify_GameplayEvent`
- `UAnimNotify_CameraShake`
- `UAnimNotify_FootstepSurfaceEvent`
- `UAnimNotify_PlayVFX`
- `UAnimNotify_SpawnDecal`
- `UAnimNotifyLog`

### 10.2 `Type = Notify State`

시작/종료 구간이 필요한 클래스용입니다.

대표 클래스:

- `UAnimNotifyState_GameplayEventWindow`
- `UAnimNotifyState_PlayLoopingSFX`
- `UAnimNotifyState_AttackWindow`
- `UAnimNotifyStateLog`

### 10.3 잘못 조합하면?

예를 들어:

- `Type`은 `Notify`인데 `Notify State` 클래스를 선택
- `Type`은 `Notify State`인데 일반 `Notify` 클래스를 선택

이 경우 `Validation`에서 오류가 납니다.

## 11. 현재 내장 파생 AnimNotify 클래스 목록

현재 코드 기준 내장 파생 클래스는 아래입니다.

### 일반 Notify

- `UAnimNotify_PlaySFX`
- `UAnimNotify_GameplayEvent`
- `UAnimNotify_CameraShake`
- `UAnimNotify_FootstepSurfaceEvent`
- `UAnimNotify_PlayVFX`
- `UAnimNotify_SpawnDecal`
- `UAnimNotifyLog`

### Notify State

- `UAnimNotifyState_GameplayEventWindow`
- `UAnimNotifyState_PlayLoopingSFX`
- `UAnimNotifyState_AttackWindow`
- `UAnimNotifyStateLog`

## 12. 내장 클래스별 요약

### 12.1 `UAnimNotify_PlaySFX`

효과음을 1회 재생합니다.

주요 필드:

- `SoundCue` 필수
- `SocketName` 선택
- `VolumeMultiplier` 선택
- `Spatialized` 선택

예:

```txt
SoundCue=Footstep_Stone;SocketName=foot_l;VolumeMultiplier=0.9;Spatialized=true
```

### 12.2 `UAnimNotify_GameplayEvent`

게임플레이 이벤트를 1회 전송합니다.

주요 필드:

- `EventName` 필수
- `Payload` 선택

예:

```txt
EventName=Combo.Open;Payload=Light1
```

### 12.3 `UAnimNotify_CameraShake`

카메라 흔들림을 1회 실행합니다.

주요 필드:

- `Shake` 필수
- `Scale` 선택

예:

```txt
Shake=HeavyHit;Scale=0.75
```

### 12.4 `UAnimNotify_FootstepSurfaceEvent`

발 아래 표면을 추적해서 발소리용 게임플레이 이벤트를 보냅니다.

주요 필드:

- `EventName` 필수
- `SocketName` 필수
- `TraceDistance` 선택
- `Payload` 선택

예:

```txt
EventName=Footstep;SocketName=foot_l;TraceDistance=25.0;Payload=Run
```

### 12.5 `UAnimNotify_PlayVFX`

VFX를 재생합니다.

주요 필드:

- `Effect` 필수
- `SocketName` 선택
- `Attached` 선택
- `Scale` 선택

예:

```txt
Effect=SlashArc;SocketName=weapon_tip;Attached=true;Scale=1.0
```

### 12.6 `UAnimNotify_SpawnDecal`

데칼을 생성합니다.

주요 필드:

- `Decal` 필수
- `SocketName` 선택
- `TraceDistance` 선택
- `Size` 선택
- `Lifetime` 선택
- `AlignToHitNormal` 선택

예:

```txt
Decal=FootDust;SocketName=foot_l;TraceDistance=50.0;Size=1.0;Lifetime=2.0;AlignToHitNormal=true
```

### 12.7 `UAnimNotifyState_GameplayEventWindow`

구간 시작과 종료에서 게임플레이 이벤트를 보냅니다.

주요 필드:

- `EventName` 필수
- `EndEventName` 선택
- `Payload` 선택

예:

```txt
EventName=InvulnWindow;EndEventName=InvulnWindowEnd;Payload=Short
```

### 12.8 `UAnimNotifyState_PlayLoopingSFX`

구간 동안 루프 사운드를 재생합니다.

주요 필드:

- `SoundCue` 필수
- `SocketName` 선택
- `VolumeMultiplier` 선택
- `Spatialized` 선택

예:

```txt
SoundCue=SwordHum;SocketName=weapon_tip;VolumeMultiplier=1.0;Spatialized=true
```

### 12.9 `UAnimNotifyState_AttackWindow`

구간 동안 공격 판정용 primitive component의 overlap을 열고 닫습니다.

주요 필드:

- `ComponentName` 필수
- `AttackId` 선택
- `Priority` 선택

예:

```txt
ComponentName=WeaponHitbox;AttackId=Light1;Priority=1
```

### 12.10 `UAnimNotifyLog` / `UAnimNotifyStateLog`

디버그용 로그 출력 클래스입니다.

- `UAnimNotifyLog`: 일반 `Notify` 로그
- `UAnimNotifyStateLog`: `Begin`, `Tick`, `End` 로그

처음 연결 테스트할 때 매우 유용합니다.

## 13. Payload 입력 방법

### 13.1 구조화된 편집기

클래스에 스키마가 있으면 필드별 입력 UI가 나타납니다.

예:

- `Sound Cue`
- `Socket Name`
- `Volume Multiplier`
- `Spatialized`

이 방식이 가장 안전합니다.

장점:

- 키 이름 오타 방지
- 저장 시 canonical 형태로 정리
- 검증 메시지가 더 정확함

### 13.2 Raw Payload 직접 입력

스키마가 없거나 수동 수정이 필요하면 문자열로 직접 입력할 수 있습니다.

형식:

```txt
Key=Value;Key=Value;Key=Value
```

예:

```txt
EventName=Combo.Open;Payload=Light1
```

### 13.3 `Advanced Raw Payload`

구조화된 편집기를 쓰는 클래스도 raw 문자열을 직접 확인하고 수정할 수 있습니다.

사용하기 좋은 상황:

- 기존 데이터 복사
- 레거시 키 확인
- 미세 조정

## 14. Socket / Component 선택 필드 이해하기

### 14.1 Socket Picker

예:

- `SocketName`

프리뷰 메시에 소켓이 있으면 목록으로 고를 수 있습니다.

### 14.2 Component Picker

예:

- `ComponentName`

프리뷰 액터의 `PrimitiveComponent` 목록이 있으면 목록으로 고를 수 있습니다.

특히 `UAnimNotifyState_AttackWindow` 설정 시 중요합니다.

## 15. Validation 보는 법

`Notify` 작업에서 가장 중요한 습관은 저장 전에 `Validation`을 확인하는 것입니다.

### 15.1 어디서 보나

2곳에서 볼 수 있습니다.

1. `Selected Notify > Validation`
2. `Notify Debug > Validation`

### 15.2 어떤 종류가 나오나

- `Error`
- `Warning`
- `Info`

### 15.3 자주 나오는 문제

#### 필수 Payload 누락

예:

- `PlaySFX`인데 `SoundCue` 없음
- `GameplayEvent`인데 `EventName` 없음
- `AttackWindow`인데 `ComponentName` 없음

#### Type / Class 불일치

- 일반 `Notify`에 state 클래스 사용
- state 타입에 일반 클래스 사용

#### `Notify State`인데 Duration이 0 이하

구간형인데 길이가 없어서 경고가 납니다.

#### 일반 `Notify`인데 Duration이 0보다 큼

설정 의도가 맞는지 다시 봐야 합니다.

#### 이름 없음

이름이 비어 있으면 추적이 어렵고 검증 메시지가 나올 수 있습니다.

#### 중복 `StableId`

정상적인 추가/복제 기능을 쓰면 거의 발생하지 않지만, 수동 데이터 편집 시 생길 수 있습니다.

#### 프리뷰 컨텍스트와 불일치

예:

- 존재하지 않는 `SocketName`
- 존재하지 않는 `ComponentName`

### 15.4 Validation 목록 활용

오른쪽 `Validation` 목록에서 항목을 클릭하면 해당 `Notify`를 선택할 수 있습니다.

## 16. 처음 쓰는 사람을 위한 추천 연습 순서

### 16.1 1단계: 로그 Notify로 연결 확인

1. 빈 타이밍에 `Notify` 추가
2. `Notify Class = UAnimNotifyLog`
3. `Name = Test_Log`
4. 필요하면 `Payload=Hello`
5. 재생 후 로그 또는 `Recent Fired` 확인

### 16.2 2단계: 발소리 Notify 만들기

1. 발 접지 타이밍에 `Notify` 추가
2. `Notify Class = UAnimNotify_PlaySFX`
3. `SoundCue`, `SocketName`, `Spatialized` 설정

### 16.3 3단계: 공격 구간 Notify State 만들기

1. 구간 시작 지점에 `Notify State` 생성
2. 오른쪽 끝을 드래그해서 구간 길이 조절
3. `Notify Class = UAnimNotifyState_AttackWindow`
4. `ComponentName` 설정

## 17. 실전 예시

### 17.1 발소리

```txt
SoundCue=Footstep_Stone;SocketName=foot_l;VolumeMultiplier=1.0;Spatialized=true
```

### 17.2 콤보 입력 가능 구간

```txt
EventName=Combo.Open;EndEventName=Combo.Close;Payload=Light1
```

### 17.3 공격 판정 구간

```txt
ComponentName=WeaponHitbox;AttackId=Light1;Priority=1
```

### 17.4 검 궤적 VFX

```txt
Effect=SlashArc;SocketName=weapon_tip;Attached=true;Scale=1.0
```

## 18. 자주 하는 실수

### 18.1 이름만 바꾸고 클래스는 기본값으로 두는 경우

`Name`은 표시 이름일 뿐이고, 실제 동작은 `Notify Class`가 결정합니다.

### 18.2 Notify와 Notify State를 혼동하는 경우

한 번 실행인지, 구간 전체 영향인지 먼저 정해야 합니다.

### 18.3 Payload 키를 임의로 바꾸는 경우

예를 들어 `SoundCue`를 `Sound`로 쓰면 동작하지 않을 수 있습니다.

### 18.4 없는 소켓 이름을 넣는 경우

가능하면 목록 선택기를 우선 사용하세요.

### 18.5 AttackWindow에 잘못된 컴포넌트 이름을 넣는 경우

`ComponentName`은 실제 `PrimitiveComponent` 이름과 정확히 일치해야 합니다.

## 19. 체크리스트

- `Type`이 맞는가
- `Notify Class`가 맞는가
- `Name`이 기능을 설명하는가
- 필수 `Payload` 키가 있는가
- `SocketName` 또는 `ComponentName`이 실제 프리뷰와 맞는가
- `Notify State`라면 `Duration`이 충분한가
- `Validation`에 `Error`가 없는가

## 20. 관련 문서

- [AnimationSequenceViewer_User_Guide.md](/C:/development/W11_Team3_Engine/Docs/AnimationSequenceViewer_User_Guide.md)
- [AnimationSequenceViewer_Footstep_Example_walk_sound.md](/C:/development/W11_Team3_Engine/Docs/AnimationSequenceViewer_Footstep_Example_walk_sound.md)
- [AnimNotify_GameplayEvent_Guide.md](/C:/development/W11_Team3_Engine/Docs/AnimNotify_GameplayEvent_Guide.md)
- [AnimNotify_CameraShake_Guide.md](/C:/development/W11_Team3_Engine/Docs/AnimNotify_CameraShake_Guide.md)
- [AnimNotify_PlayVFX_Guide.md](/C:/development/W11_Team3_Engine/Docs/AnimNotify_PlayVFX_Guide.md)
- [AnimNotify_SpawnDecal_Guide.md](/C:/development/W11_Team3_Engine/Docs/AnimNotify_SpawnDecal_Guide.md)
- [AnimNotify_FootstepSurfaceEvent_Guide.md](/C:/development/W11_Team3_Engine/Docs/AnimNotify_FootstepSurfaceEvent_Guide.md)
- [AnimNotifyState_GameplayEventWindow_Guide.md](/C:/development/W11_Team3_Engine/Docs/AnimNotifyState_GameplayEventWindow_Guide.md)
- [AnimNotifyState_PlayLoopingSFX_Guide.md](/C:/development/W11_Team3_Engine/Docs/AnimNotifyState_PlayLoopingSFX_Guide.md)
- [AnimNotifyState_AttackWindow_Guide.md](/C:/development/W11_Team3_Engine/Docs/AnimNotifyState_AttackWindow_Guide.md)
- [AnimNotify_Log_Guide.md](/C:/development/W11_Team3_Engine/Docs/AnimNotify_Log_Guide.md)

## 21. 요약

처음에는 아래만 기억하면 됩니다.

1. 한 번 실행이면 `Notify`, 구간이면 `Notify State`
2. 실제 동작은 `Notify Class`가 결정한다
3. 세부 데이터는 `Payload`에 들어간다
4. 가능하면 구조화된 입력 UI를 사용한다
5. 저장 전 `Validation`을 반드시 확인한다
6. 첫 테스트는 `UAnimNotifyLog`, `UAnimNotifyStateLog`가 가장 쉽다
