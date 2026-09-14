#!/usr/bin/env python3
"""生成技能机制表键名 schema（assets/data/skill_mechanics_schema.json）。

背景：SkillMechanicsRegistry 运行时按 (skill_id, node_id, key) 读取
assets/data/skill_mechanics.json。若代码里改了键名而 JSON 未同步（或反之），
读取会静默回退到默认值，无法在编译期或加载期发现。

本脚本扫描 src/ 下全部 GetMech(/GetFloat(/GetInt( 机制读取调用点，尽量静态
解析出 (skill_id, node_id, key) 三元组，并在加载期与 skill_mechanics.json
做双向比对；同时在 CI 用 --check 校验 schema 是否与源码一致（防止改了代码
却忘记重新生成 schema）。

产物结构：
  {
    "version": 1,
    "entries":   [[skill, node, "key"], ...],  # 代码读取且静态可解析的三元组
    "dynamic_keys": ["key", ...],              # skill/node 为运行时变量、只能确定键名
    "unreferenced": [[skill, node, "key"], ...]# 当前 JSON 中存在但代码未精确读取的三元组
  }

保守策略（文档化限制）：
  * 只扫描 src/ 下的 .cpp/.hpp/.cc/.h 源文件，忽略 tests/。
  * 只解析字面量字符串键、`*Nodes::Name` 命名空间常量、文件内/同名字段
    `kSkillId`、全局唯一的 `constexpr uint32_t` 常量，以及静态机制绑定表
    （`std::array<*MechBinding, N>` 的 {node, "key", ...} 元素）。
  * 无法静态解析 skill/node 的调用不猜测：若键名是字面量则记入 dynamic_keys，
    否则完全跳过。这样 schema 永远不会包含猜测值。

用法：
  python scripts/gen_skill_mechanics_schema.py            # 重新生成 schema
  python scripts/gen_skill_mechanics_schema.py --check    # CI：校验磁盘 schema 是否最新
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SRC_DIR = ROOT / "src"
DEFAULT_MECHANICS = ROOT / "assets" / "data" / "skill_mechanics.json"
DEFAULT_SCHEMA = ROOT / "assets" / "data" / "skill_mechanics_schema.json"
SOURCE_SUFFIXES = {".cpp", ".hpp", ".cc", ".h"}

# ---------------------------------------------------------------- 源码处理工具

def strip_comments(text: str) -> str:
    """去除 // 与 /* */ 注释，但保留字符串/字符字面量内的内容。"""
    out: list[str] = []
    i, n = 0, len(text)
    in_str: str | None = None
    while i < n:
        c = text[i]
        if in_str is not None:
            out.append(c)
            if c == "\\" and i + 1 < n:
                out.append(text[i + 1])
                i += 2
                continue
            if c == in_str:
                in_str = None
            i += 1
            continue
        if c in ('"', "'"):
            in_str = c
            out.append(c)
            i += 1
            continue
        if c == "/" and i + 1 < n:
            if text[i + 1] == "/":
                j = text.find("\n", i)
                if j == -1:
                    break
                out.append("\n")
                i = j + 1
                continue
            if text[i + 1] == "*":
                j = text.find("*/", i + 2)
                if j == -1:
                    break
                out.append(" ")
                i = j + 2
                continue
        out.append(c)
        i += 1
    return "".join(out)


def match_brace(text: str, open_idx: int) -> int:
    """返回与 text[open_idx] 的 '{' 配对的 '}' 下标，找不到返回 -1。"""
    depth = 0
    i = open_idx
    in_str: str | None = None
    while i < len(text):
        c = text[i]
        if in_str is not None:
            if c == "\\":
                i += 2
                continue
            if c == in_str:
                in_str = None
        elif c in ('"', "'"):
            in_str = c
        elif c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


