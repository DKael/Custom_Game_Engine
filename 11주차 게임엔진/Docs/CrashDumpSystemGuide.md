# JSEngine Crash Dump System Guide

작성일: 2026-05-21  
대상: JSEngine 크래시 덤프 시스템을 사용하는 개발자

이 문서는 현재 프로젝트에 구현된 MiniDump 기반 크래시 덤프 시스템의 구조, 동작 흐름, 출력 결과, 그리고 에디터 콘솔 명령어 사용법을 정리한다.

## 1. 개요

현재 크래시 덤프 시스템의 목표는 다음과 같다.

- 엔진 실행 중 발생한 unhandled exception을 감지해 `.dmp`와 `.txt` 로그를 남긴다.
- 현재 스레드 또는 다른 스레드의 상태를 프로세스를 종료하지 않고 snapshot 형태로 저장할 수 있다.
- 실제 크래시 경로와 수동 snapshot 경로가 동일한 결과 구조를 사용하도록 통합한다.
- 콘솔 명령을 통해 과제 시연과 디버깅이 가능하도록 한다.

핵심 API는 아래 파일에 정리되어 있다.

- `JSEngine/Source/Engine/Core/CrashDump.h`
- `JSEngine/Source/Engine/Core/CrashDumpCapture.cpp`
- `JSEngine/Source/Engine/Core/CrashDumpArtifacts.cpp`
- `JSEngine/Source/Engine/Core/CrashDumpFormatting.cpp`
- `JSEngine/Source/Engine/Core/CrashDumpRuntime.cpp`

## 2. 구성 요소

현재 시스템은 역할별로 4개 구현 파일로 나뉜다.

### 2.1 Capture

파일:

- `JSEngine/Source/Engine/Core/CrashDumpCapture.cpp`

역할:

- `EXCEPTION_POINTERS`를 깊은 복사해 안정적인 `FCrashContext`를 만든다.
- 현재 스레드 컨텍스트를 `RtlCaptureContext`로 캡처한다.
- 다른 스레드를 `SuspendThread` / `GetThreadContext` / `ResumeThread` 흐름으로 캡처한다.
- `FCrashContext`의 thread handle 소유권을 관리한다.

주요 타입:

- `FCrashContext`
- `FCrashCaptureResult`

주요 함수:

- `CaptureCrashContext(...)`
- `CaptureCurrentThreadContext(...)`
- `CaptureOtherThreadContext(...)`
- `EnumerateProcessThreads(...)`

### 2.2 Artifacts

파일:

- `JSEngine/Source/Engine/Core/CrashDumpArtifacts.cpp`

역할:

- dump 파일 경로 생성
- `MiniDumpWriteDump` 호출
- crash log text 파일 생성
- dump/log 결과를 `FCrashArtifactResult`로 집계

주요 함수:

- `WriteCrashDump(...)`
- `WriteCrashLog(...)`
- `WriteCrashArtifacts(...)`
- `WriteCurrentThreadSnapshot(...)`
- `WriteCapturedThreadSnapshot(...)`

### 2.3 Formatting

파일:

- `JSEngine/Source/Engine/Core/CrashDumpFormatting.cpp`

역할:

- 실패 단계 enum을 문자열로 변환
- 사람이 읽기 쉬운 failure description 생성
- 콘솔 출력용 summary 문자열 생성
- crash message box용 문자열 생성

주요 함수:

- `GetCrashArtifactFailureStageString(...)`
- `DescribeCrashArtifactFailure(...)`
- `BuildCrashArtifactConsoleSummary(...)`
- `BuildCrashArtifactDialogMessage(...)`

### 2.4 Runtime

파일:

- `JSEngine/Source/Engine/Core/CrashDumpRuntime.cpp`

역할:

- crash handler 설치
- 실제 unhandled exception 보고
- `RaiseException` 기반 강제 크래시 유발
- `CauseCrash()` 진입점 제공

주요 함수:

- `InstallCrashHandler()`
- `ReportCrash(...)`
- `RaiseCrashException(...)`
- `CauseCrash()`

## 3. 데이터 구조

### 3.1 FCrashContext

