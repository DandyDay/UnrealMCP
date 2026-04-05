# N1UnrealMCP

[English](README.md)

> **Unreal Engine 5.7** 기준으로 작성되었습니다. 다른 UE 버전에서는 API 차이로 컴파일 에러가 발생할 수 있습니다.

AI 에이전트(Claude Code 등)가 UE 에디터를 원격 조작할 수 있게 해주는 MCP 플러그인입니다. 액터 스폰, 블루프린트 편집, 머티리얼 생성, 에셋 관리 등을 지원합니다.

## 빠른 시작

```bash
# 1. 프로젝트의 Plugins 폴더에 클론
git clone https://github.com/DandyDay/UnrealMCP.git YourProject/Plugins/N1UnrealMCP

# 2. Python 의존성 설치
cd YourProject/Plugins/N1UnrealMCP/Bridge
uv sync

# 3. 프로젝트 빌드 (또는 UE 에디터에서 열면 자동 빌드)
```

Claude Code 설정 파일(`~/.claude.json` 또는 `.mcp.json`)에 추가:

```json
{
  "mcpServers": {
    "n1-unreal-mcp": {
      "command": "uv",
      "args": ["run", "--directory", "/path/to/N1UnrealMCP/Bridge", "python", "-m", "src"]
    }
  }
}
```

UE 에디터 실행 → Claude Code 시작 → 끝. `ping`으로 연결 확인.

---

## 아키텍처

```
Claude Code  ──── MCP (stdio) ────►  Python 브릿지  ──── TCP :55558 ────►  UE 플러그인
(MCP 클라이언트)                      (MCP 서버)                            (EditorSubsystem)
```

- **UE 플러그인** (C++): TCP 소켓 서버, 커맨드 레지스트리, 11개 핸들러 카테고리
- **Python 브릿지**: MCP SDK (`FastMCP`), TCP 클라이언트, tool 정의
- **Claude Code**: `mcpServers` 설정으로 자동 실행

## 기능

11개 카테고리, 104개 커맨드:

| 카테고리 | 커맨드 수 | 설명 |
|---|---|---|
| **Editor** | 19 | 액터 스폰/삭제/변환, 레벨 열기, 뷰포트, 스크린샷 |
| **Blueprint** | 12 | BP 생성, 컴포넌트/변수 추가, 컴파일 |
| **Blueprint Node** | 15 | 이벤트/함수/분기 노드, 핀 연결 |
| **Material** | 11 | 머티리얼 생성, 노드 추가/연결, 속성 설정 |
| **UMG** | 7 | 위젯 블루프린트 생성, 위젯 추가/제거, 레이아웃 |
| **Project** | 5 | Enhanced Input 매핑, 프로젝트 설정, 플러그인 |
| **Asset** | 8 | 에셋 검색/임포트/이름변경/이동/삭제/복제 |
| **Landscape** | 8 | 랜드스케이프 생성, 머티리얼 할당, 레이어 |
| **PIE** | 5 | 플레이/정지, 콘솔 커맨드 (차단 리스트 적용) |
| **Data** | 10 | DataTable CRUD, CurveFloat/CurveLinearColor, DataAsset |
| **Meta** | 4 | ping, list_commands, describe_command, list_categories |

### 주요 설계

- **TMap 기반 커맨드 레지스트리** — 함수 하나 + 등록 한 줄로 새 커맨드 추가
- **Request ID 상관관계** — UUID로 동시 MCP tool 호출 매칭
- **핸드셰이크 프로토콜** — 연결 시 버전 확인 + capabilities 교환
- **Undo/Redo** — 모든 변경형 커맨드를 `FScopedTransaction`으로 래핑
- **Perforce 연동** — 자동 체크아웃, `MarkPackageDirty`, 선택적 저장
- **파라미터 검증** — JSON Schema 서브셋으로 필수 필드 + 타입 검사
- **페이징** — 대량 결과에 `limit`/`offset` 지원
- **콘솔 커맨드 보안** — PIE에서 위험 커맨드 차단 리스트

## 요구사항

- Unreal Engine 5.7+
- Python 3.11+
- [uv](https://docs.astral.sh/uv/) (Python 패키지 매니저)
- Claude Code (또는 MCP 호환 클라이언트)

## 설치

### 1. 플러그인 설치

`N1UnrealMCP/` 폴더를 프로젝트의 `Plugins/` 디렉토리에 복사하고 리빌드합니다.

### 2. Python 의존성 설치

```bash
cd N1UnrealMCP/Bridge
uv sync
```

### 3. Claude Code 설정

`~/.claude.json`의 프로젝트 `mcpServers`에 추가:

```json
{
  "n1-unreal-mcp": {
    "command": "uv",
    "args": [
      "run",
      "--directory",
      "/path/to/N1UnrealMCP/Bridge",
      "python",
      "-m",
      "src"
    ]
  }
}
```

### 4. 실행

1. UE 에디터 실행 (플러그인이 TCP 서버를 포트 55558에 자동 시작)
2. Claude Code 시작 (MCP를 통해 Python 브릿지 자동 실행)
3. `ping`으로 연결 확인

## 사용 예시

```
# 포인트 라이트 스폰
spawn_actor(class_path="PointLight", location=[0, 0, 500])

# 블루프린트 생성 + 컴포넌트 추가
create_blueprint(name="BP_MyActor", parent_class="Actor")
add_component(blueprint_path="/Game/Blueprints/BP_MyActor.BP_MyActor", component_class="StaticMeshComponent")
compile_blueprint(blueprint_path="/Game/Blueprints/BP_MyActor.BP_MyActor")

# 에셋 검색
find_assets(name_pattern="Sky", limit=10)

# 커브 읽기 + 복제
get_curve_keys(curve_path="/Game/Data/Sky/MC_SunIntensity.MC_SunIntensity")
create_curve(name="MC_SunIntensity_New", curve_type="CurveFloat", path="/Game/Data/Sky")
set_curve_keys(curve_path="/Game/Data/Sky/MC_SunIntensity_New.MC_SunIntensity_New", keys=[...])
```

## 확장하기

### 기존 핸들러에 커맨드 추가

```cpp
// 핸들러의 RegisterCommands()에서:
RegisterCommand("my_command", "AI가 보는 설명",
    nullptr, // ParameterSchema (선택)
    true,    // bMutatesEditorState
    false,   // bRequiresPIE
    false,   // bLongRunning
    10000,   // DefaultTimeoutMs
    [this](const TSharedPtr<FJsonObject>& P) { return HandleMyCommand(P); });
```

### 새 핸들러 카테고리 추가

1. `FN1MCPHandlerBase`를 상속하는 `N1MCPMyHandler.h/cpp` 생성
2. `N1MCPBridge.cpp`의 `RegisterAllHandlers()`에 등록
3. `Bridge/src/tools/my_handler.py`에 `@mcp.tool()` 함수 작성
4. `Bridge/src/server.py`에 import 추가

## 프로토콜

TCP 메시지는 줄바꿈(`\n`)으로 구분된 JSON:

```json
// 요청
{"type":"request", "id":"<uuid>", "command":"ping", "params":{}, "timeout_ms":10000}

// 응답
{"type":"response", "id":"<uuid>", "status":"success", "result":{"pong":"N1UnrealMCP"}}

// 핸드셰이크
{"type":"hello", "protocol_version":"1.0", "client":"n1-unreal-mcp-bridge"}
{"type":"hello_ack", "protocol_version":"1.0", "server":"N1UnrealMCP", "capabilities":["pagination","meta_commands"]}
```

## 라이선스

MIT