def split_top_level(text: str, separator: str = ",") -> list[str]:
    """按顶层（括号深度 0）分隔符切分，忽略嵌套与字面量内部。"""
    parts: list[str] = []
    cur: list[str] = []
    depth = 0
    in_str: str | None = None
    i = 0
    while i < len(text):
        c = text[i]
        if in_str is not None:
            cur.append(c)
            if c == "\\" and i + 1 < len(text):
                cur.append(text[i + 1])
                i += 2
                continue
            if c == in_str:
                in_str = None
            i += 1
            continue
        if c in ('"', "'"):
            in_str = c
            cur.append(c)
        elif c in "([{":
            depth += 1
            cur.append(c)
        elif c in ")]}":
            depth -= 1
            cur.append(c)
        elif c == separator and depth == 0:
            parts.append("".join(cur))
            cur = []
        else:
            cur.append(c)
        i += 1
    parts.append("".join(cur))
    return parts


INT_RE = re.compile(r"^(\d+)[uUlL]*$")
STRING_LITERAL_RE = re.compile(r'^"((?:[^"\\]|\\.)*)"$')
NODE_NS_RE = re.compile(r"\bnamespace\s+([A-Za-z_]\w*Nodes(?:Gen)?)\s*\{")
# 命名空间别名：`namespace SwordArrayNodes = SwordArrayNodesGen;`（A-01 生成式 SpecState 以别名重导出节点常量）。
NS_ALIAS_RE = re.compile(r"\bnamespace\s+([A-Za-z_]\w*)\s*=\s*([A-Za-z_]\w*)\s*;")
CONST_U32_RE = re.compile(
    r"(?:inline\s+|static\s+)?constexpr\s+uint32_t\s+([A-Za-z_]\w*)\s*=\s*([^;]+?)\s*;"
)
USING_NS_RE = re.compile(r"\busing\s+namespace\s+([A-Za-z_]\w*)\s*;")
STRUCT_RE = re.compile(r"\b(?:struct|class)\s+([A-Za-z_]\w*)")
MECH_ARRAY_RE = re.compile(
    r"constexpr\s+std::array\s*<\s*(\w*MechBinding)\s*,\s*\d+\s*>\s+(\w+)\s*=?\s*\{"
)
CALL_RE = re.compile(r"\b(GetMech|GetFloat|GetInt)\s*\(")
BINDING_LOOP_RE = re.compile(
    r"for\s*\(\s*const\s+auto\s*&\s*binding\s*:\s*(\w+)\s*\)"
)


def split_call_args(text: str, start: int) -> list[str] | None:
    """从 '(' 之后开始切分实参，返回实参列表；括号不闭合返回 None。"""
    depth = 0
    args: list[str] = []
    cur: list[str] = []
    i = start
    in_str: str | None = None
    while i < len(text):
        c = text[i]
        if in_str is not None:
            cur.append(c)
            if c == "\\" and i + 1 < len(text):
                cur.append(text[i + 1])
                i += 2
                continue
            if c == in_str:
                in_str = None
            i += 1
            continue
        if c in ('"', "'"):
            in_str = c
            cur.append(c)
        elif c == "(":
            depth += 1
            cur.append(c)
        elif c == ")":
            if depth == 0:
                args.append("".join(cur).strip())
                return args
            depth -= 1
            cur.append(c)
        elif c == "," and depth == 0:
            args.append("".join(cur).strip())
            cur = []
        else:
            cur.append(c)
        i += 1
    return None


class SourceFacts:
    """从 src/ 汇总得到的静态符号信息。"""

    def __init__(self) -> None:
        self.node_namespaces: dict[str, dict[str, int]] = {}
        self.namespace_aliases: dict[str, str] = {}
        self.const_exprs: dict[str, list[tuple[Path, str]]] = {}
        self.file_k_skill: dict[Path, list[str]] = {}
        self.class_k_skill: dict[str, list[str]] = {}
        self.file_using_namespaces: dict[Path, set[str]] = {}