`FCrashContext`는 크래시 또는 snapshot 시점의 스레드 상태를 소유하는 구조체다.

포함 정보:

- `ThreadId`
- `ThreadHandle`
- `CONTEXT`
- `EXCEPTION_RECORD`
- `EXCEPTION_POINTERS`
- 설명 메시지 문자열

중요한 점:

- `EXCEPTION_POINTERS`는 외부 포인터를 그대로 들고 있지 않는다.
- 구조체 내부의 `ContextRecord`, `ExceptionRecord`를 가리키도록 재구성된다.
- 따라서 예외 필터를 벗어난 뒤에도 안전하게 dump/log 생성에 사용할 수 있다.

### 3.2 FCrashArtifactResult

`FCrashArtifactResult`는 artifact 생성 결과를 나타낸다.

포함 정보:

- 컨텍스트 캡처 성공 여부
- dump 작성 성공 여부
- log 작성 성공 여부
- failure stage
- failure error code
- capture/dump/log 개별 error code
- 생성된 dump/log 경로

이 구조체는 실제 crash 경로와 콘솔 snapshot 경로가 공통으로 사용한다.

### 3.3 ECrashArtifactFailureStage

실패 위치를 구조적으로 표현하기 위한 enum이다.

대표 값:

- `InvalidThreadId`
- `SelfCapture`
- `OpenThread`
- `DuplicateThreadHandle`
- `SuspendThread`
- `GetThreadContext`
- `ResumeThread`
- `DumpWrite`
- `LogWrite`

이 값은 콘솔 출력과 crash message box에 그대로 사용된다.

## 4. 실행 흐름

### 4.1 실제 crash 경로

실제 crash 경로는 아래 순서로 동작한다.

1. `Launch()`에서 `InstallCrashHandler()` 호출
2. `GuardedMain()`이 `__try / __except`로 감싸진 상태에서 실행
3. 실제 SEH 예외 또는 `RaiseCrashException()` 호출 발생
4. `ReportCrash(GetExceptionInformation())` 실행
5. `CaptureCrashContext(...)`로 안정적인 컨텍스트 생성
6. `WriteCrashArtifacts(...)`로 dump/log 생성
7. `BuildCrashArtifactDialogMessage(...)` 결과를 `MessageBoxW`로 표시

관련 파일:

- `JSEngine/Source/Engine/Runtime/Launch.cpp`
- `JSEngine/Source/Engine/Core/CrashDumpRuntime.cpp`

### 4.2 C++ 예외 경로

`GuardedMain()` 내부의 C++ 예외는 직접 dump를 쓰지 않고 SEH 경로로 다시 보낸다.

- `catch (const std::exception& e)`  
  `RaiseCrashException(CRASH_EXCEPTION_CODE_STD_EXCEPTION, e.what())`

- `catch (...)`  
  `RaiseCrashException(CRASH_EXCEPTION_CODE_UNKNOWN_CPP_EXCEPTION, "...")`

이렇게 하면 실제 crash 경로와 동일한 dump/log 처리 흐름을 사용할 수 있다.

### 4.3 현재 스레드 snapshot 경로

비파괴 snapshot은 아래 순서로 동작한다.

1. 콘솔 명령에서 `WriteCurrentThreadSnapshot(...)` 호출
2. `CaptureCurrentThreadContext(...)`로 현재 스레드 컨텍스트 캡처
3. synthetic exception record 구성
4. `WriteCrashArtifacts(...)` 호출
5. 결과를 콘솔 summary로 출력

프로세스는 종료되지 않는다.

### 4.4 다른 스레드 snapshot 경로

다른 스레드 snapshot은 아래 순서로 동작한다.

1. 콘솔 명령에서 thread id 입력
2. `WriteCapturedThreadSnapshot(threadId, ...)` 호출
3. `OpenThread(...)`로 대상 스레드 열기
4. `CaptureOtherThreadContext(...)` 실행
5. 내부에서 `DuplicateThreadHandle`, `SuspendThread`, `GetThreadContext`, `ResumeThread` 수행
6. `WriteCrashArtifacts(...)` 호출
7. 결과를 콘솔 summary로 출력

