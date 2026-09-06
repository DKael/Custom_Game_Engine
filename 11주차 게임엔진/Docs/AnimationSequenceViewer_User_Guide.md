# Animation Sequence Viewer 사용 가이드

이 문서는 `Animation Sequence Viewer`를 처음 열어보는 사람을 위한 전체 사용 설명서입니다.

목표는 다음과 같습니다.

1. 화면이 어떻게 구성되어 있는지 이해한다.
2. 애니메이션을 재생, 정지, 탐색할 수 있다.
3. `Notify`, `Curve`, `Attribute`를 보고 편집할 수 있다.
4. 검증 패널을 이용해 문제를 찾을 수 있다.

이 문서는 코드 기준으로 실제 UI 동작에 맞춰 작성했습니다.

## 1. Animation Sequence Viewer가 무엇인가

`Animation Sequence Viewer`는 애니메이션 시퀀스를 열어서 아래 작업을 할 수 있는 에디터입니다.

- 프리뷰 메시로 애니메이션 재생
- 현재 시간 이동
- 재생 속도 변경
- 루프 on/off
- `Notify` 추가, 삭제, 이동, 복제
- `Curve`와 `Attribute` 확인
- 선택한 `Notify`의 세부 데이터 수정
- `Validation`, `Recent Fired`, `Event Log` 확인

즉, "애니메이션 데이터를 보고, 타이밍을 조정하고, 이벤트를 심는 작업"을 위한 전용 뷰어라고 보면 됩니다.

## 2. 화면 전체 구조

처음 열면 화면은 크게 3개 영역으로 나뉩니다.

### 2.1 상단: Preview 영역

실제로 애니메이션이 재생되는 3D 프리뷰 영역입니다.

포함 요소:

- `Skeleton Tree`
- 프리뷰 툴바
- 3D 뷰포트
- 시퀀스 정보 오버레이

### 2.2 중단: Sequencer 영역

타임라인과 트랙이 보이는 편집 영역입니다.

포함 요소:

- 트랙 아웃라이너
- `Notifies`
- `Curves`
- `Attributes`
- 타임라인 캔버스

### 2.3 하단: Details / Debug 영역

선택한 `Notify`를 편집하고 상태를 점검하는 영역입니다.

왼쪽:

- `Selected Notify`
- `Selected Curve`

오른쪽:

- `Notify Debug`
  - `Validation`
  - `Recent Fired`
  - `Event Log`

## 3. 처음 열었을 때 먼저 봐야 할 것

프리뷰 상단 오버레이에는 현재 시퀀스의 요약 정보가 나옵니다.

예:

- 시퀀스 이름
- 프레임 수
- 키 수
- 본 트랙 수
- 커브 수
- Notify 수
- FPS

처음 파일을 열면 아래 순서로 확인하는 것이 좋습니다.

1. 프리뷰가 정상적으로 보이는가
2. 애니메이션이 재생되는가
3. `Notifies`가 몇 개인가
4. `Curves`가 있는가
5. 하단 패널에 오류 메시지가 없는가

## 4. Preview 영역 사용법

## 4.1 Preview 화면에서 보이는 것

프리뷰에는 현재 애니메이션이 3D 메시 위에서 재생됩니다.

오버레이에는 대략 이런 상태가 표시됩니다.

- `Playing` 또는 `Paused`
- 현재 시간
- 현재 재생 속도

## 4.2 Skeleton Tree

프리뷰 왼쪽에는 `Skeleton Tree`가 있습니다.

할 수 있는 일:

- 본 계층 펼치기/접기
- 특정 본 선택
- 선택 본 이름 확인

본을 클릭하면:

- 해당 본이 선택됩니다.
- 프리뷰에서 본 표시가 켜질 수 있습니다.

본 우클릭 메뉴:

- `Expand Children`
- `Collapse Children`

이 기능은 리그 구조를 빠르게 확인할 때 유용합니다.

## 4.3 Preview 툴바

프리뷰 툴바에서는 뷰포트 표현 방식을 바꿀 수 있습니다.

대표 메뉴:

- `Viewport Type`
- `Cam Speed`
- `Show`

### 4.3.1 Viewport Type

카메라 뷰 형태를 바꿉니다.

예:

- `Perspective`
- `Top`
- `Bottom`
- `Front`
- `Back`
- `Left`
- `Right`

보통은 `Perspective`를 가장 많이 씁니다.

### 4.3.2 Cam Speed

카메라 이동 속도를 조절합니다.

