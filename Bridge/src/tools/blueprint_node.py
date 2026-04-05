import json
from typing import Any

from src.server import mcp
from src.ue_connection import get_connection


async def _cmd(command: str, params: dict[str, Any] | None = None, timeout_ms: int = 10000) -> str:
    conn = get_connection()
    result = await conn.send_command(command, params, timeout_ms)
    return json.dumps(result, ensure_ascii=False)


@mcp.tool()
async def add_event_node(blueprint_path: str, event_name: str, position: list[float] | None = None) -> str:
    """이벤트 노드를 생성합니다 (BeginPlay, Tick 등)."""
    params: dict[str, Any] = {"blueprint_path": blueprint_path, "event_name": event_name}
    if position: params["position"] = position
    return await _cmd("add_event_node", params)


@mcp.tool()
async def add_function_call_node(
    blueprint_path: str, function_name: str,
    target_class: str | None = None, position: list[float] | None = None,
) -> str:
    """함수 호출 노드를 생성합니다."""
    params: dict[str, Any] = {"blueprint_path": blueprint_path, "function_name": function_name}
    if target_class: params["target_class"] = target_class
    if position: params["position"] = position
    return await _cmd("add_function_call_node", params)


@mcp.tool()
async def add_variable_get_node(blueprint_path: str, var_name: str, position: list[float] | None = None) -> str:
    """변수 Get 노드를 생성합니다."""
    params: dict[str, Any] = {"blueprint_path": blueprint_path, "var_name": var_name}
    if position: params["position"] = position
    return await _cmd("add_variable_get_node", params)


@mcp.tool()
async def add_variable_set_node(blueprint_path: str, var_name: str, position: list[float] | None = None) -> str:
    """변수 Set 노드를 생성합니다."""
    params: dict[str, Any] = {"blueprint_path": blueprint_path, "var_name": var_name}
    if position: params["position"] = position
    return await _cmd("add_variable_set_node", params)


@mcp.tool()
async def add_self_reference_node(blueprint_path: str, position: list[float] | None = None) -> str:
    """Self 참조 노드를 생성합니다."""
    params: dict[str, Any] = {"blueprint_path": blueprint_path}
    if position: params["position"] = position
    return await _cmd("add_self_reference_node", params)


@mcp.tool()
async def add_component_reference_node(blueprint_path: str, component_name: str, position: list[float] | None = None) -> str:
    """컴포넌트 참조 노드를 생성합니다."""
    params: dict[str, Any] = {"blueprint_path": blueprint_path, "component_name": component_name}
    if position: params["position"] = position
    return await _cmd("add_component_reference_node", params)


@mcp.tool()
async def add_input_action_node(blueprint_path: str, input_action_path: str, position: list[float] | None = None) -> str:
    """Enhanced Input 액션 노드를 생성합니다."""
    params: dict[str, Any] = {"blueprint_path": blueprint_path, "input_action_path": input_action_path}
    if position: params["position"] = position
    return await _cmd("add_input_action_node", params)


@mcp.tool()
async def add_pure_math_node(blueprint_path: str, operation: str, position: list[float] | None = None) -> str:
    """수학 연산 노드를 생성합니다. operation: Add, Subtract, Multiply, Divide 등"""
    params: dict[str, Any] = {"blueprint_path": blueprint_path, "operation": operation}
    if position: params["position"] = position
    return await _cmd("add_pure_math_node", params)


@mcp.tool()
async def add_branch_node(blueprint_path: str, position: list[float] | None = None) -> str:
    """Branch (if) 노드를 생성합니다."""
    params: dict[str, Any] = {"blueprint_path": blueprint_path}
    if position: params["position"] = position
    return await _cmd("add_branch_node", params)


@mcp.tool()
async def add_macro_node(blueprint_path: str, macro_name: str, position: list[float] | None = None) -> str:
    """매크로 노드를 생성합니다."""
    params: dict[str, Any] = {"blueprint_path": blueprint_path, "macro_name": macro_name}
    if position: params["position"] = position
    return await _cmd("add_macro_node", params)


@mcp.tool()
async def connect_nodes(
    blueprint_path: str,
    source_node_id: str, source_pin: str,
    target_node_id: str, target_pin: str,
) -> str:
    """두 노드의 핀을 연결합니다. node_id는 NodeGuid 문자열."""
    return await _cmd("connect_nodes", {
        "blueprint_path": blueprint_path,
        "source_node_id": source_node_id, "source_pin": source_pin,
        "target_node_id": target_node_id, "target_pin": target_pin,
    })


@mcp.tool()
async def disconnect_nodes(blueprint_path: str, node_id: str, pin_name: str) -> str:
    """노드 핀의 모든 연결을 해제합니다."""
    return await _cmd("disconnect_nodes", {
        "blueprint_path": blueprint_path, "node_id": node_id, "pin_name": pin_name,
    })


@mcp.tool()
async def find_nodes(blueprint_path: str, filter: str | None = None, limit: int = 100, offset: int = 0) -> str:
    """그래프 내 노드를 검색합니다."""
    params: dict[str, Any] = {"blueprint_path": blueprint_path, "limit": limit, "offset": offset}
    if filter: params["filter"] = filter
    return await _cmd("find_nodes", params)


@mcp.tool()
async def delete_node(blueprint_path: str, node_id: str) -> str:
    """노드를 삭제합니다."""
    return await _cmd("delete_node", {"blueprint_path": blueprint_path, "node_id": node_id})


@mcp.tool()
async def get_node_pins(blueprint_path: str, node_id: str) -> str:
    """노드의 핀 정보를 조회합니다."""
    return await _cmd("get_node_pins", {"blueprint_path": blueprint_path, "node_id": node_id})
