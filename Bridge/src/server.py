from mcp.server.fastmcp import FastMCP

mcp = FastMCP("N1UnrealMCP")


def main():
    from src.tools import meta  # noqa: F401
    from src.tools import editor  # noqa: F401
    from src.tools import blueprint  # noqa: F401
    from src.tools import blueprint_node  # noqa: F401
    from src.tools import material  # noqa: F401
    from src.tools import umg  # noqa: F401
    from src.tools import project  # noqa: F401
    from src.tools import asset  # noqa: F401
    from src.tools import landscape  # noqa: F401
    from src.tools import pie  # noqa: F401
    from src.tools import data  # noqa: F401
    mcp.run(transport="stdio")