프리뷰를 세밀하게 다뤄야 하면 낮추고, 넓게 보려면 높이면 됩니다.

### 4.3.3 Show

현재 코드 기준으로 대표적으로 `Bones` 표시를 켜고 끌 수 있습니다.

## 5. 재생 컨트롤 사용법

재생 컨트롤은 가운데 영역 아래쪽 바에서 사용합니다.

버튼 종류:

- 처음으로 이동
- 이전 프레임
- 역재생 / 일시정지
- 재생 / 일시정지
- 다음 프레임
- 끝으로 이동
- 루프 on/off
- 재생 속도 변경

## 5.1 주요 버튼 의미

### 5.1.1 Jump to Start

현재 시간을 시작 지점으로 이동합니다.

### 5.1.2 Previous Frame / Next Frame

한 프레임씩 이동합니다.

타이밍 작업할 때 매우 중요합니다.

### 5.1.3 Play / Pause

정방향 재생과 일시정지를 제어합니다.

### 5.1.4 Reverse Play

역방향으로 재생합니다.

### 5.1.5 Loop

루프 재생 여부를 켜고 끕니다.

루프 애니메이션 편집 시 거의 항상 켜두는 편이 편합니다.

## 5.2 Playback Speed

속도 버튼을 누르면 재생 속도 팝업이 열립니다.

기본 프리셋 예:

- `0.1x`
- `0.25x`
- `0.5x`
- `0.75x`
- `1.0x`
- `2.0x`
- `5.0x`
- `10.0x`

추가 기능:

- 직접 속도 입력
- `Snap to Frames` 체크

### 5.2.1 Snap to Frames

시간 이동과 `Notify` 조정 시 프레임 단위로 맞추고 싶을 때 켭니다.

정밀한 애니메이션 타이밍 작업에서는 보통 켜 두는 것이 좋습니다.

## 6. 타임라인 기본 개념

중앙 타임라인은 현재 보이는 시간 구간만 확대해서 보여 줍니다.

이 영역에서 볼 수 있는 것:

- 프레임 그리드
- 현재 재생 헤드
- `Notify` 마커
- `Curve` 그래프

## 6.1 Playhead

주황색 선과 삼각형 표시가 현재 시간입니다.

이 선이 지나가는 위치가 현재 재생 위치입니다.

## 6.2 Ruler

상단 눈금 영역에는 프레임 번호가 표시됩니다.

여기서 할 수 있는 것:

- 클릭 드래그로 현재 시간 이동

즉, 자(ruler)를 왼쪽 버튼으로 드래그하면 플레이헤드를 직접 움직일 수 있습니다.

## 6.3 Pan

타임라인 위에서 마우스 가운데 버튼 드래그로 좌우 이동할 수 있습니다.

이 기능은 확대된 상태에서 특히 유용합니다.

## 6.4 표시 구간 조절

하단 오른쪽 영역에는 시간 범위 조절 바가 있습니다.

여기에는 6개의 기준값이 표시됩니다.

왼쪽:

- `View start`
- `Playback start`
- `Sequence start`

오른쪽:

- `Sequence end`
- `Playback end`
- `View end`

이 값들은 클릭해서 직접 수정할 수 있습니다.

또 가운데 막대를 드래그해서 보이는 시간 범위를 조절할 수 있습니다.

## 7. Track Outliner 사용법

타임라인 왼쪽에는 트랙 목록이 있습니다.

여기에는 보통 아래 섹션이 보입니다.

- `Notifies`
- `Curves`
- `Additive Layer Tracks`
- `Attributes`

## 7.1 Filter tracks and curves

상단 필터 입력창에 텍스트를 넣으면 트랙과 커브를 검색할 수 있습니다.

예:

- `foot`
- `attack`
- `curve`

트랙 수가 많을 때 매우 유용합니다.

## 7.2 섹션 펼치기 / 접기

각 헤더는 클릭해서 펼치거나 접을 수 있습니다.

예:

- `Notifies`
- `Curves`
- `Attributes`

## 8. Notifies 사용법

`Notifies`는 애니메이션 특정 시간에 이벤트를 심는 기능입니다.

처음 사용하는 경우에는 아래 흐름으로 이해하면 됩니다.

1. 트랙을 만든다
2. 타임라인에서 마커를 추가한다
3. 마커를 선택한다
4. 하단 `Selected Notify`에서 내용을 수정한다

## 8.1 Notify Track 추가

`Notifies` 헤더 우클릭:

