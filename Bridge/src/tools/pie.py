from typing import Any

from src.server import mcp
from src.tools._helpers import cmd as _cmd


@mcp.tool()
async def play_in_editor(mode: str | None = None) -> str:
    """PIE를 시작합니다. mode: 기본(뷰포트), NewWindow"""
    params: dict[str, Any] = {}
    if mode: params["mode"] = mode
    return await _cmd("play_in_editor", params)


@mcp.tool()
async def stop_pie() -> str:
    """PIE를 중지합니다."""
    return await _cmd("stop_pie")


@mcp.tool()
async def is_pie_running() -> str:
    """PIE 실행 상태를 조회합니다."""
    return await _cmd("is_pie_running")


@mcp.tool()
async def execute_console_command(command: str) -> str:
    """PIE 콘솔 커맨드를 실행합니다. 차단 리스트가 적용됩니다."""
    return await _cmd("execute_console_command", {"command": command})


@mcp.tool()
async def add_widget_to_viewport(widget_bp_path: str, player_index: int = 0) -> str:
    """PIE 런타임에 위젯을 뷰포트에 추가합니다. PIE 실행 중이어야 합니다."""
    return await _cmd("add_widget_to_viewport", {
        "widget_bp_path": widget_bp_path, "player_index": player_index,
    })
