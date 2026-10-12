#!/usr/bin/env python3
"""Check that a compiled macOS app icon contains the authored dark background."""

import json
import subprocess
import sys
from pathlib import Path


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit(f"Usage: {sys.argv[0]} PATH_TO_ASSETS.CAR")

    catalog = Path(sys.argv[1])
    if not catalog.is_file() or catalog.stat().st_size == 0:
        raise SystemExit(f"Missing compiled icon catalog: {catalog}")

    source = Path(__file__).resolve().parents[2] / "res/icons/macOS/Entropy.icon/icon.json"
    icon = json.loads(source.read_text(encoding="utf-8"))
    dark_fills = [
        item["value"]
        for item in icon["fill-specializations"]
        if item.get("appearance") == "dark"
    ]
    if dark_fills != ["system-dark"]:
        raise SystemExit("The icon source must use Apple's system-dark background")

    result = subprocess.run(
        ["xcrun", "assetutil", "--info", str(catalog)],
        check=True,
        capture_output=True,
        text=True,
    )
    assets = json.loads(result.stdout)
    dark_stacks = [
        item
        for item in assets
        if item.get("AssetType") == "IconImageStack"
        and item.get("Appearance") == "NSAppearanceNameDarkAqua"
        and item.get("Name") == "Entropy"
    ]
    if len(dark_stacks) != 1:
        raise SystemExit(f"Expected one Entropy dark icon stack, found {len(dark_stacks)}")

    background = dark_stacks[0]["Layers"][0].get("Name")
    if background != "Entropy_Assets/system-dark":
        raise SystemExit(f"Dark icon background {background} is not system-dark")

    print(f"Verified dark icon background in {catalog}")


if __name__ == "__main__":
    main()
