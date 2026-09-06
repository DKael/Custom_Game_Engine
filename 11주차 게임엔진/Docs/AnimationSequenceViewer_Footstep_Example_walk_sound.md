# walk_sound.mp3로 걷기 발자국 소리 넣기 예제

이 문서는 `JSEngine/Asset/Sound/walk_sound.mp3` 파일을 사용해서, 캐릭터가 걷기 애니메이션을 재생할 때 발자국 소리가 나게 만드는 가장 쉬운 예제를 설명합니다.

이번 예제는 가장 단순한 방식인 `UAnimNotify_PlaySFX`를 사용합니다.

## 1. 핵심 설정 먼저 보기

현재 파일 위치:

```txt
Asset/Sound/walk_sound.mp3
```

따라서 `Payload`는 아래처럼 넣는 것이 가장 안전합니다.

```txt
SoundCue=Asset/Sound/walk_sound.mp3;SocketName=foot_l;VolumeMultiplier=1.0;Spatialized=true
```

중요:

- 오디오 시스템은 단순 키 이름도 받을 수 있지만, 현재 파일은 `Asset/Sound/`에 있습니다.
- 따라서 `walk_sound`처럼 축약하지 말고 `Asset/Sound/walk_sound.mp3`처럼 경로를 정확히 쓰는 것을 권장합니다.

## 2. 이번 예제에서 만드는 것

걷기 애니메이션 안에 발이 닿는 타이밍마다 `Notify`를 하나씩 넣습니다.

즉:

- 왼발 접지 순간 `Notify` 1개
- 오른발 접지 순간 `Notify` 1개

장점:

- 가장 단순합니다.
- 파일 경로와 재생 여부를 바로 확인할 수 있습니다.
- 이후 `FootstepSurfaceEvent`로 확장하기 쉽습니다.

## 3. 사용할 Notify 클래스

```txt
UAnimNotify_PlaySFX
```

이 클래스는 선택한 시점에 효과음을 1회 재생합니다.

## 4. 작업 전 준비

필요한 것:

- 걷기 애니메이션 시퀀스 1개
- 발 소켓이 있는 프리뷰 메시가 있으면 더 좋음
- 예: `foot_l`, `foot_r`

소켓이 없어도 테스트는 가능합니다.

## 5. 전체 작업 순서

1. 걷기 애니메이션을 `Animation Sequence Viewer`에서 연다.
2. `Notifies` 섹션을 펼친다.
3. 필요하면 `Notify Track`를 만든다.
4. 왼발 접지 타이밍에 `Notify`를 추가한다.
5. 오른발 접지 타이밍에 `Notify`를 추가한다.
6. 두 `Notify`의 `Notify Class`를 `UAnimNotify_PlaySFX`로 바꾼다.
7. `Payload`에 `walk_sound.mp3` 경로를 넣는다.
8. 재생해서 타이밍을 확인한다.
9. `Validation`을 확인하고 저장한다.

## 6. Notify Track 준비

이미 트랙이 있으면 그대로 써도 됩니다.

없다면:

1. `Notifies` 헤더 우클릭
2. `Add Track`

처음에는 `Track 1` 하나면 충분합니다.

## 7. 발자국 Notify 2개 추가

## 7.1 왼발 타이밍 찾기

재생하면서 왼발이 바닥에 닿는 프레임을 찾습니다.

보통 아래 기준으로 잡으면 됩니다.

- 발이 가장 아래로 내려온 순간
- 몸의 무게가 실리는 순간
- 접지 직전이나 너무 늦은 시점이 아닌 실제 접지 순간

## 7.2 왼발 Notify 추가

방법 1:

- 해당 위치를 더블 클릭

방법 2:

- 해당 위치 우클릭
- `Add Notify Here`

추가되면 마커를 선택합니다.

## 7.3 오른발 Notify 추가

오른발도 같은 방식으로 추가합니다.

## 8. 왼발 Notify 설정

### 8.1 Basic

- `Name = Footstep_L`
- `Type = Notify`
- `Notify Class = UAnimNotify_PlaySFX`

### 8.2 Timing

- `Time`: 왼발 접지 시점으로 조정
- `Duration = 0`

### 8.3 Payload

```txt
SoundCue=Asset/Sound/walk_sound.mp3;SocketName=foot_l;VolumeMultiplier=1.0;Spatialized=true
```

### 8.4 Visual

원하면 색을 바꿔서 왼발 마커를 구분합니다.

## 9. 오른발 Notify 설정

### 9.1 Basic

- `Name = Footstep_R`
- `Type = Notify`
- `Notify Class = UAnimNotify_PlaySFX`

### 9.2 Timing

- `Time`: 오른발 접지 시점
- `Duration = 0`

### 9.3 Payload

오른발 소켓이 있다면:

```txt
SoundCue=Asset/Sound/walk_sound.mp3;SocketName=foot_r;VolumeMultiplier=1.0;Spatialized=true
```

