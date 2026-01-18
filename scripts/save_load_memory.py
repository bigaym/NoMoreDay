"""
NoMoreDay Session Memory & Snapshot Manager
Purpose: Provides MCP tools to save, load, and list Gemini CLI session snapshots.
         Helps maintain continuity across long-running development tasks.
Usage: python scripts/save_load_memory.py
"""
import json
import os
from datetime import datetime
from mcp.server.fastmcp import FastMCP

# --- 配置区 ---
# 建议使用绝对路径，确保 CLI 运行时能准确找到目录
PROJECT_ROOT = "F:/NoMoreDay"
MEMORY_DIR = os.path.join(PROJECT_ROOT, "memory")
CONTEXT_FILE = os.path.join(PROJECT_ROOT, "GEMINI.md")
# --------------

# 创建 FastMCP 实例
mcp = FastMCP("SnapshotManager")

@mcp.tool()
def get_project_context() -> str:
    """
    读取 GEMINI.md 文件，获取项目的核心背景、技术架构和操作指令。
    开始新会话时应首先调用此工具。
    """
    try:
        if not os.path.exists(CONTEXT_FILE):
            return f"❌ 找不到上下文文件: {CONTEXT_FILE}"
        with open(CONTEXT_FILE, 'r', encoding='utf-8') as f:
            content = f.read()
        return f"✅ 已加载项目上下文 (GEMINI.md):\n\n{content}"
    except Exception as e:
        return f"❌ 读取上下文失败: {str(e)}"

@mcp.tool()
def save_snapshot(filename: str, root_prompt: str, current_context: str) -> str:
    """
    持久化当前的工作记忆快照。
    :param filename: 快照文件名（如 'feat_ecs_done.json'）
    :param root_prompt: 当前任务的原始需求
    :param current_context: 由 memory MCP 压缩后的进度总结
    """
    try:
        if not os.path.exists(MEMORY_DIR):
            os.makedirs(MEMORY_DIR)
        
        # 确保文件名以 .json 结尾
        if not filename.endswith(".json"):
            filename += ".json"
            
        file_path = os.path.join(MEMORY_DIR, filename)
        
        data = {
            "version": "1.0",
            "project": "NoMoreDay",
            "root_prompt": root_prompt,
            "current_context": current_context,
            "timestamp": datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        }
        
        with open(file_path, 'w', encoding='utf-8') as f:
            json.dump(data, f, ensure_ascii=False, indent=2)
            
        return f"✅ 记忆快照已保存至: {file_path}"
    except Exception as e:
        return f"❌ 保存失败: {str(e)}"

@mcp.tool()
def load_snapshot(filename: str) -> str:
    """
    从指定的 JSON 文件加载之前的记忆快照。
    """
    try:
        if not filename.endswith(".json"):
            filename += ".json"
        file_path = os.path.join(MEMORY_DIR, filename)

        if not os.path.exists(file_path):
            return f"❌ 错误：找不到快照文件 {file_path}"
            
        with open(file_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
            
        return json.dumps(data, ensure_ascii=False, indent=2)
    except Exception as e:
        return f"❌ 加载失败: {str(e)}"

@mcp.tool()
def list_snapshots() -> str:
    """列出所有已保存的记忆快照文件"""
    try:
        if not os.path.exists(MEMORY_DIR) or not os.listdir(MEMORY_DIR):
            return "📭 目前没有任何快照文件。"
        files = [f for f in os.listdir(MEMORY_DIR) if f.endswith(".json")]
        return "📂 可用快照列表:\n" + "\n".join(files)
    except Exception as e:
        return f"❌ 无法读取快照目录: {str(e)}"

if __name__ == "__main__":
    mcp.run()