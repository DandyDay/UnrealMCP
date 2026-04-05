# N1UnrealMCP

[한국어](README.ko.md)

> Built for **Unreal Engine 5.7**. Other UE versions may require API adjustments.

MCP plugin that lets AI agents (Claude Code, etc.) control the Unreal Editor — spawn actors, edit blueprints, create materials, manage assets, and more.

## Quick Start

```bash
# 1. Clone into your project's Plugins folder
git clone https://github.com/DandyDay/UnrealMCP.git YourProject/Plugins/N1UnrealMCP

# 2. Install Python dependencies
cd YourProject/Plugins/N1UnrealMCP/Bridge
uv sync

# 3. Build the project (or open in UE Editor — it builds automatically)
```

Then add to your Claude Code config (`~/.claude.json` or `.mcp.json`):

```json
{
  "mcpServers": {
    "n1-unreal-mcp": {
      "command": "uv",
      "args": ["run", "--directory", "/path/to/N1UnrealMCP/Bridge", "python", "-m", "src"]
    }
  }
}
```

Open UE Editor → Start Claude Code → Done. Try `ping` to verify.

---

## Architecture

```
Claude Code  ──── MCP (stdio) ────►  Python Bridge  ──── TCP :55558 ────►  UE Plugin
(MCP Client)                         (MCP Server)                          (EditorSubsystem)
```

- **UE Plugin** (C++): TCP socket server, command registry, 11 handler categories
- **Python Bridge**: MCP SDK (`FastMCP`), TCP client, tool definitions
- **Claude Code**: `mcpServers` config for auto-launch

## Features

100+ commands across 11 categories:

| Category | Commands | Description |
|---|---|---|
| **Editor** | 18 | Actor spawn/delete/transform, viewport, screenshot |
| **Blueprint** | 12 | Create BP, add components/variables, compile |
| **Blueprint Node** | 15 | Event/function/branch nodes, pin connections |
| **Material** | 11 | Create materials, add expressions, connect nodes |
| **UMG** | 7 | Widget blueprints, add/remove widgets, layout |
| **Project** | 5 | Enhanced Input mappings, project settings, plugins |
| **Asset** | 8 | Find/import/rename/move/delete/duplicate assets |
| **Landscape** | 8 | Create landscape, assign material, layers |
| **PIE** | 5 | Play/stop, console commands (with blocklist) |
| **Data** | 10 | DataTable CRUD, CurveFloat/CurveLinearColor, DataAsset |
| **Meta** | 4 | ping, list_commands, describe_command, list_categories |

### Key Design

- **TMap-based Command Registry** — add new commands with one function + one registration line
- **Request ID correlation** — concurrent MCP tool calls matched by UUID
- **Handshake protocol** — version check + capabilities exchange on connect
- **Undo/Redo** — all mutating commands wrapped in `FScopedTransaction`
- **Perforce integration** — auto checkout, `MarkPackageDirty`, optional save
- **Parameter validation** — JSON Schema subset for required fields + type checks
- **Pagination** — `limit`/`offset` for large result sets
- **Console command security** — blocklist for dangerous commands in PIE

## Requirements

- Unreal Engine 5.7+
- Python 3.11+
- [uv](https://docs.astral.sh/uv/) (Python package manager)
- Claude Code (or any MCP-compatible client)

## Setup

### 1. Install the Plugin

Copy `N1UnrealMCP/` into your project's `Plugins/` directory. Rebuild.

### 2. Install Python Dependencies

```bash
cd N1UnrealMCP/Bridge
uv sync
```

### 3. Configure Claude Code

Add to `~/.claude.json` (under your project's `mcpServers`):

```json
{
  "n1-unreal-mcp": {
    "command": "uv",
    "args": [
      "run",
      "--directory",
      "/path/to/N1UnrealMCP/Bridge",
      "python",
      "-m",
      "src"
    ]
  }
}
```

### 4. Launch

1. Open UE Editor (plugin auto-starts TCP server on port 55558)
2. Start Claude Code (auto-launches Python bridge via MCP)
3. Use `ping` to verify connection

## Usage Examples

```
# Spawn a point light
spawn_actor(class_path="PointLight", location=[0, 0, 500])

# Create a blueprint with components
create_blueprint(name="BP_MyActor", parent_class="Actor")
add_component(blueprint_path="/Game/Blueprints/BP_MyActor.BP_MyActor", component_class="StaticMeshComponent")
compile_blueprint(blueprint_path="/Game/Blueprints/BP_MyActor.BP_MyActor")

# Search assets
find_assets(name_pattern="Sky", limit=10)

# Read and clone a curve
get_curve_keys(curve_path="/Game/Data/Sky/MC_SunIntensity.MC_SunIntensity")
create_curve(name="MC_SunIntensity_New", curve_type="CurveFloat", path="/Game/Data/Sky")
set_curve_keys(curve_path="/Game/Data/Sky/MC_SunIntensity_New.MC_SunIntensity_New", keys=[...])
```

## Extending

### Add a new command to an existing handler

```cpp
// In your handler's RegisterCommands():
RegisterCommand("my_command", "Description for AI",
    nullptr, // ParameterSchema (optional)
    true,    // bMutatesEditorState
    false,   // bRequiresPIE
    false,   // bLongRunning
    10000,   // DefaultTimeoutMs
    [this](const TSharedPtr<FJsonObject>& P) { return HandleMyCommand(P); });
```

### Add a new handler category

1. Create `N1MCPMyHandler.h/cpp` inheriting `FN1MCPHandlerBase`
2. Register in `N1MCPBridge.cpp`'s `RegisterAllHandlers()`
3. Create `Bridge/src/tools/my_handler.py` with `@mcp.tool()` functions
4. Add import in `Bridge/src/server.py`

## Protocol

TCP messages are newline-delimited JSON:

```json
// Request
{"type":"request", "id":"<uuid>", "command":"ping", "params":{}, "timeout_ms":10000}

// Response
{"type":"response", "id":"<uuid>", "status":"success", "result":{"pong":"N1UnrealMCP"}}

// Handshake
{"type":"hello", "protocol_version":"1.0", "client":"n1-unreal-mcp-bridge"}
{"type":"hello_ack", "protocol_version":"1.0", "server":"N1UnrealMCP", "capabilities":["pagination","meta_commands"]}
```

## License

MIT
