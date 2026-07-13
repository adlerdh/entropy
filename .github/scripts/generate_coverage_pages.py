#!/usr/bin/env python3

import argparse
import html
import json
from pathlib import Path


def coverage_color(percent: float) -> str:
    if percent >= 90.0:
        return "#2ea043"
    if percent >= 75.0:
        return "#6f9f2f"
    if percent >= 60.0:
        return "#d29922"
    return "#cf222e"


def read_line_percent(summary_path: Path) -> float:
    with summary_path.open("r", encoding="utf-8") as summary_file:
        summary = json.load(summary_file)

    candidates = [
        summary.get("line_percent"),
        summary.get("root", {}).get("line_percent") if isinstance(summary.get("root"), dict) else None,
    ]
    for candidate in candidates:
        if candidate is not None:
            return float(candidate)

    raise ValueError(f"Could not find line_percent in {summary_path}")


def write_badge(path: Path, percent: float) -> None:
    label = "coverage"
    value = f"{percent:.1f}%"
    label_width = 68
    value_width = max(48, 10 + len(value) * 7)
    total_width = label_width + value_width
    color = coverage_color(percent)

    path.write_text(
        f"""<svg xmlns="http://www.w3.org/2000/svg" width="{total_width}" height="20" role="img" aria-label="{label}: {value}">
  <title>{label}: {value}</title>
  <linearGradient id="s" x2="0" y2="100%">
    <stop offset="0" stop-color="#bbb" stop-opacity=".1"/>
    <stop offset="1" stop-opacity=".1"/>
  </linearGradient>
  <clipPath id="r">
    <rect width="{total_width}" height="20" rx="3" fill="#fff"/>
  </clipPath>
  <g clip-path="url(#r)">
    <rect width="{label_width}" height="20" fill="#555"/>
    <rect x="{label_width}" width="{value_width}" height="20" fill="{color}"/>
    <rect width="{total_width}" height="20" fill="url(#s)"/>
  </g>
  <g fill="#fff" text-anchor="middle" font-family="Verdana,Geneva,DejaVu Sans,sans-serif" font-size="11">
    <text x="{label_width / 2}" y="15" fill="#010101" fill-opacity=".3">{html.escape(label)}</text>
    <text x="{label_width / 2}" y="14">{html.escape(label)}</text>
    <text x="{label_width + value_width / 2}" y="15" fill="#010101" fill-opacity=".3">{html.escape(value)}</text>
    <text x="{label_width + value_width / 2}" y="14">{html.escape(value)}</text>
  </g>
</svg>
""",
        encoding="utf-8",
    )


def write_index(path: Path, percent: float) -> None:
    path.write_text(
        f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta http-equiv="refresh" content="0; url=html/index.html">
  <title>Entropy Coverage</title>
</head>
<body>
  <p>Entropy line coverage: {percent:.1f}%</p>
  <p><a href="html/index.html">Open the HTML coverage report</a></p>
</body>
</html>
""",
        encoding="utf-8",
    )


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate static coverage pages and badge files.")
    parser.add_argument("--summary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    percent = read_line_percent(args.summary)
    args.output.mkdir(parents=True, exist_ok=True)
    write_badge(args.output / "badge.svg", percent)
    write_index(args.output / "index.html", percent)
    (args.output / "summary.json").write_text(args.summary.read_text(encoding="utf-8"), encoding="utf-8")


if __name__ == "__main__":
    main()
