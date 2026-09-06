# UAnimNotifyLog / UAnimNotifyStateLog 사용 가이드

이 문서는 테스트용 로그 클래스 2개를 설명합니다.

- `UAnimNotifyLog`
- `UAnimNotifyStateLog`

이 두 클래스는 실제 게임 효과를 만드는 용도보다는, `Notify`가 정상적으로 실행되는지 빠르게 확인하는 용도입니다.

## 1. 언제 쓰나

- 첫 연결 테스트
- 타이밍이 맞는지 검증
- begin / tick / end 호출 확인
- payload 전달 확인

## 2. `UAnimNotifyLog`

### 설정

- `Type = Notify`
- `Notify Class = UAnimNotifyLog`

### 특징

- 실행 시 로그를 1회 남깁니다.
- 이름, 애니메이션, 컴포넌트, 시간, payload가 같이 기록됩니다.

### 추천 예제

```txt
Payload=Hello
```

## 3. `UAnimNotifyStateLog`

### 설정

- `Type = Notify State`
- `Notify Class = UAnimNotifyStateLog`

### 특징

- `Begin`, `Tick`, `End` 시점 로그를 남깁니다.
- 구간형 이벤트가 실제로 얼마나 유지되는지 확인하기 좋습니다.

## 4. 추천 사용 순서

1. 원하는 위치에 `Notify` 또는 `Notify State` 생성
2. 클래스 지정
3. 필요하면 짧은 payload 입력
4. 재생
5. 로그 / `Recent Fired` 확인

## 5. 언제 다른 클래스 대신 이걸 써야 하나

- 아직 실제 사운드/VFX/공격 판정 연결 전
- "이 타이밍에 진짜 호출되는가"만 먼저 확인하고 싶을 때

즉, 가장 좋은 첫 디버그 도구입니다.
