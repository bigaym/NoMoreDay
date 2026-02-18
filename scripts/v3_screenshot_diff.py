#!/usr/bin/env python3
"""Compare screenshot pairs using SSIM and pixel difference thresholds."""

from __future__ import annotations

import argparse
import json
import math
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


try:
    from PIL import Image
except ImportError:  # pragma: no cover - optional dependency
    Image = None


@dataclass
class ImageData:
    width: int
    height: int
    pixels: list[tuple[int, int, int]]


def _load_with_pillow(path: Path) -> ImageData:
    if Image is None:
        raise RuntimeError("Pillow is not available for non-PPM image loading")
    with Image.open(path) as img:
        rgb = img.convert("RGB")
        width, height = rgb.size
        pixels = list(rgb.getdata())
    return ImageData(width=width, height=height, pixels=pixels)


def _tokenize_ppm(raw: bytes) -> Iterable[str]:
    text = raw.decode("ascii", errors="strict")
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        for token in line.split():
            yield token


def _load_ppm(path: Path) -> ImageData:
    raw = path.read_bytes()
    tokens = list(_tokenize_ppm(raw))
    if len(tokens) < 4:
        raise ValueError(f"PPM parse failed: {path}")
    if tokens[0] != "P3":
        raise ValueError(f"Only ASCII P3 PPM is supported: {path}")
    width = int(tokens[1])
    height = int(tokens[2])
    max_value = int(tokens[3])
    if max_value <= 0:
        raise ValueError(f"Invalid max value in PPM: {path}")
    values = [int(v) for v in tokens[4:]]
    if len(values) != width * height * 3:
        raise ValueError(f"PPM size mismatch: {path}")
    scale = 255.0 / float(max_value)
    pixels: list[tuple[int, int, int]] = []
    for i in range(0, len(values), 3):
        r = int(round(values[i] * scale))
        g = int(round(values[i + 1] * scale))
        b = int(round(values[i + 2] * scale))
        pixels.append((r, g, b))
    return ImageData(width=width, height=height, pixels=pixels)


def load_image(path: Path) -> ImageData:
    suffix = path.suffix.lower()
    if suffix == ".ppm":
        return _load_ppm(path)
    return _load_with_pillow(path)


def to_gray(pixel: tuple[int, int, int]) -> float:
    r, g, b = pixel
    return (0.299 * float(r)) + (0.587 * float(g)) + (0.114 * float(b))


def compute_ssim(values_a: list[float], values_b: list[float]) -> float:
    if len(values_a) != len(values_b) or not values_a:
        return 0.0
    n = float(len(values_a))
    mean_a = sum(values_a) / n
    mean_b = sum(values_b) / n
    var_a = sum((x - mean_a) ** 2 for x in values_a) / n
    var_b = sum((x - mean_b) ** 2 for x in values_b) / n
    cov = sum((values_a[i] - mean_a) * (values_b[i] - mean_b)
              for i in range(len(values_a))) / n
    c1 = (0.01 * 255.0) ** 2
    c2 = (0.03 * 255.0) ** 2
    num = (2.0 * mean_a * mean_b + c1) * (2.0 * cov + c2)
    den = (mean_a ** 2 + mean_b ** 2 + c1) * (var_a + var_b + c2)
    if den <= 0.0:
        return 0.0
    return max(-1.0, min(1.0, num / den))


def save_diff_image(path: Path, baseline: ImageData, candidate: ImageData) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if Image is None:
        # Fallback to PPM if Pillow is unavailable.
        ppm_path = path.with_suffix(".ppm")
        lines = ["P3", f"{baseline.width} {baseline.height}", "255"]
        for i, base_pixel in enumerate(baseline.pixels):
            cur_pixel = candidate.pixels[i]
            if base_pixel != cur_pixel:
                lines.append("255 0 0")
            else:
                lines.append(f"{base_pixel[0]} {base_pixel[1]} {base_pixel[2]}")
        ppm_path.write_text("\n".join(lines), encoding="ascii")
        return

    diff_pixels = []
    for i, base_pixel in enumerate(baseline.pixels):
        cur_pixel = candidate.pixels[i]
        if base_pixel != cur_pixel:
            diff_pixels.append((255, 0, 0))
        else:
            diff_pixels.append(base_pixel)

    image = Image.new("RGB", (baseline.width, baseline.height))
    image.putdata(diff_pixels)
    image.save(path)


