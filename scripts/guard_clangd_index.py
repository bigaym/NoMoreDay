from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path


def filter_compile_commands(src: Path, dst: Path) -> tuple[int, int]:
    data = json.loads(src.read_text(encoding="utf-8"))
    kept = []
    removed = 0
    for entry in data:
        file_path = str(entry.get("file", "")).lower().replace("\\", "/")

        if file_path.endswith(".rc"):
            removed += 1
            continue
        if file_path.endswith("cmake_pch.cxx"):
            removed += 1
            continue
        if "/build_clangd_index/" in file_path:
            removed += 1
            continue
        if "/third_party/spdlog/src/spdlog.cpp" in file_path:
            removed += 1
            continue

        kept.append(entry)

    dst.write_text(json.dumps(kept, ensure_ascii=False, indent=2), encoding="utf-8")
    return len(data), len(kept)


def repair_kind_lang_lines(index_path: Path) -> int:
    text = index_path.read_text(encoding="utf-8", errors="ignore")
    fixed_lines: list[str] = []
    fixes = 0
    pattern = re.compile(r"^(\s*)Kind:\s*(.*?)\s+Lang:\s*(\S.*?)\s*$")
    for line in text.splitlines():
        match = pattern.match(line)
        if match:
            indent, kind, lang = match.groups()
            fixed_lines.append(f"{indent}Kind:            {kind.strip()}")
            fixed_lines.append(f"{indent}Lang:            {lang.strip()}")
            fixes += 1
        else:
            fixed_lines.append(line)

    if fixes > 0:
        index_path.write_text("\n".join(fixed_lines) + "\n", encoding="utf-8")
    return fixes


def count_malformed_kind_lang(index_path: Path) -> int:
    text = index_path.read_text(encoding="utf-8", errors="ignore")
    return sum(1 for line in text.splitlines() if "Kind:" in line and "Lang:" in line)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Guarded clangd-indexer runner for Windows toolchain quirks."
    )
    parser.add_argument(
        "--indexer", required=True, help="Path to clangd-indexer executable"
    )
    parser.add_argument(
        "--compile-commands", required=True, help="Path to compile_commands.json"
    )
    parser.add_argument("--output", required=True, help="Output index.yaml path")
    args = parser.parse_args()

    indexer = Path(args.indexer)
    compile_commands = Path(args.compile_commands)
    output = Path(args.output)

    if not indexer.exists():
        print(f"[Index] ERROR: indexer not found: {indexer}")
        return 1
    if not compile_commands.exists():
        print(f"[Index] ERROR: compile_commands not found: {compile_commands}")
        return 1

    filtered = output.with_suffix(".filtered_compile_commands.json")
    total, kept = filter_compile_commands(compile_commands, filtered)
    print(f"[Index] Filtered compile_commands: {total} -> {kept}")

    command = [
        str(indexer),
        "--executor=all-TUs",
        "--execute-concurrency=1",
        "--format=yaml",
        str(filtered),
    ]

    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", encoding="utf-8", newline="\n") as stream:
        result = subprocess.run(command, stdout=stream)

    if result.returncode != 0:
        print(
            f"[Index] WARNING: clangd-indexer exited with code {result.returncode}. Continuing with guard checks."
        )

    if not output.exists() or output.stat().st_size == 0:
        print("[Index] ERROR: index.yaml was not generated.")
        return 1

    before = count_malformed_kind_lang(output)
    fixes = repair_kind_lang_lines(output)
    after = count_malformed_kind_lang(output)
    print(
        f"[Index] Malformed Kind/Lang lines: before={before}, fixed={fixes}, after={after}"
    )

    if after > 0:
        print(
            "[Index] ERROR: index.yaml still has malformed Kind/Lang lines after repair."
        )
        return 1

    print(f"[Index] Wrote guarded index: {output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
