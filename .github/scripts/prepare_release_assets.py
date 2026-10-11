#!/usr/bin/env python3
"""Validate release assets and generate/verify their SHA-256 checksum manifest."""

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path


MANIFEST_NAME = "SHA256SUMS.txt"
PACKAGE_SUFFIXES = (
    "Fedora-x86_64-portable.tar.gz",
    "Fedora-x86_64.rpm",
    "macOS-arm64.dmg",
    "macOS-arm64.zip",
    "macOS-x86_64.dmg",
    "macOS-x86_64.zip",
    "source.tar.gz",
    "source.zip",
    "Ubuntu-x86_64-portable.tar.gz",
    "Ubuntu-x86_64.deb",
    "Windows-x86_64-portable.zip",
    "Windows-x86_64.msi",
)


def expected_asset_names(version):
    if not re.fullmatch(r"[0-9]+(?:\.[0-9]+){3}", version):
        raise ValueError("Release version must contain four numeric components")
    return sorted(f"Entropy-{version}-{suffix}" for suffix in PACKAGE_SUFFIXES)


def validate_assets(asset_dir, version, *, include_manifest=False):
    names = expected_asset_names(version)
    expected = set(names)
    if include_manifest:
        expected.add(MANIFEST_NAME)
    actual = {entry.name for entry in asset_dir.iterdir()}
    if actual != expected:
        raise ValueError(
            f"Unexpected release asset set; missing={sorted(expected - actual)}, "
            f"extra={sorted(actual - expected)}"
        )
    for name in sorted(expected):
        path = asset_dir / name
        if path.is_symlink() or not path.is_file() or path.stat().st_size == 0:
            raise ValueError(f"Release asset must be a nonempty regular file: {name}")
    return names


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def manifest_contents(asset_dir, names):
    # Basenames and LF line endings allow verification after downloading to any directory.
    return "".join(f"{sha256(asset_dir / name)}  {name}\n" for name in names)


def generate_manifest(asset_dir, version):
    names = validate_assets(asset_dir, version)
    contents = manifest_contents(asset_dir, names)
    (asset_dir / MANIFEST_NAME).write_text(contents, encoding="ascii", newline="\n")
    verify_manifest(asset_dir, version)


def verify_manifest(asset_dir, version):
    names = validate_assets(asset_dir, version, include_manifest=True)
    expected = manifest_contents(asset_dir, names).encode("ascii")
    if (asset_dir / MANIFEST_NAME).read_bytes() != expected:
        raise ValueError("SHA256SUMS.txt does not match the final release assets")


def verify_uploaded_assets(asset_dir, version, uploaded_assets):
    verify_manifest(asset_dir, version)
    names = expected_asset_names(version) + [MANIFEST_NAME]
    if not isinstance(uploaded_assets, list):
        raise ValueError("GitHub asset response must be a list")
    if len(uploaded_assets) != len(names):
        raise ValueError(f"Expected {len(names)} uploaded assets, found {len(uploaded_assets)}")
    by_name = {asset["name"]: asset for asset in uploaded_assets}
    if set(by_name) != set(names):
        raise ValueError("Uploaded release assets do not match the expected filenames")
    for name in names:
        uploaded = by_name[name]
        local_path = asset_dir / name
        if uploaded.get("state") != "uploaded":
            raise ValueError(f"Asset upload is incomplete: {name}")
        if uploaded.get("size") != local_path.stat().st_size:
            raise ValueError(f"Uploaded size mismatch: {name}")
        if uploaded.get("digest") != f"sha256:{sha256(local_path)}":
            raise ValueError(f"Uploaded SHA-256 digest mismatch: {name}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("generate", "verify", "verify-upload"))
    parser.add_argument("--version", required=True)
    parser.add_argument("--asset-dir", required=True, type=Path)
    parser.add_argument("--uploaded-assets-json", type=Path)
    args = parser.parse_args()
    if args.mode == "verify-upload" and args.uploaded_assets_json is None:
        parser.error("verify-upload requires --uploaded-assets-json")
    try:
        if args.mode == "generate":
            generate_manifest(args.asset_dir, args.version)
        elif args.mode == "verify":
            verify_manifest(args.asset_dir, args.version)
        else:
            uploaded_assets = json.loads(args.uploaded_assets_json.read_text(encoding="utf-8"))
            verify_uploaded_assets(args.asset_dir, args.version, uploaded_assets)
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f"Release asset validation failed: {error}", file=sys.stderr)
        return 1
    print(f"Release assets: {args.mode} succeeded for Entropy {args.version}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
