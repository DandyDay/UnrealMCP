import json
from typing import Any

from src.server import mcp
from src.ue_connection import get_connection


async def _cmd(command: str, params: dict[str, Any] | None = None, timeout_ms: int = 10000) -> str:
    conn = get_connection()
    result = await conn.send_command(command, params, timeout_ms)
    return json.dumps(result, ensure_ascii=False)


@mcp.tool()
async def create_widget_blueprint(name: str, path: str | None = None) -> str:
    """위젯 블루프린트를 생성합니다."""
    params: dict[str, Any] = {"name": name}
    if path: params["path"] = path
    return await _cmd("create_widget_blueprint", params)


@mcp.tool()
async def add_widget(widget_bp_path: str, widget_class: str, name: str | None = None) -> str:
    """위젯을 추가합니다. widget_class 예: TextBlock, Button, Image"""
    params: dict[str, Any] = {"widget_bp_path": widget_bp_path, "widget_class": widget_class}
    if name: params["name"] = name
    return await _cmd("add_widget", params)


@mcp.tool()
async def remove_widget(widget_bp_path: str, widget_name: str) -> str:
    """위젯을 제거합니다."""
    return await _cmd("remove_widget", {"widget_bp_path": widget_bp_path, "widget_name": widget_name})


@mcp.tool()
async def set_widget_property(widget_bp_path: str, widget_name: str, property: str, value: Any) -> str:
    """위젯 속성을 수정합니다."""
    return await _cmd("set_widget_property", {
        "widget_bp_path": widget_bp_path, "widget_name": widget_name,
        "property": property, "value": value,
    })


@mcp.tool()
async def set_widget_layout(
    widget_bp_path: str, widget_name: str,
    position: list[float] | None = None,
    size: list[float] | None = None,
    alignment: list[float] | None = None,
) -> str:
    """위젯 레이아웃을 설정합니다."""
    params: dict[str, Any] = {"widget_bp_path": widget_bp_path, "widget_name": widget_name}
    if position: params["position"] = position
    if size: params["size"] = size
    if alignment: params["alignment"] = alignment
    return await _cmd("set_widget_layout", params)


@mcp.tool()
async def bind_widget_event(widget_bp_path: str, widget_name: str, event_name: str) -> str:
    """위젯 이벤트를 바인딩합니다. (v1: 미구현, BP Node 커맨드 사용 권장)"""
    return await _cmd("bind_widget_event", {
        "widget_bp_path": widget_bp_path, "widget_name": widget_name, "event_name": event_name,
    })


@mcp.tool()
async def get_widget_tree(widget_bp_path: str) -> str:
    """위젯 계층 구조를 조회합니다."""
    return await _cmd("get_widget_tree", {"widget_bp_path": widget_bp_path})
