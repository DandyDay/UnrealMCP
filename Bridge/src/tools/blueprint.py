import json
from typing import Any

from src.server import mcp
from src.ue_connection import get_connection


async def _cmd(command: str, params: dict[str, Any] | None = None, timeout_ms: int = 10000) -> str:
    conn = get_connection()
    result = await conn.send_command(command, params, timeout_ms)
    return json.dumps(result, ensure_ascii=False)


@mcp.tool()
async def create_blueprint(name: str, parent_class: str, path: str | None = None) -> str:
    """새 블루프린트 에셋을 생성합니다. parent_class 예: Actor, Pawn, Character"""
    params: dict[str, Any] = {"name": name, "parent_class": parent_class}
    if path: params["path"] = path
    return await _cmd("create_blueprint", params)


@mcp.tool()
async def open_blueprint(blueprint_path: str) -> str:
    """에디터에서 블루프린트를 엽니다."""
    return await _cmd("open_blueprint", {"blueprint_path": blueprint_path})


@mcp.tool()
async def compile_blueprint(blueprint_path: str) -> str:
    """블루프린트를 컴파일합니다."""
    return await _cmd("compile_blueprint", {"blueprint_path": blueprint_path}, timeout_ms=120000)


@mcp.tool()
async def add_component(blueprint_path: str, component_class: str, name: str | None = None) -> str:
    """블루프린트에 컴포넌트를 추가합니다. component_class 예: StaticMeshComponent, PointLightComponent"""
    params: dict[str, Any] = {"blueprint_path": blueprint_path, "component_class": component_class}
    if name: params["name"] = name
    return await _cmd("add_component", params)


@mcp.tool()
async def remove_component(blueprint_path: str, component_name: str) -> str:
    """블루프린트에서 컴포넌트를 제거합니다."""
    return await _cmd("remove_component", {"blueprint_path": blueprint_path, "component_name": component_name})


@mcp.tool()
async def get_components(blueprint_path: str) -> str:
    """블루프린트의 컴포넌트 목록을 조회합니다."""
    return await _cmd("get_components", {"blueprint_path": blueprint_path})


@mcp.tool()
async def set_component_property(blueprint_path: str, component_name: str, property: str, value: Any) -> str:
    """블루프린트 컴포넌트의 속성을 수정합니다."""
    return await _cmd("set_component_property", {
        "blueprint_path": blueprint_path, "component_name": component_name,
        "property": property, "value": value,
    })


@mcp.tool()
async def get_blueprint_info(blueprint_path: str) -> str:
    """블루프린트의 전체 정보를 조회합니다 (부모 클래스, 변수, 컴포넌트, 인터페이스)."""
    return await _cmd("get_blueprint_info", {"blueprint_path": blueprint_path})


@mcp.tool()
async def add_variable(
    blueprint_path: str, var_name: str, var_type: str,
    default_value: Any = None, editable: bool = False,
) -> str:
    """블루프린트에 변수를 추가합니다. var_type: bool, int, float, string, vector, rotator"""
    params: dict[str, Any] = {
        "blueprint_path": blueprint_path, "var_name": var_name, "var_type": var_type,
    }
    if default_value is not None: params["default_value"] = default_value
    params["editable"] = editable
    return await _cmd("add_variable", params)


@mcp.tool()
async def remove_variable(blueprint_path: str, var_name: str) -> str:
    """블루프린트에서 변수를 제거합니다."""
    return await _cmd("remove_variable", {"blueprint_path": blueprint_path, "var_name": var_name})


@mcp.tool()
async def set_variable_default(blueprint_path: str, var_name: str, value: Any) -> str:
    """블루프린트 변수의 기본값을 설정합니다."""
    return await _cmd("set_variable_default", {
        "blueprint_path": blueprint_path, "var_name": var_name, "value": value,
    })


@mcp.tool()
async def add_interface(blueprint_path: str, interface_path: str) -> str:
    """블루프린트에 인터페이스 구현을 추가합니다."""
    return await _cmd("add_interface", {"blueprint_path": blueprint_path, "interface_path": interface_path})
