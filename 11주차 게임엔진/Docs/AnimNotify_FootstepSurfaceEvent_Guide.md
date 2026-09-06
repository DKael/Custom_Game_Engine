# UAnimNotify_FootstepSurfaceEvent 사용 가이드

`UAnimNotify_FootstepSurfaceEvent`는 발 아래 표면을 추적해서 발소리용 게임플레이 이벤트를 보내는 클래스입니다.

이 클래스는 직접 사운드를 재생하는 용도보다, "표면 종류까지 포함한 발걸음 이벤트"를 런타임이나 스크립트에 넘기는 용도에 가깝습니다.

## 언제 쓰나

- 표면별 발소리 분기
- 발걸음 이벤트를 Lua 또는 게임플레이 로직으로 전달
- 걷기/달리기/재질별 분기 처리

## 필수 설정

- `Type = Notify`
- `Notify Class = UAnimNotify_FootstepSurfaceEvent`

## Payload

- `EventName` 필수
- `SocketName` 필수
- `TraceDistance` 선택
- `Payload` 선택

예:

```txt
EventName=Footstep;SocketName=foot_l;TraceDistance=25.0;Payload=Run
```

## 의미

- `EventName`: 보낼 이벤트 이름
- `SocketName`: 발 추적 시작 위치
- `TraceDistance`: 아래 방향 표면 탐색 거리
- `Payload`: 추가 문자열 정보

## 추천 예제

### 기본 걷기 발걸음 이벤트

```txt
EventName=Footstep;SocketName=foot_l;TraceDistance=25.0;Payload=Walk
```

### 달리기 발걸음 이벤트

```txt
EventName=Footstep;SocketName=foot_r;TraceDistance=35.0;Payload=Run
```

## `PlaySFX`와 차이

- `UAnimNotify_PlaySFX`: 사운드를 직접 재생
- `UAnimNotify_FootstepSurfaceEvent`: 표면 정보가 포함된 이벤트를 전달

즉, 단순 발소리 재생은 `PlaySFX`, 표면 분기까지 필요하면 `FootstepSurfaceEvent`가 더 적합합니다.

## 주의

- `SocketName`이 비어 있으면 동작하지 않습니다.
- 표면 탐지가 실패하면 기본 표면 처리로 넘어갈 수 있습니다.
