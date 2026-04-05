from typing import Any

from src.server import mcp
from src.tools._helpers import cmd as _cmd


@mcp.tool()
async def create_landscape(
    sections_per_component: int = 1,
    quads_per_section: int = 63,
    components_x: int = 1,
    components_y: int = 1,
    scale: list[float] | None = None,
) -> str:
    """랜드스케이프를 생성합니다."""
    params: dict[str, Any] = {
        "sections_per_component": sections_per_component,
        "quads_per_section": quads_per_section,
        "components_x": components_x,
        "components_y": components_y,
    }
    if scale: params["scale"] = scale
    return await _cmd("create_landscape", params, timeout_ms=30000)


@mcp.tool()
async def add_landscape_layer(landscape_ref: str, layer_name: str) -> str:
    """랜드스케이프에 페인트 레이어를 추가합니다."""
    return await _cmd("add_landscape_layer", {
        "landscape_ref": landscape_ref, "layer_name": layer_name,
    })


@mcp.tool()
async def sculpt_landscape(landscape_ref: str, center: list[float], radius: float, strength: float, brush_type: str | None = None) -> str:
    """하이트맵 스컬프팅 (실험적, v1 미구현)."""
    params: dict[str, Any] = {
        "landscape_ref": landscape_ref, "center": center,
        "radius": radius, "strength": strength,
    }
    if brush_type: params["brush_type"] = brush_type
    return await _cmd("sculpt_landscape", params, timeout_ms=60000)


@mcp.tool()
async def paint_landscape_layer(landscape_ref: str, layer_name: str, center: list[float], radius: float, strength: float) -> str:
    """레이어 페인팅 (실험적, v1 미구현)."""
    return await _cmd("paint_landscape_layer", {
        "landscape_ref": landscape_ref, "layer_name": layer_name,
        "center": center, "radius": radius, "strength": strength,
    }, timeout_ms=60000)


@mcp.tool()
async def import_heightmap(landscape_ref: str, file_path: str) -> str:
    """하이트맵 파일을 임포트합니다."""
    return await _cmd("import_heightmap", {
        "landscape_ref": landscape_ref, "file_path": file_path,
    }, timeout_ms=120000)


@mcp.tool()
async def export_heightmap(landscape_ref: str, file_path: str) -> str:
    """하이트맵을 내보냅니다."""
    return await _cmd("export_heightmap", {
        "landscape_ref": landscape_ref, "file_path": file_path,
    })


@mcp.tool()
async def assign_landscape_material(landscape_ref: str, material_path: str) -> str:
    """랜드스케이프에 머티리얼을 할당합니다."""
    return await _cmd("assign_landscape_material", {
        "landscape_ref": landscape_ref, "material_path": material_path,
    })


@mcp.tool()
async def get_landscape_info(landscape_ref: str | None = None) -> str:
    """랜드스케이프 정보를 조회합니다. landscape_ref 생략 시 레벨 첫 번째 랜드스케이프."""
    params: dict[str, Any] = {}
    if landscape_ref: params["landscape_ref"] = landscape_ref
    return await _cmd("get_landscape_info", params)