- `Add Track`

추가하면 기본 이름은 `Track 1`, `Track 2`처럼 생성됩니다.

## 8.2 Notify 추가

방법은 3가지입니다.

### 8.2.1 더블 클릭

`Notify Track`의 빈 공간을 더블 클릭하면 현재 위치에 `Notify`가 추가됩니다.

### 8.2.2 우클릭 메뉴

트랙 빈 공간 우클릭:

- `Add Notify Here`
- `Paste Notify Here`

### 8.2.3 하단 버튼

`Selected Notify` 툴바의 `Add Notify` 버튼으로 현재 시간에 추가할 수 있습니다.

## 8.3 Notify 선택

마커를 클릭하면 선택됩니다.

선택되면 아래 `Selected Notify` 패널에 세부 정보가 표시됩니다.

## 8.4 Notify 이동

마커를 드래그하면 시간 위치를 옮길 수 있습니다.

`Notify State`라면 오른쪽 끝을 드래그해서 길이도 조절할 수 있습니다.

## 8.5 Notify 우클릭 메뉴

마커 우클릭:

- `Duplicate`
- `Copy`
- `Delete`
- `Move To Track`

## 8.6 Track 우클릭 메뉴

트랙 우클릭:

- `Rename Track`
- `Paste Notify Here`
- `Move Selected Notify Here`
- `Move Up`
- `Move Down`
- `Delete Empty Track`

## 9. Selected Notify 패널 사용법

하단 왼쪽의 `Selected Notify`는 선택한 `Notify`를 편집하는 영역입니다.

아무것도 선택하지 않으면 안내 문구가 표시됩니다.

구성:

- 상단 툴바
- `Basic`
- `Timing`
- `Payload`
- `Visual`
- `Validation`

## 9.1 상단 툴바

버튼:

- `Add Notify`
- `Duplicate`
- `Copy`
- `Paste`
- `Delete Selected`

## 9.2 Basic

편집 가능 항목:

- `Name`
- `Type`
- `Notify Class`

`Type`은 두 가지입니다.

- `Notify`
- `Notify State`

## 9.3 Timing

편집 가능 항목:

- `Track`
- `Time`
- `Duration`

의미:

- `Time`: 시작 시점
- `Duration`: 길이

## 9.4 Payload

선택한 `Notify Class`에 따라 구조화된 입력 UI가 나오거나, raw 문자열 입력 UI가 나옵니다.

즉:

- 스키마가 있는 클래스: 필드별 편집기
- 스키마가 없는 클래스: 직접 문자열 입력

## 9.5 Visual

마커 색을 바꿀 수 있습니다.

색으로 역할을 분류하면 관리가 쉬워집니다.

## 9.6 Validation

현재 선택한 `Notify`의 문제를 즉시 보여 줍니다.

예:

- 필수 payload 누락
- 타입과 클래스 불일치
- 이름 누락
- duration 이상

## 10. Notify Debug 패널 사용법

하단 오른쪽 `Notify Debug`는 디버그와 검증 전용 영역입니다.

탭:

- `Validation`
- `Recent Fired`
- `Event Log`

## 10.1 Validation 탭

문서 전체의 `Notify` 문제를 보여 줍니다.

상단에서 다음 개수를 볼 수 있습니다.

- `Error`
- `Warning`
- `Info`

필터 버튼:

- `All`
- `Errors`
- `Warnings`
- `Info`

리스트에서 항목을 클릭하면 해당 `Notify`를 바로 선택할 수 있습니다.

## 10.2 Recent Fired 탭

최근 실행된 `Notify` 요약을 확인하는 용도입니다.

재생 중 어떤 `Notify`가 실제로 발생했는지 추적할 때 유용합니다.

## 10.3 Event Log 탭

실행 기록을 더 자세히 볼 때 사용합니다.

처음 디버깅할 때는 `Validation`과 `Recent Fired`를 먼저 보는 것을 권장합니다.

## 10.4 어떤 탭을 먼저 보면 좋은가

처음 사용하는 사람에게는 아래 순서를 권장합니다.

1. `Validation`: 설정 오류가 있는지 먼저 확인
2. `Recent Fired`: 재생했을 때 `Notify`가 실제로 호출되는지 확인
3. `Event Log`: 더 자세한 실행 흐름이 필요할 때 확인

## 11. Curves 사용법

시퀀스에 float curve가 있으면 `Curves` 섹션에서 볼 수 있습니다.

그룹 예:

