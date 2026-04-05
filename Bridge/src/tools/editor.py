from typing import Any

from src.server import mcp
from src.tools._helpers import cmd as _cmd


# ── 조회 ──

@mcp.tool()
async def get_actors_in_level(
    class_filter: str | None = None,
    tag_filter: str | None = None,
    limit: int = 100,
    offset: int = 0,
) -> str:
    """현재 레벨의 모든 액터 목록을 반환합니다."""
    return await _cmd("get_actors_in_level", {
        "class_filter": class_filter, "tag_filter": tag_filter,
        "limit": limit, "offset": offset,
    })


@mcp.tool()
async def find_actors(
    pattern: str | None = None,
    class_filter: str | None = None,
    tag_filter: str | None = None,
    limit: int = 100,
    offset: int = 0,
) -> str:
    """액터 범용 검색. pattern(이름), class_filter(클래스), tag_filter(태그) 중 하나 이상 필수."""
    params: dict[str, Any] = {"limit": limit, "offset": offset}
    if pattern: params["pattern"] = pattern
    if class_filter: params["class_filter"] = class_filter
    if tag_filter: params["tag_filter"] = tag_filter
    return await _cmd("find_actors", params)


@mcp.tool()
async def get_actor_properties(actor_name: str) -> str:
    """액터의 속성을 조회합니다. actor_name에 object_path도 사용 가능."""
    return await _cmd("get_actor_properties", {"actor_name": actor_name})


@mcp.tool()
async def get_level_info() -> str:
    """현재 레벨의 이름, 액터 수, 서브레벨 목록을 반환합니다."""
    return await _cmd("get_level_info")


@mcp.tool()
async def get_selected_actors() -> str:
    """에디터에서 현재 선택된 액터 목록을 반환합니다."""
    return await _cmd("get_selected_actors")


# ── 변경 ──

@mcp.tool()
async def set_actor_property(actor_name: str, property: str, value: Any) -> str:
    """액터의 속성을 수정합니다."""
    return await _cmd("set_actor_property", {
        "actor_name": actor_name, "property": property, "value": value,
    })


@mcp.tool()
async def set_actor_transform(
    actor_name: str,
    location: list[float] | None = None,
    rotation: list[float] | None = None,
    scale: list[float] | None = None,
) -> str:
    """액터의 위치/회전/스케일을 변경합니다."""
    params: dict[str, Any] = {"actor_name": actor_name}
    if location is not None: params["location"] = location
    if rotation is not None: params["rotation"] = rotation
    if scale is not None: params["scale"] = scale
    return await _cmd("set_actor_transform", params)


@mcp.tool()
async def spawn_actor(
    class_path: str,
    name: str | None = None,
    location: list[float] | None = None,
    rotation: list[float] | None = None,
    scale: list[float] | None = None,
    static_mesh: str | None = None,
) -> str:
    """네이티브 클래스로 액터를 스폰합니다. class_path 예: PointLight, StaticMeshActor. StaticMeshActor의 경우 static_mesh로 메쉬 경로 지정 (예: /Engine/BasicShapes/Cube.Cube)"""
    params: dict[str, Any] = {"class_path": class_path}
    if name: params["name"] = name
    if location: params["location"] = location
    if rotation: params["rotation"] = rotation
    if scale: params["scale"] = scale
    if static_mesh: params["static_mesh"] = static_mesh
    return await _cmd("spawn_actor", params)


@mcp.tool()
async def spawn_blueprint_actor(
    blueprint_path: str,
    name: str | None = None,
    location: list[float] | None = None,
    rotation: list[float] | None = None,
    scale: list[float] | None = None,
) -> str:
    """블루프린트 클래스로 액터를 스폰합니다."""
    params: dict[str, Any] = {"blueprint_path": blueprint_path}
    if name: params["name"] = name
    if location: params["location"] = location
    if rotation: params["rotation"] = rotation
    if scale: params["scale"] = scale
    return await _cmd("spawn_blueprint_actor", params)


@mcp.tool()
async def duplicate_actor(
    actor_name: str,
    new_name: str | None = None,
    offset: list[float] | None = None,
) -> str:
    """액터를 복제합니다."""
    params: dict[str, Any] = {"actor_name": actor_name}
    if new_name: params["new_name"] = new_name
    if offset: params["offset"] = offset
    return await _cmd("duplicate_actor", params)


@mcp.tool()
async def delete_actor(actor_name: str) -> str:
    """액터를 삭제합니다."""
    return await _cmd("delete_actor", {"actor_name": actor_name})


@mcp.tool()
async def rename_actor(actor_name: str, new_name: str) -> str:
    """액터의 이름을 변경합니다."""
    return await _cmd("rename_actor", {"actor_name": actor_name, "new_name": new_name})


@mcp.tool()
async def set_actor_mobility(actor_name: str, mobility: str) -> str:
    """액터의 모빌리티를 설정합니다. mobility: Static, Stationary, Movable"""
    return await _cmd("set_actor_mobility", {"actor_name": actor_name, "mobility": mobility})


@mcp.tool()
async def attach_actor(actor_name: str, parent_name: str, socket: str | None = None) -> str:
    """액터를 다른 액터에 어태치합니다."""
    params: dict[str, Any] = {"actor_name": actor_name, "parent_name": parent_name}
    if socket: params["socket"] = socket
    return await _cmd("attach_actor", params)


@mcp.tool()
async def detach_actor(actor_name: str) -> str:
    """액터의 어태치를 해제합니다."""
    return await _cmd("detach_actor", {"actor_name": actor_name})


@mcp.tool()
async def set_actor_tags(actor_name: str, tags: list[str]) -> str:
    """액터의 태그를 설정합니다."""
    return await _cmd("set_actor_tags", {"actor_name": actor_name, "tags": tags})


@mcp.tool()
async def set_component_material(
    actor_name: str,
    component_name: str,
    material_path: str,
    slot_index: int = 0,
) -> str:
    """액터 컴포넌트의 머티리얼 슬롯을 설정합니다."""
    return await _cmd("set_component_material", {
        "actor_name": actor_name, "component_name": component_name,
        "material_path": material_path, "slot_index": slot_index,
    })


@mcp.tool()
async def focus_viewport(actor_name: str) -> str:
    """뷰포트 카메라를 해당 액터에 포커스합니다."""
    return await _cmd("focus_viewport", {"actor_name": actor_name})


@mcp.tool()
async def take_screenshot(filename: str | None = None) -> str:
    """뷰포트 스크린샷을 캡처합니다."""
    params: dict[str, Any] = {}
    if filename: params["filename"] = filename
    return await _cmd("take_screenshot", params)


@mcp.tool()
async def open_level(level_path: str) -> str:
    """레벨(맵)을 엽니다. 풀 경로(/Game/Level/MapName) 또는 짧은 이름(MapName) 가능."""
    return await _cmd("open_level", {"level_path": level_path}, timeout_ms=30000)
