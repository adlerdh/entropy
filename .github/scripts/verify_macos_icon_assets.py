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
    def icon_stacks(appearance: str) -> list[dict]:
        return [
            item
            for item in assets
            if item.get("AssetType") == "IconImageStack"
            and item.get("Appearance") == appearance
            and item.get("Name") == "Entropy"
        ]

    dark_stacks = icon_stacks("NSAppearanceNameDarkAqua")
    light_stacks = icon_stacks("NSAppearanceNameAqua")
    if len(dark_stacks) != 1 or len(light_stacks) != 1:
        variants = [
            (item.get("AssetType"), item.get("Appearance"), item.get("Name"))
            for item in assets
            if item.get("AssetType") in {"IconImageStack", "IconGroup"}
        ]
        raise SystemExit(
            f"Expected one Entropy light and dark icon stack, found "
            f"{len(light_stacks)} light and {len(dark_stacks)} dark; "
            f"available icon variants: {variants}"
        )

    dark_background = dark_stacks[0]["Layers"][0].get("Name")
    light_background = light_stacks[0]["Layers"][0].get("Name")
    if not dark_background or not light_background:
        raise SystemExit("A compiled icon stack is missing its background")

    # actool changes the system-dark gradient's asset name between Xcode versions.
    # The source above specifies the preset; the catalog must contain its dark variant.
    if dark_background == light_background:
        raise SystemExit("Light and dark icon stacks use the same background")

    print(
        f"Verified distinct light and dark icon backgrounds in {catalog}: "
        f"{light_background}, {dark_background}"
    )


if __name__ == "__main__":
    main()