def collect_facts(sources: dict[Path, str]) -> SourceFacts:
    facts = SourceFacts()
    for path, text in sources.items():
        # 节点命名空间：namespace XxxNodes { constexpr uint32_t Name = 值; }
        for m in NODE_NS_RE.finditer(text):
            end = match_brace(text, m.end() - 1)
            if end < 0:
                continue
            table = facts.node_namespaces.setdefault(m.group(1), {})
            for cm in CONST_U32_RE.finditer(text[m.end():end]):
                lm = INT_RE.match(cm.group(2).strip())
                if lm:
                    table[cm.group(1)] = int(lm.group(1))
        # 全文件 constexpr uint32_t 常量
        for m in CONST_U32_RE.finditer(text):
            facts.const_exprs.setdefault(m.group(1), []).append((path, m.group(2).strip()))
        # 本文件 using namespace XxxNodes;
        for m in USING_NS_RE.finditer(text):
            facts.file_using_namespaces.setdefault(path, set()).add(m.group(1))
        # 命名空间别名（A-01 生成式 SpecState 将节点常量重导出为既有命名空间名）
        for m in NS_ALIAS_RE.finditer(text):
            facts.namespace_aliases[m.group(1)] = m.group(2)
        # 结构体/类内的 static constexpr uint32_t kSkillId = ...;
        for m in STRUCT_RE.finditer(text):
            brace = text.find("{", m.end())
            if brace < 0:
                continue
            end = match_brace(text, brace)
            if end < 0:
                continue
            for cm in CONST_U32_RE.finditer(text[brace:end]):
                if cm.group(1) == "kSkillId":
                    facts.class_k_skill.setdefault(m.group(1), []).append(cm.group(2).strip())
    # 命名空间别名解析：把目标命名空间的节点表挂到别名下（目标可能后于别名所在文件被扫描）。
    for alias, target in facts.namespace_aliases.items():
        if alias not in facts.node_namespaces:
            table = facts.node_namespaces.get(target)
            if table is not None:
                facts.node_namespaces[alias] = table
    # kSkillId 的文件级解析：显式定义优先，否则回退到同名结构体（.cpp 实现 .hpp 结构体方法）
    for name, entries in facts.const_exprs.items():
        if name == "kSkillId":
            for path, expr in entries:
                facts.file_k_skill.setdefault(path, []).append(expr)
    for path in sources:
        if path not in facts.file_k_skill:
            fallback = facts.class_k_skill.get(path.stem)
            if fallback:
                facts.file_k_skill[path] = list(fallback)
    return facts


def collect_mech_arrays(sources: dict[Path, str]) -> dict[tuple[Path, str], list[tuple[str, str]]]:
    """解析静态机制绑定表：数组名 -> [(node 表达式, key 字面量), ...]。"""
    arrays: dict[tuple[Path, str], list[tuple[str, str]]] = {}
    for path, text in sources.items():
        for m in MECH_ARRAY_RE.finditer(text):
            brace = text.find("{", m.end() - 1)
            if brace < 0:
                continue
            end = match_brace(text, brace)
            if end < 0:
                continue
            body = text[brace + 1:end]
            inner = body.find("{")
            if inner < 0:
                continue
            inner_end = match_brace(body, inner)
            if inner_end < 0:
                continue
            entries: list[tuple[str, str]] = []
            for element in split_top_level(body[inner + 1:inner_end]):
                element = element.strip()
                if not element.startswith("{"):
                    continue
                fields = split_top_level(element[1:element.rfind("}")])
                fields = [f.strip() for f in fields]
                if len(fields) >= 2 and STRING_LITERAL_RE.match(fields[1]):
                    entries.append((fields[0], fields[1]))
            arrays[(path, m.group(2))] = entries
    return arrays