소켓 이름이 확실하지 않으면 먼저 아래처럼 테스트해도 됩니다.

```txt
SoundCue=Asset/Sound/walk_sound.mp3;VolumeMultiplier=1.0;Spatialized=false
```

## 10. 가장 추천하는 테스트 순서

### 10.1 1차 테스트: 소리만 먼저 확인

양쪽 모두 아래처럼 넣습니다.

```txt
SoundCue=Asset/Sound/walk_sound.mp3;VolumeMultiplier=1.0;Spatialized=false
```

이 상태에서 소리가 나면:

- 파일 경로는 맞음
- 클래스 설정은 맞음
- 타이밍은 대체로 맞음

그 다음에 소켓과 3D 위치감을 추가하면 됩니다.

### 10.2 2차 테스트: 위치감 추가

왼발:

```txt
SoundCue=Asset/Sound/walk_sound.mp3;SocketName=foot_l;VolumeMultiplier=1.0;Spatialized=true
```

오른발:

```txt
SoundCue=Asset/Sound/walk_sound.mp3;SocketName=foot_r;VolumeMultiplier=1.0;Spatialized=true
```

## 11. Payload 항목 의미

### 11.1 `SoundCue`

재생할 사운드 파일 경로 또는 키입니다.

이번 예제에서는 아래 값을 사용합니다.

```txt
Asset/Sound/walk_sound.mp3
```

### 11.2 `SocketName`

소리가 시작될 위치입니다.

예:

- `foot_l`
- `foot_r`

### 11.3 `VolumeMultiplier`

볼륨 배수입니다.

예:

- `1.0`: 기본
- `0.8`: 조금 작게
- `1.2`: 조금 크게

### 11.4 `Spatialized`

3D 위치 기반 재생 여부입니다.

- `true`: 발 위치에서 소리가 나는 느낌
- `false`: 단순 2D 재생

## 12. 소리가 안 날 때 체크리스트

### 12.1 `Notify Class`가 맞는가

반드시:

```txt
UAnimNotify_PlaySFX
```

### 12.2 `Type = Notify`인가

이번 예제는 `Notify State`가 아닙니다.

### 12.3 `SoundCue` 경로가 정확한가

정답:

```txt
Asset/Sound/walk_sound.mp3
```

축약 형태는 현재 구조에서 실패할 수 있습니다.

예:

- `walk_sound`
- `walk_sound.mp3`
- `Sound/walk_sound.mp3`

### 12.4 `Validation`에 오류가 없는가

- `Selected Notify > Validation`
- `Notify Debug > Validation`

둘 중 아무 곳에서나 확인할 수 있습니다.

### 12.5 소켓 이름이 실제와 같은가

`Spatialized=true`이고 `SocketName`을 쓰는 경우:

- `foot_l`
- `foot_r`

이 이름이 실제 프리뷰 메시에 있어야 합니다.

확실하지 않다면 먼저 `Spatialized=false`로 검증하세요.

## 13. 최종 예제 설정

### 13.1 왼발

- `Name: Footstep_L`
- `Type: Notify`
- `Notify Class: UAnimNotify_PlaySFX`
- `Payload`:

```txt
SoundCue=Asset/Sound/walk_sound.mp3;SocketName=foot_l;VolumeMultiplier=1.0;Spatialized=true
```

### 13.2 오른발

- `Name: Footstep_R`
- `Type: Notify`
- `Notify Class: UAnimNotify_PlaySFX`
- `Payload`:

```txt
SoundCue=Asset/Sound/walk_sound.mp3;SocketName=foot_r;VolumeMultiplier=1.0;Spatialized=true
```

## 14. 다음 단계로 확장하고 싶다면

이번 문서는 "한 파일을 바로 걷기 발소리로 재생하는 가장 쉬운 방법" 기준입니다.

추후 확장 예:

- 표면별 발소리 분기: `UAnimNotify_FootstepSurfaceEvent`
- 걷기/달리기별 다른 소리
- 왼발/오른발마다 다른 볼륨
- 랜덤 샘플 재생 로직

## 15. 관련 문서

- [AnimationSequenceViewer_AnimNotify_Guide.md](/C:/development/W11_Team3_Engine/Docs/AnimationSequenceViewer_AnimNotify_Guide.md)
- [AnimNotify_FootstepSurfaceEvent_Guide.md](/C:/development/W11_Team3_Engine/Docs/AnimNotify_FootstepSurfaceEvent_Guide.md)

## 16. 한 줄 요약

이번 예제에서 가장 중요한 값은 아래입니다.

```txt
SoundCue=Asset/Sound/walk_sound.mp3
```

가장 쉬운 시작 설정은 아래입니다.

```txt
SoundCue=Asset/Sound/walk_sound.mp3;VolumeMultiplier=1.0;Spatialized=false
```
