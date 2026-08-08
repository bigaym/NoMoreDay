from __future__ import annotations

import asyncio
import json
import sys

from mcp import StdioServerParameters
from mcp.client.session import ClientSession
from mcp.client.stdio import stdio_client


async def run_test() -> int:
    server = StdioServerParameters(
        command="powershell",
        args=[
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            r"D:\PRJ\NoMoreDay\scripts\_LaunchClangdGraphRagMcp.ps1",
        ],
    )

    async with stdio_client(server) as (read_stream, write_stream):
        async with ClientSession(read_stream, write_stream) as session:
            await session.initialize()
            print("[Test] MCP initialize ok")

            tools_result = await session.list_tools()
            tools = getattr(tools_result, "tools", [])
            names = [getattr(t, "name", "") for t in tools]
            print(f"[Test] tool_count={len(names)}")
            print(f"[Test] tools={json.dumps(names, ensure_ascii=False)}")

            if "execute_cypher_query" not in names:
                print("[Test] ERROR: execute_cypher_query tool not found")
                return 1

            query = "MATCH (n) RETURN count(n) AS nodes"
            call_result = await session.call_tool(
                "execute_cypher_query", {"query": query}
            )
            content = getattr(call_result, "content", [])
            text_parts = [
                getattr(item, "text", "") for item in content if hasattr(item, "text")
            ]
            merged = "\n".join([p for p in text_parts if p])
            print("[Test] execute_cypher_query response:")
            print(merged)

            if "nodes" not in merged.lower():
                print("[Test] WARNING: response does not contain 'nodes' keyword")

    print("[Test] PASS")
    return 0


def main() -> int:
    try:
        return asyncio.run(run_test())
    except Exception as exc:  # pragma: no cover - utility
        print(f"[Test] ERROR: {exc}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
