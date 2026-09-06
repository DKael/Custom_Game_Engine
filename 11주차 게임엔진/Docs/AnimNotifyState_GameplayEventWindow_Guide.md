# UAnimNotifyState_GameplayEventWindow 사용 가이드

`UAnimNotifyState_GameplayEventWindow`는 특정 구간의 시작과 끝에서 게임플레이 이벤트를 보내는 클래스입니다.

## 언제 쓰나

- 무적 구간
- 입력 허용 구간
- 방어 판정 구간
- 특정 상태 활성/비활성 구간

## 필수 설정

- `Type = Notify State`
- `Notify Class = UAnimNotifyState_GameplayEventWindow`

## Payload

- `EventName` 필수
- `EndEventName` 선택
- `Payload` 선택

예:

```txt
EventName=InvulnWindow;EndEventName=InvulnWindowEnd;Payload=Short
```

## 동작 방식

- 시작 시 `EventName` 전송
- 종료 시 `EndEventName` 전송
- `EndEventName`이 비어 있으면 종료 시에도 `EventName`을 재사용

## 추천 예제

### 무적 구간

```txt
EventName=Invuln.Begin;EndEventName=Invuln.End
```

### 콤보 입력 구간

```txt
EventName=Combo.Open;EndEventName=Combo.Close;Payload=Light1
```

## 팁

- begin/end를 서로 다른 이름으로 두면 로직이 읽기 쉬워집니다.
- 같은 이름을 재사용할 경우 수신 측에서 phase 분기를 잘 해야 합니다.