이 경로는 권한 부족, invalid thread id, suspend 실패 등으로 실패할 수 있으며, 실패 단계와 error code가 함께 출력된다.

## 5. 출력 파일

출력 디렉터리:

- `JSEngine/Saves/Dump/`

파일 종류:

- 실제 crash dump: `Crash_...dmp`
- 실제 crash log: `CrashLog_...txt`
- snapshot dump: `Snapshot_...dmp`
- snapshot log: `SnapshotLog_...txt`

파일명에는 다음 정보가 포함된다.

- 날짜
- 시간
- 밀리초
- process id
- thread id

예:

```text
Crash_20260521_213045_128_pid12345_tid6789.dmp
CrashLog_20260521_213045_128_pid12345_tid6789.txt
Snapshot_20260521_213112_441_pid12345_tid6789.dmp
SnapshotLog_20260521_213112_441_pid12345_tid6789.txt
```

## 6. 콘솔 명령어

이 문서에서 가장 중요한 부분이다. 현재 에디터 콘솔에서는 아래 명령어를 사용할 수 있다.

관련 파일:

- `JSEngine/Source/Editor/UI/EditorConsoleWidget.h`
- `JSEngine/Source/Editor/UI/EditorConsoleWidget.cpp`

### 6.1 causecrash

설명:

- 의도적으로 실제 크래시를 발생시킨다.
- unhandled exception 경로와 MiniDump 생성 경로를 검증할 때 사용한다.

명령:

```text
causecrash
```

alias:

```text
crash
```

동작:

- `CauseCrash()` 호출
- `RaiseCrashException(...)` 호출
- `ReportCrash(...)` 경로로 들어감
- dump/log 생성 후 프로세스 종료

주의:

- 이 명령은 실제로 프로세스를 종료시킨다.

### 6.2 dumpcurrentthread

설명:

- 현재 스레드 상태를 비파괴적으로 저장한다.
- 프로세스는 계속 살아 있고, dump/log만 생성된다.

명령:

```text
dumpcurrentthread
```

alias:

```text
dumpthread
```

동작:

- `WriteCurrentThreadSnapshot(...)` 호출
- 현재 스레드 컨텍스트 캡처
- `Snapshot_...dmp`, `SnapshotLog_...txt` 생성
- 콘솔에 경로와 결과 출력

출력 예:

```text
Current-thread snapshot written:
  Dump: Saves/Dump/Snapshot_...dmp
  Log : Saves/Dump/SnapshotLog_...txt
```

실패 예:

```text
[ERROR] Current-thread snapshot artifact generation was incomplete.
  Dump: Saves/Dump/Snapshot_...dmp [failed]
  Log : Saves/Dump/SnapshotLog_...txt
  Failure: Failed to write the mini dump file.
  Failure Stage: DumpWrite
  Error Code: 5
```

### 6.3 listthreads

설명:

- 현재 프로세스에 속한 스레드 id 목록을 출력한다.
- 다른 스레드 snapshot을 찍기 전에 thread id를 확인할 때 사용한다.

명령:

```text
listthreads
```

alias:

```text
threads
```

동작:

- `EnumerateProcessThreads(...)` 호출
- `CreateToolhelp32Snapshot` 기반으로 현재 프로세스 스레드 목록을 수집
- 현재 스레드는 `(current)`로 표시

출력 예:

```text
Process threads:
  4512 (current)
  9824
  10020
Use 'dumpcapturedthread <threadId>' to capture another thread.
```

### 6.4 dumpcapturedthread

설명:

- 다른 스레드의 컨텍스트를 캡처해 비파괴 snapshot을 생성한다.

명령:

```text
dumpcapturedthread <threadId>
```

alias:

```text
dumpthreadid <threadId>
```

동작:

- `OpenThread(...)`
- `CaptureOtherThreadContext(...)`
- `WriteCrashArtifacts(...)`
- 콘솔에 결과 출력

출력 예:

```text
Captured-thread snapshot written for thread 9824:
  Dump: Saves/Dump/Snapshot_...dmp
  Log : Saves/Dump/SnapshotLog_...txt
```

실패 예:

