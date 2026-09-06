# UAnimNotifyState_PlayLoopingSFX 사용 가이드

`UAnimNotifyState_PlayLoopingSFX`는 특정 구간 동안 루프 사운드를 재생하는 클래스입니다.

## 언제 쓰나

- 차징 사운드
- 무기 지속 이펙트 사운드
- 회전/채널링 같은 유지형 효과음

## 필수 설정

- `Type = Notify State`
- `Notify Class = UAnimNotifyState_PlayLoopingSFX`

## Payload

- `SoundCue` 필수
- `SocketName` 선택
- `VolumeMultiplier` 선택
- `Spatialized` 선택

예:

```txt
SoundCue=SwordHum;SocketName=weapon_tip;VolumeMultiplier=1.0;Spatialized=true
```

## 동작

- begin 시 루프 사운드 시작
- state 유지 중 필요하면 위치 갱신
- end 시 사운드 정지

## 추천 예제

### 검 차징 사운드

```txt
SoundCue=SwordHum;SocketName=weapon_tip;VolumeMultiplier=1.0;Spatialized=true
```

### 마법 시전 유지 사운드

```txt
SoundCue=MagicLoop;VolumeMultiplier=0.8;Spatialized=false
```

## 주의

- 일반 `Notify`가 아니라 `Notify State`여야 합니다.
- 길이가 0이면 구간형 의미가 사라집니다.