class Resolver:
    """把源码里的 skill/node 表达式尽量解析为整数；解析不了返回 None。"""

    def __init__(self, facts: SourceFacts) -> None:
        self.facts = facts

    def _unique_const(self, name: str, seen: set[str] | None = None) -> int | None:
        if seen is None:
            seen = set()
        if name in seen:
            return None
        seen.add(name)
        values: set[int] = set()
        for path, expr in self.facts.const_exprs.get(name, []):
            value = self._skill_expr(expr, path, seen)
            if value is not None:
                values.add(value)
        return next(iter(values)) if len(values) == 1 else None

    def _skill_expr(self, expr: str, path: Path, seen: set[str] | None = None) -> int | None:
        token = expr.strip()
        m = INT_RE.match(token)
        if m:
            return int(m.group(1))
        if "::" in token:
            qualifier, rest = token.rsplit("::", 1)
            if rest == "kSkillId":
                values: set[int] = set()
                for raw in self.facts.class_k_skill.get(qualifier, []):
                    value = self._skill_expr(raw, path, seen)
                    if value is not None:
                        values.add(value)
                if len(values) == 1:
                    return next(iter(values))
            return self._unique_const(rest, seen)
        if token == "kSkillId":
            values = set()
            for raw in self.facts.file_k_skill.get(path, []):
                value = self._skill_expr(raw, path, seen)
                if value is not None:
                    values.add(value)
            return next(iter(values)) if len(values) == 1 else None
        return self._unique_const(token, seen)

    def _node_expr(self, expr: str, path: Path, seen: set[str] | None = None) -> int | None:
        token = expr.strip()
        m = INT_RE.match(token)
        if m:
            return int(m.group(1))
        if "::" in token:
            qualifier, rest = token.rsplit("::", 1)
            table = self.facts.node_namespaces.get(qualifier)
            if table and rest in table:
                return table[rest]
            return None
        hits: set[int] = set()
        for ns in self.facts.file_using_namespaces.get(path, ()):
            table = self.facts.node_namespaces.get(ns, {})
            if token in table:
                hits.add(table[token])
        if len(hits) == 1:
            return next(iter(hits))
        if len(hits) > 1:
            return None
        return self._unique_const(token, seen)

    def skill(self, expr: str, path: Path) -> int | None:
        return self._skill_expr(expr, path)

    def node(self, expr: str, path: Path) -> int | None:
        return self._node_expr(expr, path)


def scan_code_reads(
    sources: dict[Path, str], facts: SourceFacts, arrays: dict[tuple[Path, str], list[tuple[str, str]]]
) -> tuple[set[tuple[int, int, str]], set[str]]:
    """返回 (静态可解析三元组集合, 只能确定键名的 dynamic 键集合)。"""
    resolver = Resolver(facts)
    tuples: set[tuple[int, int, str]] = set()
    dynamic: set[str] = set()
    for path, text in sources.items():
        for m in CALL_RE.finditer(text):
            args = split_call_args(text, m.end())
            if not args or len(args) < 3:
                continue
            skill_arg, node_arg, key_arg = args[0], args[1], args[2]
            literal = STRING_LITERAL_RE.match(key_arg)
            if literal:
                key = literal.group(1)
                skill = resolver.skill(skill_arg, path)
                node = resolver.node(node_arg, path)
                if skill is not None and node is not None:
                    tuples.add((skill, node, key))
                else:
                    # skill/node 为运行时变量：仍记录键名，避免加载期误报未知键。
                    dynamic.add(key)
            elif key_arg.replace(" ", "") == "binding.key":
                # 表驱动绑定：回溯最近的 for (const auto &binding : <数组>) 展开整张表。
                loops = BINDING_LOOP_RE.findall(text[:m.start()])
                array_name = loops[-1] if loops else None
                entries = arrays.get((path, array_name), [])
                skill = resolver.skill(skill_arg, path)
                if skill is None or not entries:
                    continue
                for node_expr, raw_key in entries:
                    node = resolver.node(node_expr, path)
                    km = STRING_LITERAL_RE.match(raw_key)
                    if km is None:
                        continue
                    if node is not None:
                        tuples.add((skill, node, km.group(1)))
                    else:
                        dynamic.add(km.group(1))
    return tuples, dynamic


