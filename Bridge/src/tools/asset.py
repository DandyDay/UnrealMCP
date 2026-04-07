from typing import Any

from src.server import mcp
from src.tools._helpers import cmd as _cmd


@mcp.tool()
async def create_asset(
    class_name: str,
    name: str,
    path: str | None = None,
    properties: dict | None = None,
) -> str:
    """범용 에셋 생성. class_name 예: InputAction, InputMappingContext, DataTable, CurveFloat, DataAsset"""
    params: dict[str, Any] = {"class_name": class_name, "name": name}
    if path:
        params["path"] = path
    if properties:
        params["properties"] = properties
    return await _cmd("create_asset", params)


@mcp.tool()
async def find_assets(
    search_path: str | None = None,
    class_filter: str | None = None,
    name_pattern: str | None = None,
    limit: int = 100,
    offset: int = 0,
) -> str:
    """AssetRegistry로 에셋을 검색합니다."""
    params: dict[str, Any] = {"limit": limit, "offset": offset}
    if search_path: params["search_path"] = search_path
    if class_filter: params["class_filter"] = class_filter
    if name_pattern: params["name_pattern"] = name_pattern
    return await _cmd("find_assets", params)


@mcp.tool()
async def get_asset_info(asset_path: str) -> str:
    """에셋 상세 정보를 조회합니다."""
    return await _cmd("get_asset_info", {"asset_path": asset_path})


@mcp.tool()
async def get_asset_references(asset_path: str, direction: str, limit: int = 100, offset: int = 0) -> str:
    """에셋 의존성/역참조를 조회합니다. direction: dependencies 또는 referencers"""
    return await _cmd("get_asset_references", {
        "asset_path": asset_path, "direction": direction, "limit": limit, "offset": offset,
    })


@mcp.tool()
async def import_asset(source_file: str, destination_path: str) -> str:
    """외부 파일을 임포트합니다 (텍스처, FBX 등)."""
    return await _cmd("import_asset", {
        "source_file": source_file, "destination_path": destination_path,
    }, timeout_ms=120000)


@mcp.tool()
async def rename_asset(asset_path: str, new_name: str) -> str:
    """에셋 이름을 변경합니다 (리다이렉터 자동 생성)."""
    return await _cmd("rename_asset", {"asset_path": asset_path, "new_name": new_name})


@mcp.tool()
async def move_asset(asset_path: str, new_path: str) -> str:
    """에셋을 이동합니다."""
    return await _cmd("move_asset", {"asset_path": asset_path, "new_path": new_path})


@mcp.tool()
async def delete_asset(asset_path: str, force: bool = False) -> str:
    """에셋을 삭제합니다."""
    return await _cmd("delete_asset", {"asset_path": asset_path, "force": force})


@mcp.tool()
async def duplicate_asset(asset_path: str, new_name: str, new_path: str | None = None) -> str:
    """에셋을 복제합니다."""
    params: dict[str, Any] = {"asset_path": asset_path, "new_name": new_name}
    if new_path: params["new_path"] = new_path
    return await _cmd("duplicate_asset", params)
