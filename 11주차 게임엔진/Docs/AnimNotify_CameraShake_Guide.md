# UAnimNotify_CameraShake 사용 가이드

`UAnimNotify_CameraShake`는 특정 프레임에서 카메라 흔들림을 1회 실행하는 클래스입니다.

## 언제 쓰나

- 강한 타격 순간
- 착지 충격
- 보스 공격 연출

## 필수 설정

- `Type = Notify`
- `Notify Class = UAnimNotify_CameraShake`

## Payload

- `Shake` 필수
- `Scale` 선택

예:

```txt
Shake=HeavyHit;Scale=0.75
```

## 사용 순서

1. 임팩트 프레임에 `Notify` 추가
2. `Name = CameraShake_HeavyHit`처럼 지정
3. `Notify Class`를 `UAnimNotify_CameraShake`로 변경
4. `Shake` 이름 입력
5. 필요하면 `Scale` 조정

## 추천 예제

### 강한 타격

```txt
Shake=HeavyHit;Scale=1.0
```

### 가벼운 반동

```txt
Shake=LightImpact;Scale=0.35
```

## 주의

- `Shake`가 비어 있으면 동작하지 않습니다.
- 흔들림은 보통 너무 자주 넣으면 피로감이 커집니다.
