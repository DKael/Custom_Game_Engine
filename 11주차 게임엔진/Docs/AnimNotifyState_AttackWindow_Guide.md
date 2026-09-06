# UAnimNotifyState_AttackWindow 사용 가이드

`UAnimNotifyState_AttackWindow`는 특정 구간 동안 공격 판정용 `PrimitiveComponent`의 overlap을 열고 닫는 클래스입니다.

## 언제 쓰나

- 무기 히트박스 on/off
- 짧은 공격 판정 구간
- 공격 ID 기반 판정 구분

## 필수 설정

- `Type = Notify State`
- `Notify Class = UAnimNotifyState_AttackWindow`

## Payload

- `ComponentName` 필수
- `AttackId` 선택
- `Priority` 선택

예:

```txt
ComponentName=WeaponHitbox;AttackId=Light1;Priority=1
```

## 동작 방식

- begin 시 `ComponentName`에 해당하는 primitive component를 찾음
- 해당 component의 overlap 생성을 켬
- `AttackId`가 있으면 `AttackId:<값>` 태그를 액터에 부여
- end 시 overlap과 태그를 정리

## 추천 예제

### 기본 라이트 공격

```txt
ComponentName=WeaponHitbox;AttackId=Light1;Priority=1
```

### 강공격 판정

```txt
ComponentName=WeaponHitbox;AttackId=Heavy1;Priority=2
```

## 주의

- `ComponentName`은 실제 프리뷰 액터에 존재하는 `PrimitiveComponent` 이름과 정확히 일치해야 합니다.
- `AttackId`는 선택 사항이지만, 공격 구분이 필요하면 꼭 넣는 것이 좋습니다.
- 이 클래스는 일반 `Notify`가 아니라 `Notify State`로 만들어야 합니다.
