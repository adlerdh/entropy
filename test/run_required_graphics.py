#!/usr/bin/env python3
"""Run graphics coverage; unavailable graphics is a failure, never a green skip."""
import argparse
import pathlib
import subprocess
import sys
import xml.etree.ElementTree as ET

REQUIRED = (
    "all assembled GLSL programs compile and link offline",
    "mesh framebuffer and planar texture resources work in an OpenGL context",
    "render pass baseline restores mutable OpenGL state",
    "mesh depth-only passes do not upload surface-shading uniforms",
    "joint histogram shaders link and scatter in OpenGL 3.3",
    "Raycast acceleration matches analytic depth and preserves mesh depth handoff",
)


def coverage_failures(root, required):
    cases = {case.attrib["name"]: case for case in root.iter("testcase")}
    return {
        "Missing": [name for name in required if name not in cases],
        "Skipped": [name for name, case in cases.items() if case.find("skipped") is not None],
        "Failed": [name for name, case in cases.items()
                   if case.find("failure") is not None or case.find("error") is not None],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build_dir", type=pathlib.Path)
    parser.add_argument("--configuration", default="Debug")
    parser.add_argument("--display-transitions", action="store_true")
    args = parser.parse_args()

    required = REQUIRED + (("Physical monitor transitions update window scale and stable UI sizing",)
                           if args.display_transitions else ())
    
    report = args.build_dir.resolve() / "required-graphics.xml"
    report.unlink(missing_ok=True)

    # CTest processes each context separately. Serialization also avoids competing
    # for the display, driver state, and fixed-size graphics test fixtures.
    result = subprocess.run([
        "ctest", "--test-dir", str(args.build_dir), "-C", args.configuration,
        "--parallel", "1", "--timeout", "120", "--output-on-failure",
        "--output-junit", str(report), "-R", "^(" + "|".join(required) + ")$",
    ], check=False)

    if not report.exists():
        return 1
    
    failures = coverage_failures(ET.parse(report).getroot(), required)

    for category, names in failures.items():
        for name in names:
            print(f"{category} required graphics coverage: {name}", file=sys.stderr)

    return int(bool(result.returncode or any(failures.values())))


if __name__ == "__main__":
    sys.exit(main())
