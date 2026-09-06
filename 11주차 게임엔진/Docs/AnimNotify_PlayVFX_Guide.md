# UAnimNotify_PlayVFX 사용 가이드

`UAnimNotify_PlayVFX`는 특정 시점에 VFX를 재생하는 클래스입니다.

## 언제 쓰나

- 검 궤적
- 손/발 이펙트
- 착지 먼지
- 피격 스파크

## 필수 설정

- `Type = Notify`
- `Notify Class = UAnimNotify_PlayVFX`

## Payload

- `Effect` 필수
- `SocketName` 선택
- `Attached` 선택
- `Scale` 선택

예:

```txt
Effect=SlashArc;SocketName=weapon_tip;Attached=true;Scale=1.0
```

## 의미

- `Effect`: 재생할 VFX 키
- `SocketName`: 이펙트를 붙일 소켓
- `Attached`: 소켓에 계속 붙어 있을지 여부
- `Scale`: 이펙트 크기 배수

## 추천 예제

### 검 궤적

```txt
Effect=SlashArc;SocketName=weapon_tip;Attached=true;Scale=1.0
```

### 착지 먼지

```txt
Effect=LandDust;SocketName=foot_l;Attached=false;Scale=0.9
```

## 팁

- 소켓이 명확한 무기 이펙트는 `Attached=true`가 자연스럽습니다.
- 한 번 터지는 이펙트는 `Attached=false`가 더 일반적입니다.