def compare_pair(
    baseline_path: Path,
    candidate_path: Path,
    diff_path: Path,
    ssim_min: float,
    pixel_diff_max_percent: float,
) -> dict[str, object]:
    baseline = load_image(baseline_path)
    candidate = load_image(candidate_path)
    if baseline.width != candidate.width or baseline.height != candidate.height:
        return {
            "status": "fail",
            "message": "image_size_mismatch",
            "ssim": 0.0,
            "pixelDiffPercent": 100.0,
        }

    total = baseline.width * baseline.height
    changed = 0
    gray_a: list[float] = []
    gray_b: list[float] = []
    gray_a_extend = gray_a.append
    gray_b_extend = gray_b.append
    for i, base_pixel in enumerate(baseline.pixels):
        cand_pixel = candidate.pixels[i]
        if base_pixel != cand_pixel:
            changed += 1
        gray_a_extend(to_gray(base_pixel))
        gray_b_extend(to_gray(cand_pixel))

    pixel_diff_percent = (float(changed) / float(total)) * 100.0
    ssim = compute_ssim(gray_a, gray_b)
    save_diff_image(diff_path, baseline, candidate)

    status = "pass"
    if ssim < ssim_min or pixel_diff_percent > pixel_diff_max_percent:
        status = "fail"
    return {
        "status": status,
        "message": "ok" if status == "pass" else "threshold_exceeded",
        "ssim": ssim,
        "pixelDiffPercent": pixel_diff_percent,
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Screenshot regression checker (SSIM + pixel diff)."
    )
    parser.add_argument(
        "--manifest",
        type=Path,
        default=Path("conductor/validation/v3_screenshot_manifest.json"),
        help="Screenshot manifest json path.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("bin/release_gate/screenshots/screenshot_report.json"),
        help="Output report file.",
    )
    parser.add_argument(
        "--allow-missing",
        action="store_true",
        help="Do not fail when baseline/candidate files are missing.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    thresholds = manifest.get("thresholds", {})
    ssim_min = float(thresholds.get("ssimMin", 0.95))
    pixel_diff_max = float(thresholds.get("pixelDiffMaxPercent", 2.0))

    report: dict[str, object] = {
        "version": str(manifest.get("version", "1.0.0")),
        "status": "pass",
        "allowMissing": args.allow_missing,
        "results": [],
        "summary": {"total": 0, "pass": 0, "fail": 0, "warning": 0},
    }

    failures = 0
    warnings = 0
    results = report["results"]
    assert isinstance(results, list)
    summary = report["summary"]
    assert isinstance(summary, dict)

    scenarios = manifest.get("scenarios", [])
    for item in scenarios:
        scenario = dict(item)
        scenario_id = str(scenario["id"])
        baseline = Path(str(scenario["baseline"]))
        candidate = Path(str(scenario["candidate"]))
        diff_path = Path(str(scenario["diff"]))
        entry: dict[str, object] = {"id": scenario_id}
        if not baseline.exists() or not candidate.exists():
            missing = []
            if not baseline.exists():
                missing.append("baseline")
            if not candidate.exists():
                missing.append("candidate")
            entry.update(
                {
                    "status": "warning" if args.allow_missing else "fail",
                    "message": f"missing_{'_'.join(missing)}",
                    "baseline": baseline.as_posix(),
                    "candidate": candidate.as_posix(),
                }
            )
            if args.allow_missing:
                warnings += 1
            else:
                failures += 1
        else:
            pair = compare_pair(
                baseline_path=baseline,
                candidate_path=candidate,
                diff_path=diff_path,
                ssim_min=ssim_min,
                pixel_diff_max_percent=pixel_diff_max,
            )
            entry.update(pair)
            entry["baseline"] = baseline.as_posix()
            entry["candidate"] = candidate.as_posix()
            entry["diff"] = diff_path.as_posix()
            if pair["status"] == "fail":
                failures += 1

        results.append(entry)

    total = len(results)
    passed = total - failures - warnings
    summary["total"] = total
    summary["pass"] = max(0, passed)
    summary["fail"] = failures
    summary["warning"] = warnings
    report["status"] = "fail" if failures > 0 else "pass"

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2), encoding="utf-8")

    print(
        f"[screenshot] total={total} pass={summary['pass']} "
        f"warning={warnings} fail={failures}"
    )
    return 1 if failures > 0 else 0


if __name__ == "__main__":
    raise SystemExit(main())
