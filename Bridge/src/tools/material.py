from typing import Any

from src.server import mcp
from src.tools._helpers import cmd as _cmd


@mcp.tool()
async def create_material(name: str, path: str | None = None) -> str:
    """머티리얼 에셋을 생성합니다."""
    params: dict[str, Any] = {"name": name}
    if path: params["path"] = path
    return await _cmd("create_material", params)


@mcp.tool()
async def create_material_instance(name: str, parent_material: str, path: str | None = None) -> str:
    """머티리얼 인스턴스를 생성합니다."""
    params: dict[str, Any] = {"name": name, "parent_material": parent_material}
    if path: params["path"] = path
    return await _cmd("create_material_instance", params)


@mcp.tool()
async def add_material_expression(material_path: str, expression_class: str, position: list[float] | None = None) -> str:
    """머티리얼에 노드를 추가합니다. expression_class 예: MaterialExpressionConstant3Vector"""
    params: dict[str, Any] = {"material_path": material_path, "expression_class": expression_class}
    if position: params["position"] = position
    return await _cmd("add_material_expression", params)


@mcp.tool()
async def connect_material_nodes(material_path: str, source_node_id: int, output_index: int, target_node_id: int, input_index: int) -> str:
    """머티리얼 노드를 연결합니다 (인덱스 기반)."""
    return await _cmd("connect_material_nodes", {
        "material_path": material_path, "source_node_id": source_node_id,
        "output_index": output_index, "target_node_id": target_node_id, "input_index": input_index,
    })


@mcp.tool()
async def disconnect_material_node(material_path: str, node_id: int, input_index: int) -> str:
    """머티리얼 노드 연결을 해제합니다."""
    return await _cmd("disconnect_material_node", {
        "material_path": material_path, "node_id": node_id, "input_index": input_index,
    })


@mcp.tool()
async def set_material_expression_param(material_path: str, node_id: int, param_name: str, value: Any) -> str:
    """머티리얼 노드 파라미터를 수정합니다."""
    return await _cmd("set_material_expression_param", {
        "material_path": material_path, "node_id": node_id, "param_name": param_name, "value": value,
    })


@mcp.tool()
async def connect_to_material_output(material_path: str, node_id: int, output_index: int, material_property: str) -> str:
    """머티리얼 최종 출력에 연결합니다. material_property: BaseColor, Normal, Roughness 등"""
    return await _cmd("connect_to_material_output", {
        "material_path": material_path, "node_id": node_id,
        "output_index": output_index, "material_property": material_property,
    })


@mcp.tool()
async def set_material_instance_param(instance_path: str, param_name: str, value: Any, param_type: str) -> str:
    """머티리얼 인스턴스 파라미터를 설정합니다. param_type: Scalar, Vector, Texture"""
    return await _cmd("set_material_instance_param", {
        "instance_path": instance_path, "param_name": param_name, "value": value, "param_type": param_type,
    })


@mcp.tool()
async def compile_material(material_path: str) -> str:
    """머티리얼을 컴파일합니다."""
    return await _cmd("compile_material", {"material_path": material_path}, timeout_ms=120000)


@mcp.tool()
async def get_material_info(material_path: str) -> str:
    """머티리얼 전체 정보를 조회합니다."""
    return await _cmd("get_material_info", {"material_path": material_path})


@mcp.tool()
async def set_material_property(material_path: str, property: str, value: str) -> str:
    """머티리얼 속성을 변경합니다. property: BlendMode, TwoSided"""
    return await _cmd("set_material_property", {
        "material_path": material_path, "property": property, "value": value,
    })