- `Morph Target Curves`
- `Material Curves`
- `Attribute Curves`
- `Other Curves`

## 11.1 Curve 표시 / 숨기기

그룹 행을 클릭하면 해당 타입의 커브 가시성을 토글할 수 있습니다.

즉, 특정 종류만 보거나 숨길 수 있습니다.

## 11.2 Curve 선택

아웃라이너나 그래프 행에서 커브를 선택하면 하단 `Selected Curve` 패널에 정보가 나옵니다.

## 11.3 Selected Curve 패널

선택된 커브가 있으면 다음 정보를 볼 수 있습니다.

- 커브 이름
- 타입
- source kind
- 키 개수
- 현재 시간의 값
- 전체 시간 범위
- 첫 키 / 마지막 키
- 값 범위
- 현재 보이는 구간의 값 범위
- 현재 시간에서 가장 가까운 키
- 마우스 hover 샘플 값

## 11.4 Selected Curve 버튼

### 11.4.1 Frame Selection

선택한 커브 전체가 보이도록 시간 범위를 맞춰 줍니다.

### 11.4.2 Solo Type

현재 커브 타입만 보이게 합니다.

### 11.4.3 Show All

모든 커브 타입을 다시 표시합니다.

## 12. Attributes 섹션

`Attributes`는 attribute curve를 별도로 모아서 보는 섹션입니다.

`Curves`와 비슷한 방식으로 펼치고 접을 수 있습니다.

## 13. 저장과 편집 상태

편집 후 변경사항이 있으면 문서 탭 라벨에 `*`가 붙습니다.

저장 가능 조건:

- 시퀀스가 유효해야 함
- 변경사항이 있어야 함

저장하면 `Notify Validation` 결과에 따라 상태 메시지가 남을 수 있습니다.

예:

- 문제 없이 저장
- 경고가 있지만 저장

## 13.1 저장 전에 꼭 보면 좋은 것

저장 직전에는 아래만 확인해도 대부분의 실수를 줄일 수 있습니다.

- 현재 선택한 `Notify`에 `Validation Error`가 없는가
- `Notify State`인데 `Duration`이 0으로 남아 있지 않은가
- `SocketName` 또는 `ComponentName`이 실제 프리뷰 대상과 맞는가
- `Notify Class`가 의도한 타입과 맞는가

## 14. 자주 쓰는 단축키

현재 코드 기준 주요 단축키:

- `Ctrl+S`: 저장
- `Delete`: 선택한 `Notify` 삭제
- `Ctrl+D`: 선택한 `Notify` 복제
- `Ctrl+C`: 선택한 `Notify` 복사
- `Ctrl+V`: 복사한 `Notify` 붙여넣기

주의:

- 이 문맥에서 편집 대상은 주로 `Notify`입니다.
- 커브 자체를 직접 수정하는 에디터는 아닙니다.

## 15. 처음 쓰는 사람을 위한 추천 사용 순서

처음 열었을 때는 아래 순서대로 익히는 것이 좋습니다.

1. 애니메이션을 열고 프리뷰가 정상인지 확인한다.
2. `Play`, `Pause`, `Next Frame`만 먼저 써본다.
3. 타임라인 자를 드래그해서 현재 시간을 옮겨본다.
4. `Notifies`를 펼치고 트랙을 하나 만든다.
5. 빈 공간 더블 클릭으로 `Notify` 하나를 만든다.
6. `Selected Notify`에서 `Name`, `Type`, `Class`를 바꿔본다.
7. `Validation` 탭에서 메시지가 어떻게 바뀌는지 본다.
8. `Curves`를 선택해서 `Selected Curve` 패널도 확인해 본다.
9. `Ctrl+S`로 저장한다.

## 16. 추천 실습 예제

처음에는 아래 실습이 가장 쉽습니다.

### 실습 1. 테스트 Notify 만들기

1. `Notify Track` 추가
2. 중간 지점에 `Notify` 추가
3. `Name = Test_Notify`
4. `Notify Class = UAnimNotifyLog`
5. 재생 후 `Recent Fired` 또는 로그 확인

### 실습 2. Curve 보기

1. `Curves` 섹션 펼치기
2. 아무 커브나 선택
3. `Frame Selection`
4. 재생하면서 `Current Value`가 바뀌는지 확인

### 실습 3. Playback Range 조절

1. 하단 오른쪽 범위 바를 드래그
2. 재생 시작/끝 지점을 줄여 본다
3. 루프 재생으로 짧은 구간 반복 확인

