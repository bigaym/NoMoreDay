#!/usr/bin/env python3
import argparse
import difflib
import json
import time
from pathlib import Path
from typing import Any


def load_sequence(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8-sig") as f:
        return json.load(f)


def dump_canonical(document: dict[str, Any]) -> str:
    return json.dumps(document, sort_keys=True, indent=2, ensure_ascii=False)


def render_timeline(sequence: dict[str, Any], cursor_time: float, width: int = 72) -> str:
    duration = max(float(sequence.get("duration", 1.0)), 0.001)
    events = sequence.get("events", [])
    track = ["-"] * width
    cursor_col = min(width - 1, int((cursor_time / duration) * (width - 1)))
    track[cursor_col] = "|"
    for event in events:
        event_time = float(event.get("time", 0.0))
        col = min(width - 1, max(0, int((event_time / duration) * (width - 1))))
        track[col] = "*"
    return "".join(track)


def estimate_budget(sequence: dict[str, Any], threshold: float) -> tuple[float, bool]:
    total = 0.0
    for event in sequence.get("events", []):
        event_type = str(event.get("type", "")).lower()
        params = event.get("params", {})
        if event_type == "particle":
            total += (
                max(1.0, float(params.get("count", 1)))
                * max(0.05, float(params.get("lifetime", 0.1)))
                * (1.0 + max(0.0, float(params.get("speed", 0.0))) / 240.0)
            )
        elif event_type == "light":
            total += (
                max(0.0, float(params.get("radius", 0.0)))
                * max(0.0, float(params.get("intensity", 0.0)))
                * max(0.05, float(params.get("duration", 0.1)))
                * 0.05
            )
        elif event_type == "shadowpulse" or event_type == "shadow_pulse":
            total += max(0.05, float(params.get("duration", 0.1))) * 50.0
        elif event_type == "materialphaseshift" or event_type == "material_phase_shift":
            total += max(0.05, float(params.get("duration", 0.1))) * 45.0
        elif event_type == "lightprofileblend" or event_type == "light_profile_blend":
            total += max(0.05, float(params.get("blendTime", 0.1))) * 20.0
    return total, total > threshold


def main() -> int:
    parser = argparse.ArgumentParser(
        description="V3 VFX preview helper: timeline + hot-reload diff + tier switch"
    )
    parser.add_argument("file", type=Path, help="Path to a VFX json file")
    parser.add_argument("--tier", default="High", choices=["Low", "Medium", "High", "Ultra"])
    parser.add_argument("--budget-threshold", type=float, default=1200.0)
    parser.add_argument(
        "--hot-reload-check",
        type=Path,
        help="Non-interactive: compare current file with updated file and emit diff",
    )
    parser.add_argument(
        "--diff-out",
        type=Path,
        help="Optional output file for --hot-reload-check diff text",
    )
    args = parser.parse_args()

    sequence_path = args.file
    if not sequence_path.exists():
        print(f"[preview] missing file: {sequence_path}")
        return 1

    current = load_sequence(sequence_path)
    previous_dump = dump_canonical(current)
    last_mtime = sequence_path.stat().st_mtime

    if args.hot_reload_check is not None:
        updated_path = args.hot_reload_check
        if not updated_path.exists():
            print(f"[preview] missing updated file: {updated_path}")
            return 1
        updated = load_sequence(updated_path)
        updated_dump = dump_canonical(updated)
        diff_lines = list(
            difflib.unified_diff(
                previous_dump.splitlines(),
                updated_dump.splitlines(),
                fromfile="before",
                tofile="after",
                lineterm="",
            )
        )
        text = "\n".join(diff_lines)
        if args.diff_out is not None:
            args.diff_out.parent.mkdir(parents=True, exist_ok=True)
            args.diff_out.write_text(text, encoding="utf-8")
        if text:
            print(text)
            return 0
        print("[preview] no diff detected")
        return 2

    playing = True
    current_time = 0.0
    tier = args.tier
    last_frame = time.perf_counter()

    print("[preview] controls: Enter=next, p=play/pause, seek <sec>, tier <name>, q=quit")
    while True:
        now = time.perf_counter()
        dt = max(0.0, now - last_frame)
        last_frame = now

        if playing:
            duration = max(float(current.get("duration", 1.0)), 0.001)
            current_time += dt
            if current_time > duration:
                current_time = duration
                playing = False

        new_mtime = sequence_path.stat().st_mtime
        if new_mtime > last_mtime:
            last_mtime = new_mtime
            try:
                reloaded = load_sequence(sequence_path)
                new_dump = dump_canonical(reloaded)
                diff = list(
                    difflib.unified_diff(
                        previous_dump.splitlines(),
                        new_dump.splitlines(),
                        fromfile="before",
                        tofile="after",
                        lineterm="",
                    )
                )
                current = reloaded
                previous_dump = new_dump
                print("[preview] hot-reload diff:")
                if diff:
                    for line in diff[:60]:
                        print(line)
                else:
                    print("(no textual diff)")
            except Exception as exc:  # noqa: BLE001
                print(f"[preview] reload failed: {exc}")

        timeline = render_timeline(current, current_time)
        total_budget, over_budget = estimate_budget(current, args.budget_threshold)
        print(
            f"[preview] tier={tier} playing={playing} time={current_time:.3f}s "
            f"budget={total_budget:.2f}/{args.budget_threshold:.2f} over={over_budget}"
        )
        print(f"[preview] timeline: {timeline}")

        cmd = input("> ").strip()
        if cmd == "":
            continue
        if cmd == "q":
            return 0
        if cmd == "p":
            playing = not playing
            continue
        if cmd.startswith("seek "):
            try:
                value = float(cmd.split(" ", 1)[1].strip())
                duration = max(float(current.get("duration", 1.0)), 0.001)
                current_time = min(max(0.0, value), duration)
            except ValueError:
                print("[preview] invalid seek value")
            continue
        if cmd.startswith("tier "):
            selected = cmd.split(" ", 1)[1].strip()
            if selected in {"Low", "Medium", "High", "Ultra"}:
                tier = selected
            else:
                print("[preview] invalid tier")
            continue
        print("[preview] unknown command")


if __name__ == "__main__":
    raise SystemExit(main())
