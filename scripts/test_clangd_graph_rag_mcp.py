from __future__ import annotations

import argparse
import asyncio
import json
import sys
from typing import Any

import httpx
from mcp.client.session import ClientSession
from mcp.client.sse import sse_client


async def run_test(base_url: str) -> int:
    health_url = f"{base_url.rstrip('/')}/health"
    sse_url = f"{base_url.rstrip('/')}/sse"

    async with httpx.AsyncClient(timeout=10) as client:
        response = await client.get(health_url)
        if response.status_code != 200:
            print(f"[Test] ERROR: health failed ({response.status_code})")
            return 1
        print(f"[Test] health ok: {response.text}")

    async with sse_client(sse_url) as (read_stream, write_stream):
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
    parser = argparse.ArgumentParser(
        description="Smoke test for clangd_graph_rag MCP over SSE"
    )
    parser.add_argument(
        "--base-url", default="http://127.0.0.1:8800", help="MCP HTTP base URL"
    )
    args = parser.parse_args()

    try:
        return asyncio.run(run_test(args.base_url))
    except Exception as exc:  # pragma: no cover - test utility
        print(f"[Test] ERROR: {exc}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
