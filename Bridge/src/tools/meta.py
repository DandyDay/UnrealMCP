from src.server import mcp
from src.tools._helpers import cmd as _cmd


@mcp.tool()
async def ping() -> str:
    """UE 에디터 연결을 확인합니다."""
    return await _cmd("ping")


@mcp.tool()
async def list_commands(limit: int = 100, offset: int = 0) -> str:
    """등록된 모든 커맨드 이름과 설명을 반환합니다."""
    return await _cmd("list_commands", {"limit": limit, "offset": offset})


@mcp.tool()
async def list_categories() -> str:
    """카테고리 목록을 반환합니다."""
    return await _cmd("list_categories")


@mcp.tool()
async def describe_command(command: str) -> str:
    """특정 커맨드의 상세 파라미터 스키마를 반환합니다."""
    return await _cmd("describe_command", {"command": command})