## 17. 자주 헷갈리는 점

### 17.1 Visible Range와 Playback Range는 다르다

- `Visible Range`: 화면에 보이는 구간
- `Playback Range`: 실제 재생이 반복되는 구간

둘은 같을 수도 있고 다를 수도 있습니다.

### 17.2 Notify를 선택해야 아래 패널이 채워진다

`Selected Notify`는 아무것도 선택하지 않으면 비어 있는 것이 정상입니다.

### 17.3 Curve는 보기 기능 중심이다

현재 Viewer는 커브 값을 상세히 관찰하는 기능은 강하지만, 일반적인 키 편집기처럼 직접 키를 찍는 구조는 아닙니다.

### 17.4 빈 트랙만 삭제 가능하다

`Notify Track` 안에 마커가 남아 있으면 바로 삭제되지 않습니다.

## 18. 문제 해결 팁

### 18.1 Preview mesh is unavailable

프리뷰 메쉬를 찾지 못한 상태입니다.

확인할 것:

- 시퀀스가 정상 로드되었는가
- 프리뷰 초기화가 되었는가

### 18.2 Timeline data is unavailable

타임라인 데이터가 준비되지 않은 상태입니다.

보통 시퀀스 또는 프리뷰 초기화 문제를 먼저 확인합니다.

### 18.3 No float curves in this sequence

그 시퀀스에는 float curve가 없는 것입니다.

오류가 아니라 데이터 특성일 수 있습니다.

### 18.4 Validation 오류가 많다

우선순위:

1. `Error`부터 해결
2. 그다음 `Warning`
3. 마지막으로 `Info`

## 19. 관련 문서

이 문서와 함께 보면 좋은 문서:

- [AnimationSequenceViewer_AnimNotify_Guide.md](/C:/development/W11_Team3_Engine/Docs/AnimationSequenceViewer_AnimNotify_Guide.md)
- [AnimationSequenceViewer_Footstep_Example_walk_sound.md](/C:/development/W11_Team3_Engine/Docs/AnimationSequenceViewer_Footstep_Example_walk_sound.md)
- [AnimNotify_GameplayEvent_Guide.md](/C:/development/W11_Team3_Engine/Docs/AnimNotify_GameplayEvent_Guide.md)
- [AnimNotify_CameraShake_Guide.md](/C:/development/W11_Team3_Engine/Docs/AnimNotify_CameraShake_Guide.md)
- [AnimNotify_PlayVFX_Guide.md](/C:/development/W11_Team3_Engine/Docs/AnimNotify_PlayVFX_Guide.md)
- [AnimNotify_SpawnDecal_Guide.md](/C:/development/W11_Team3_Engine/Docs/AnimNotify_SpawnDecal_Guide.md)
- [AnimNotify_FootstepSurfaceEvent_Guide.md](/C:/development/W11_Team3_Engine/Docs/AnimNotify_FootstepSurfaceEvent_Guide.md)
- [AnimNotifyState_GameplayEventWindow_Guide.md](/C:/development/W11_Team3_Engine/Docs/AnimNotifyState_GameplayEventWindow_Guide.md)
- [AnimNotifyState_PlayLoopingSFX_Guide.md](/C:/development/W11_Team3_Engine/Docs/AnimNotifyState_PlayLoopingSFX_Guide.md)
- [AnimNotifyState_AttackWindow_Guide.md](/C:/development/W11_Team3_Engine/Docs/AnimNotifyState_AttackWindow_Guide.md)
- [AnimNotify_Log_Guide.md](/C:/development/W11_Team3_Engine/Docs/AnimNotify_Log_Guide.md)

## 20. 요약

처음에는 아래만 기억하면 됩니다.

1. 위는 프리뷰, 가운데는 타임라인, 아래는 상세/디버그다.
2. 재생은 아래 바에서, 현재 시간 이동은 ruler 드래그로 한다.
3. `Notify`는 `Notifies` 트랙에서 추가하고 아래 `Selected Notify`에서 수정한다.
4. `Curve`는 `Curves`에서 선택하고 아래 `Selected Curve`에서 읽는다.
5. 문제 확인은 오른쪽 `Notify Debug > Validation`에서 한다.
6. 저장은 `Ctrl+S`다.

이 문서 기준으로 익히면 `Animation Sequence Viewer`의 기본 탐색, 재생, `Notify` 작업, 커브 확인, 검증 흐름까지 혼자 사용할 수 있습니다.
