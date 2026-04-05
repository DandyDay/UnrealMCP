import json
from typing import Any

from src.server import mcp
from src.ue_connection import get_connection


async def _cmd(command: str, params: dict[str, Any] | None = None, timeout_ms: int = 10000) -> str:
    conn = get_connection()
    result = await conn.send_command(command, params, timeout_ms)
    return json.dumps(result, ensure_ascii=False)


@mcp.tool()
async def create_input_mapping(mapping_context_path: str, input_action_path: str, key: str) -> str:
    """Enhanced Input 기반 입력 매핑을 생성합니다."""
    return await _cmd("create_input_mapping", {
        "mapping_context_path": mapping_context_path,
        "input_action_path": input_action_path, "key": key,
    })


@mcp.tool()
async def get_project_settings(category: str) -> str:
    """프로젝트 설정을 조회합니다. category: General, Input"""
    return await _cmd("get_project_settings", {"category": category})


@mcp.tool()
async def set_project_setting(setting_path: str, value: Any) -> str:
    """프로젝트 설정을 변경합니다. setting_path: 'ClassName.PropertyName'"""
    return await _cmd("set_project_setting", {"setting_path": setting_path, "value": value})


@mcp.tool()
async def get_plugins_list(limit: int = 100, offset: int = 0) -> str:
    """플러그인 목록을 조회합니다."""
    return await _cmd("get_plugins_list", {"limit": limit, "offset": offset})


@mcp.tool()
async def set_plugin_enabled(plugin_name: str, enabled: bool) -> str:
    """플러그인을 활성/비활성합니다. 반영에는 에디터 재시작이 필요합니다."""
    return await _cmd("set_plugin_enabled", {"plugin_name": plugin_name, "enabled": enabled})
