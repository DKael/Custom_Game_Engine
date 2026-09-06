# UAnimNotify_GameplayEvent 사용 가이드

`UAnimNotify_GameplayEvent`는 애니메이션의 특정 순간에 게임플레이 이벤트를 1회 전송하는 클래스입니다.

## 언제 쓰나

- 콤보 입력 가능 시점 알림
- 상태 전환 신호
- 특정 프레임에서 스크립트 이벤트 호출

## 필수 설정

- `Type = Notify`
- `Notify Class = UAnimNotify_GameplayEvent`

## Payload

- `EventName` 필수
- `Payload` 선택

예:

```txt
EventName=Combo.Open;Payload=Light1
```

## 추천 사용 순서

1. 원하는 프레임에 `Notify` 추가
2. `Name`을 의미 있게 지정
3. `Notify Class`를 `UAnimNotify_GameplayEvent`로 변경
4. `EventName`을 먼저 넣기
5. 필요하면 `Payload` 추가
6. `Validation` 확인

## 예제

### 예제 1. 콤보 입력 허용

```txt
EventName=Combo.Open;Payload=Light1
```

### 예제 2. 회피 종료 체크

```txt
EventName=Evade.Recover
```

## 자주 하는 실수

- `EventName`을 비워 둠
- `Notify State`로 잘못 만듦
- 이벤트 이름 규칙을 팀 규칙과 다르게 작성