```text
[ERROR] Snapshot artifact generation was incomplete for thread 9824.
  Dump: <not generated>
  Log : <not generated>
  Failure: Failed to suspend the target thread.
  Failure Stage: SuspendThread
  Error Code: 5
  Capture Error: 5
```

주의:

- 현재 스레드를 넣으면 거부되고 `dumpcurrentthread` 사용을 안내한다.
- 권한이나 스레드 상태에 따라 캡처 실패가 가능하다.

## 7. 로그 내용

`.txt` crash log에는 아래 정보가 들어간다.

- report type
- process id
- thread id
- exception code
- exception address
- crash context message
- call stack

report type 예:

- `Crash`
- `Manual Snapshot`
- `Captured Thread Snapshot`

## 8. failure handling

현재 시스템은 실패를 `FCrashArtifactResult`에 구조적으로 모은다.

예를 들어 다른 스레드 snapshot 실패 시:

- `OpenThread` 실패
- `DuplicateThreadHandle` 실패
- `SuspendThread` 실패
- `GetThreadContext` 실패
- `ResumeThread` 실패
- `DumpWrite` 실패
- `LogWrite` 실패

중 어디서 문제가 났는지 `FailureStage`와 error code로 확인할 수 있다.

### 8.1 콘솔

콘솔은 `BuildCrashArtifactConsoleSummary(...)`를 사용한다.

따라서 아래 정보가 함께 보인다.

- dump 경로
- log 경로
- failure description
- failure stage
- error code
- capture/dump/log 개별 error code

### 8.2 실제 crash dialog

실제 crash 경로는 `BuildCrashArtifactDialogMessage(...)`를 사용한다.

따라서 message box에 아래 정보가 포함된다.

- 생성된 dump 파일 경로
- 생성된 log 파일 경로
- failure description
- failure stage
- error code

## 9. 과제 시연 권장 순서

과제 시연 시에는 아래 순서를 권장한다.

1. `dumpcurrentthread`
2. `listthreads`
3. `dumpcapturedthread <threadId>`
4. `causecrash`

이 순서의 장점:

- 먼저 비파괴 snapshot 기능을 확인할 수 있다.
- 마지막에 실제 크래시 경로를 검증할 수 있다.
- 과제 요구사항인 "임의 위치(CallStack)에서 Crash를 일으키는 콘솔 명령어"도 `causecrash`로 시연 가능하다.

## 10. 관련 API와 과제 요구사항 연결

현재 구현은 과제에서 요구한 Win32 / DbgHelp 계열 API를 다음처럼 사용한다.

- `MiniDumpWriteDump()`  
  실제 `.dmp` 파일 생성

- `RaiseException()`  
  `RaiseCrashException(...)` 내부에서 강제 크래시 유발

- `SetUnhandledExceptionFilter()`  
  `InstallCrashHandler()` 내부에서 전역 unhandled exception filter 등록

또한 C++ 예외 경로도 아래처럼 연결된다.

- `try`
- `catch (const std::exception& e)`
- `catch (...)`

이 예외들은 최종적으로 `RaiseCrashException(...)`을 통해 같은 crash reporting 경로를 사용한다.

## 11. 주의 사항

- `causecrash`는 실제 종료를 유발한다.
- 다른 스레드 캡처는 운영체제 권한과 스레드 상태에 따라 실패할 수 있다.
- 심볼 상태에 따라 call stack 품질은 달라질 수 있다.
- 빌드 검증은 별도로 수행하지 않았으며, 실제 확인은 사용자가 빌드 후 진행해야 한다.

## 12. 요약

현재 크래시 덤프 시스템은 다음 4가지를 지원한다.

- 실제 unhandled exception dump 생성
- 현재 스레드 비파괴 snapshot 생성
- 다른 스레드 비파괴 snapshot 생성
- 콘솔 명령 기반 검증 및 시연

실무적으로 가장 자주 쓰는 명령어는 아래 4개다.

```text
dumpcurrentthread
listthreads
dumpcapturedthread <threadId>
causecrash
```

이 네 명령만 알아도 현재 시스템의 대부분을 사용할 수 있다.