def load_json_tuples(mechanics_path: Path) -> set[tuple[int, int, str]]:
    """读取机制表的全部叶子三元组（跳过 version/comment 元字段）。"""
    doc = json.loads(mechanics_path.read_text(encoding="utf-8"))
    if not isinstance(doc, dict):
        raise ValueError(f"{mechanics_path} 顶层必须是对象")
    result: set[tuple[int, int, str]] = set()
    for skill_key, skill_value in doc.items():
        if skill_key in ("version", "comment"):
            continue
        if not isinstance(skill_value, dict):
            continue
        for node_key, node_value in skill_value.items():
            if node_key == "comment" or not isinstance(node_value, dict):
                continue
            if not node_key.isdigit():
                continue
            for key, value in node_value.items():
                if not isinstance(value, (int, float)):
                    continue
                result.add((int(skill_key), int(node_key), key))
    return result


def build_manifest(src_dir: Path, mechanics_path: Path) -> tuple[dict, list[str]]:
    """构建 schema 字典，并返回覆盖率诊断信息（非致命）。"""
    sources: dict[Path, str] = {}
    for path in sorted(src_dir.rglob("*")):
        if path.suffix in SOURCE_SUFFIXES and path.is_file():
            sources[path] = strip_comments(path.read_text(encoding="utf-8", errors="replace"))
    facts = collect_facts(sources)
    arrays = collect_mech_arrays(sources)
    code_tuples, dynamic_keys = scan_code_reads(sources, facts, arrays)
    json_tuples = load_json_tuples(mechanics_path)

    diagnostics: list[str] = []
    missing_from_json = sorted(t for t in code_tuples if t not in json_tuples)
    if missing_from_json:
        diagnostics.append(
            f"{len(missing_from_json)} code-read tuple(s) missing from {mechanics_path.name}:"
        )
        diagnostics.extend(f"  {s}.{n}.{k}" for s, n, k in missing_from_json)

    manifest = {
        "version": 1,
        "comment": (
            "AUTO-GENERATED by scripts/gen_skill_mechanics_schema.py; do not edit by hand."
        ),
        "mechanics": mechanics_path.name,
        "entries": [[s, n, k] for s, n, k in sorted(code_tuples)],
        "dynamic_keys": sorted(dynamic_keys),
        "unreferenced": [
            [s, n, k] for s, n, k in sorted(json_tuples - code_tuples)
        ],
    }
    return manifest, diagnostics


def serialize(manifest: dict) -> str:
    return json.dumps(manifest, ensure_ascii=False, indent=2) + "\n"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", help="校验磁盘 schema 是否最新，不写文件")
    parser.add_argument("--src", type=Path, default=DEFAULT_SRC_DIR, help="源码根目录")
    parser.add_argument("--mechanics", type=Path, default=DEFAULT_MECHANICS, help="机制表 JSON")
    parser.add_argument("--schema", type=Path, default=DEFAULT_SCHEMA, help="schema 输出路径")
    args = parser.parse_args(argv)

    if not args.mechanics.is_file():
        print(f"[gen-schema] mechanics not found: {args.mechanics}", file=sys.stderr)
        return 2

    try:
        manifest, diagnostics = build_manifest(args.src, args.mechanics)
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"[gen-schema] failed to build schema: {exc}", file=sys.stderr)
        return 2

    for line in diagnostics:
        print(f"[gen-schema] WARN {line}", file=sys.stderr)

    text = serialize(manifest)
    if args.check:
        if not args.schema.is_file():
            print(f"[gen-schema] missing {args.schema}", file=sys.stderr)
            return 1
        on_disk = args.schema.read_text(encoding="utf-8")
        if on_disk != text:
            print(
                f"[gen-schema] OUTDATED: {args.schema} differs from generated output; "
                f"run `python scripts/gen_skill_mechanics_schema.py`",
                file=sys.stderr,
            )
            return 1
        print(
            f"[gen-schema] OK: {args.schema.name} up to date "
            f"({len(manifest['entries'])} entries, {len(manifest['unreferenced'])} unreferenced)"
        )
        return 0

    args.schema.write_text(text, encoding="utf-8", newline="\n")
    print(
        f"[gen-schema] wrote {args.schema} "
        f"({len(manifest['entries'])} entries, {len(manifest['dynamic_keys'])} dynamic, "
        f"{len(manifest['unreferenced'])} unreferenced)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
