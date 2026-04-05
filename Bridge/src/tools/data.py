import json
from typing import Any

from src.server import mcp
from src.ue_connection import get_connection


async def _cmd(command: str, params: dict[str, Any] | None = None, timeout_ms: int = 10000) -> str:
    conn = get_connection()
    result = await conn.send_command(command, params, timeout_ms)
    return json.dumps(result, ensure_ascii=False)


@mcp.tool()
async def create_data_table(name: str, row_struct: str, path: str | None = None) -> str:
    """DataTable 에셋을 생성합니다. row_struct: 행 구조체 이름"""
    params: dict[str, Any] = {"name": name, "row_struct": row_struct}
    if path: params["path"] = path
    return await _cmd("create_data_table", params)


@mcp.tool()
async def get_data_table_rows(data_table_path: str, limit: int = 100, offset: int = 0) -> str:
    """DataTable의 행 데이터를 조회합니다."""
    return await _cmd("get_data_table_rows", {
        "data_table_path": data_table_path, "limit": limit, "offset": offset,
    })


@mcp.tool()
async def get_data_table_row_names(data_table_path: str) -> str:
    """DataTable의 행 이름 목록을 반환합니다."""
    return await _cmd("get_data_table_row_names", {"data_table_path": data_table_path})


@mcp.tool()
async def add_data_table_row(data_table_path: str, row_name: str, values: dict | None = None) -> str:
    """DataTable에 행을 추가합니다."""
    params: dict[str, Any] = {"data_table_path": data_table_path, "row_name": row_name}
    if values: params["values"] = values
    return await _cmd("add_data_table_row", params)


@mcp.tool()
async def remove_data_table_row(data_table_path: str, row_name: str) -> str:
    """DataTable에서 행을 제거합니다."""
    return await _cmd("remove_data_table_row", {
        "data_table_path": data_table_path, "row_name": row_name,
    })


@mcp.tool()
async def get_curve_table_info(curve_table_path: str) -> str:
    """CurveTable 정보를 조회합니다."""
    return await _cmd("get_curve_table_info", {"curve_table_path": curve_table_path})


@mcp.tool()
async def get_data_asset_properties(asset_path: str) -> str:
    """DataAsset의 속성을 조회합니다."""
    return await _cmd("get_data_asset_properties", {"asset_path": asset_path})


@mcp.tool()
async def create_curve(name: str, curve_type: str, path: str | None = None) -> str:
    """커브 에셋을 생성합니다. curve_type: CurveFloat, CurveLinearColor, CurveVector"""
    params: dict[str, Any] = {"name": name, "curve_type": curve_type}
    if path: params["path"] = path
    return await _cmd("create_curve", params)


@mcp.tool()
async def get_curve_keys(curve_path: str) -> str:
    """커브의 키 데이터를 조회합니다. CurveFloat은 keys, CurveLinearColor은 r/g/b/a 채널."""
    return await _cmd("get_curve_keys", {"curve_path": curve_path})


@mcp.tool()
async def set_curve_keys(
    curve_path: str,
    keys: list[dict] | None = None,
    r: list[dict] | None = None,
    g: list[dict] | None = None,
    b: list[dict] | None = None,
    a: list[dict] | None = None,
    x: list[dict] | None = None,
    y: list[dict] | None = None,
    z: list[dict] | None = None,
) -> str:
    """커브에 키를 일괄 설정합니다. 각 키: {time, value, interp?, arrive_tangent?, leave_tangent?}"""
    params: dict[str, Any] = {"curve_path": curve_path}
    if keys: params["keys"] = keys
    if r: params["r"] = r
    if g: params["g"] = g
    if b: params["b"] = b
    if a: params["a"] = a
    if x: params["x"] = x
    if y: params["y"] = y
    if z: params["z"] = z
    return await _cmd("set_curve_keys", params)
