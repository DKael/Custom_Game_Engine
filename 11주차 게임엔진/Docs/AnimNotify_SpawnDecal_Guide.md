# UAnimNotify_SpawnDecal 사용 가이드

`UAnimNotify_SpawnDecal`은 특정 순간에 바닥 또는 충돌 표면에 데칼을 생성하는 클래스입니다.

## 언제 쓰나

- 발자국 자국
- 착지 흔적
- 무기 충돌 자국

## 필수 설정

- `Type = Notify`
- `Notify Class = UAnimNotify_SpawnDecal`

## Payload

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

## 의미

- `Decal`: 데칼 키
- `SocketName`: 시작 위치 소켓
- `TraceDistance`: 아래 방향 표면 탐색 거리
- `Size`: 크기 배수
- `Lifetime`: 유지 시간
- `AlignToHitNormal`: 표면 방향 정렬 여부

## 추천 예제

### 발자국 먼지

```txt
Decal=FootDust;SocketName=foot_l;TraceDistance=50.0;Size=1.0;Lifetime=2.0;AlignToHitNormal=true
```

### 강한 착지 자국

```txt
Decal=LandCrack;TraceDistance=80.0;Size=1.4;Lifetime=3.0;AlignToHitNormal=true
```

## 주의

- `Decal`이 없으면 동작하지 않습니다.
- `TraceDistance`가 너무 짧으면 바닥을 못 찾을 수 있습니다.
