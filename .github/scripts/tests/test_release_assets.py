"""Offline regression tests for the release checksum and upload gate."""

import copy
import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "prepare_release_assets.py"
SPEC = importlib.util.spec_from_file_location("release_assets", SCRIPT)
assets = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(assets)
VERSION = "0.9.9.8"


class ReleaseAssetsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.names = assets.expected_asset_names(VERSION)
        for name in self.names:
            (self.directory / name).write_bytes(f"Test package: {name}\n".encode("ascii"))

    def uploaded_assets(self):
        return [
            {
                "name": name,
                "state": "uploaded",
                "size": (self.directory / name).stat().st_size,
                "digest": f"sha256:{assets.sha256(self.directory / name)}",
            }
            for name in self.names + [assets.MANIFEST_NAME]
        ]

    def test_manifest_covers_all_platforms_and_source_archives(self):
        assets.generate_manifest(self.directory, VERSION)
        contents = (self.directory / assets.MANIFEST_NAME).read_bytes()
        self.assertNotIn(b"\r", contents)
        lines = contents.decode("ascii").splitlines()
        self.assertEqual([line.split("  ")[1] for line in lines], self.names)
        self.assertEqual(len(lines), 12)
        for label in ("Windows", "Ubuntu", "Fedora", "macOS-arm64", "macOS-x86_64", "source"):
            self.assertEqual(sum(label in name for name in self.names), 2)
        assets.verify_manifest(self.directory, VERSION)

    def test_known_sha256(self):
        path = self.directory / self.names[0]
        path.write_bytes(b"abc")
        self.assertEqual(
            assets.sha256(path),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        )

    def test_invalid_version_is_rejected(self):
        for version in ("0.9.9", "v0.9.9.8", "0.9.9.8/../", "0.9.9.8\n"):
            with self.subTest(version=version), self.assertRaises(ValueError):
                assets.expected_asset_names(version)

    def test_missing_asset_is_rejected(self):
        (self.directory / self.names[0]).unlink()
        with self.assertRaises(ValueError):
            assets.generate_manifest(self.directory, VERSION)

    def test_wrong_twelve_files_are_rejected(self):
        (self.directory / self.names[0]).rename(self.directory / "unexpected-package.zip")
        with self.assertRaises(ValueError):
            assets.generate_manifest(self.directory, VERSION)
        self.assertFalse((self.directory / assets.MANIFEST_NAME).exists())

    def test_extra_asset_is_rejected(self):
        (self.directory / "unexpected-file").write_bytes(b"extra")
        with self.assertRaises(ValueError):
            assets.generate_manifest(self.directory, VERSION)

    def test_extra_directory_is_rejected(self):
        (self.directory / "nested-artifacts").mkdir()
        with self.assertRaises(ValueError):
            assets.generate_manifest(self.directory, VERSION)

    def test_directory_named_like_asset_is_rejected(self):
        path = self.directory / self.names[0]
        path.unlink()
        path.mkdir()
        with self.assertRaises(ValueError):
            assets.generate_manifest(self.directory, VERSION)

    def test_empty_asset_is_rejected(self):
        (self.directory / self.names[0]).write_bytes(b"")
        with self.assertRaises(ValueError):
            assets.generate_manifest(self.directory, VERSION)

    def test_symlink_asset_is_rejected(self):
        target = self.directory / self.names[1]
        path = self.directory / self.names[0]
        path.unlink()
        try:
            path.symlink_to(target)
        except OSError:
            self.skipTest("Creating symlinks requires Windows Developer Mode or elevation")
        with self.assertRaises(ValueError):
            assets.generate_manifest(self.directory, VERSION)

    def test_changed_asset_is_rejected(self):
        assets.generate_manifest(self.directory, VERSION)
        (self.directory / self.names[0]).write_bytes(b"tampered")
        with self.assertRaises(ValueError):
            assets.verify_manifest(self.directory, VERSION)

    def test_changed_manifest_is_rejected(self):
        assets.generate_manifest(self.directory, VERSION)
        path = self.directory / assets.MANIFEST_NAME
        path.write_bytes(path.read_bytes().replace(b"  Entropy", b" *Entropy", 1))
        with self.assertRaises(ValueError):
            assets.verify_manifest(self.directory, VERSION)

    def test_valid_uploaded_assets_include_manifest_digest(self):
        assets.generate_manifest(self.directory, VERSION)
        assets.verify_uploaded_assets(self.directory, VERSION, self.uploaded_assets())

    def test_uploaded_assets_fail_closed(self):
        assets.generate_manifest(self.directory, VERSION)
        for field, value in (("name", "wrong.zip"), ("state", "starter"), ("size", 0), ("digest", "sha256:wrong")):
            with self.subTest(field=field):
                uploaded = self.uploaded_assets()
                uploaded[0][field] = value
                with self.assertRaises(ValueError):
                    assets.verify_uploaded_assets(self.directory, VERSION, uploaded)

    def test_missing_uploaded_manifest_is_rejected(self):
        assets.generate_manifest(self.directory, VERSION)
        with self.assertRaises(ValueError):
            assets.verify_uploaded_assets(self.directory, VERSION, self.uploaded_assets()[:-1])

    def test_duplicate_uploaded_asset_is_rejected(self):
        assets.generate_manifest(self.directory, VERSION)
        uploaded = self.uploaded_assets()
        uploaded[-1] = copy.deepcopy(uploaded[0])
        with self.assertRaises(ValueError):
            assets.verify_uploaded_assets(self.directory, VERSION, uploaded)

    def test_missing_uploaded_digest_is_rejected(self):
        assets.generate_manifest(self.directory, VERSION)
        uploaded = self.uploaded_assets()
        del uploaded[-1]["digest"]
        with self.assertRaises(ValueError):
            assets.verify_uploaded_assets(self.directory, VERSION, uploaded)

    def test_command_line_round_trip(self):
        command = [sys.executable, str(SCRIPT)]
        options = ["--version", VERSION, "--asset-dir", str(self.directory)]
        for mode in ("generate", "verify"):
            result = subprocess.run(command + [mode] + options, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
        with tempfile.TemporaryDirectory() as response_dir:
            response = Path(response_dir) / "assets.json"
            response.write_text(json.dumps(self.uploaded_assets()), encoding="utf-8")
            result = subprocess.run(
                command + ["verify-upload"] + options + ["--uploaded-assets-json", str(response)],
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
        (self.directory / self.names[0]).write_bytes(b"changed after hashing")
        result = subprocess.run(command + ["verify"] + options, capture_output=True, text=True)
        self.assertEqual(result.returncode, 1)


if __name__ == "__main__":
    unittest.main()
