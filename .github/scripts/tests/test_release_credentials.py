"""Exercise macOS credential setup/cleanup with fake keys and a mocked security tool."""

import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


SCRIPTS = Path(__file__).resolve().parents[1]
GIT_BASH = Path("C:/Program Files/Git/bin/bash.exe")
BASH = str(GIT_BASH) if os.name == "nt" and GIT_BASH.exists() else shutil.which("bash")


@unittest.skipUnless(BASH, "Bash is required for shell credential tests")
class ReleaseCredentialTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.runner_temp = self.root / "runner"
        self.runner_temp.mkdir()
        self.bin_dir = self.root / "bin"
        self.bin_dir.mkdir()
        self.exports = self.root / "github-env"
        self.exports.touch()
        self.log = self.root / "security-log"
        self.env = dict(os.environ)
        self.env.update(
            RUNNER_TEMP=self.runner_temp.as_posix(),
            GITHUB_ENV=self.exports.as_posix(),
            MOCK_SECURITY_LOG=self.log.as_posix(),
            RELEASE_TEST_BIN=self.bin_dir.as_posix(),
            PATH=str(self.bin_dir) + os.pathsep + os.environ["PATH"],
            ENTROPY_MACOS_CERTIFICATE_P12_BASE64="ZmFrZSBjZXJ0aWZpY2F0ZQ==",
            ENTROPY_MACOS_CERTIFICATE_PASSWORD="fake-export-password",
            ENTROPY_MACOS_CODESIGN_IDENTITY="Developer ID Application: Test Developer (TESTTEAM)",
            ENTROPY_NOTARY_API_KEY_BASE64="ZmFrZSBhcGkga2V5",
            ENTROPY_NOTARY_API_KEY_ID="TESTKEY",
            ENTROPY_NOTARY_API_ISSUER_ID="test-issuer",
        )
        # Fixtures contain only dummy credentials. Never log import/create passwords.
        self.write_tool(
            "security",
            """#!/bin/bash
set -eu
printf '%s\\n' "$1" >> "$MOCK_SECURITY_LOG"
case "$1" in
  list-keychains)
    if [[ "${4:-}" == "-s" ]]; then
      shift 4
      printf 'restored:%s\\n' "$@" >> "$MOCK_SECURITY_LOG"
    else
      printf '%s\\n' '    "/tmp/Login Keychain"' '    "/tmp/Other Keychain"'
    fi
    ;;
  create-keychain) touch "${@: -1}" ;;
  import) [[ "${MOCK_FAIL_IMPORT:-0}" != "1" ]] ;;
  find-identity) printf '%s\\n' "${ENTROPY_MACOS_CODESIGN_IDENTITY}" ;;
  delete-keychain) rm -f -- "$2" ;;
esac
""",
        )
        # Linux/Git Bash use -d; the real setup script uses macOS's -D.
        self.write_tool(
            "base64",
            """#!/bin/bash
if [[ "${1:-}" == "-D" ]]; then shift; exec /usr/bin/base64 -d "$@"; fi
exec /usr/bin/base64 "$@"
""",
        )

    def write_tool(self, name, contents):
        path = self.bin_dir / name
        path.write_text(contents, encoding="utf-8", newline="\n")
        path.chmod(0o755)

    def run_script(self, name, env=None):
        return subprocess.run(
            [
                BASH,
                "-c",
                'bin_path="$RELEASE_TEST_BIN"; '
                'if command -v cygpath >/dev/null; then bin_path="$(cygpath -u "$bin_path")"; fi; '
                'export PATH="$bin_path:$PATH"; exec "$BASH" "$1"',
                "release-credential-test",
                str(SCRIPTS / name),
            ],
            env=self.env if env is None else env,
            capture_output=True,
            text=True,
        )

    def exported_environment(self):
        env = dict(self.env)
        for line in self.exports.read_text().splitlines():
            key, value = line.split("=", 1)
            env[key] = value
        return env

    def test_setup_then_cleanup_restores_search_list(self):
        setup = self.run_script("setup_macos_release_credentials.sh")
        self.assertEqual(setup.returncode, 0, setup.stderr)
        env = self.exported_environment()
        directory = Path(env["ENTROPY_MACOS_CREDENTIALS_DIR"])
        self.assertTrue(directory.is_dir())
        self.assertFalse((directory / "entropy-developer-id.p12").exists())
        self.assertEqual((directory / "AuthKey.p8").read_bytes(), b"fake api key")
        if os.name != "nt":
            self.assertEqual((directory / "AuthKey.p8").stat().st_mode & 0o777, 0o600)
        cleanup = self.run_script("cleanup_macos_release_credentials.sh", env)
        self.assertEqual(cleanup.returncode, 0, cleanup.stderr)
        self.assertFalse(directory.exists())
        log = self.log.read_text()
        self.assertIn("restored:/tmp/Login Keychain", log)
        self.assertIn("restored:/tmp/Other Keychain", log)
        self.assertIn("lock-keychain", log)
        self.assertIn("delete-keychain", log)
        self.assertNotIn("default-keychain", log)
        self.assertEqual(self.exported_environment()["ENTROPY_NOTARY_API_KEY_PATH"], "")
        # Cleanup can be called again after partial failure/cancellation.
        self.assertEqual(self.run_script("cleanup_macos_release_credentials.sh", env).returncode, 0)

    def test_setup_failure_removes_partial_credentials(self):
        self.env["MOCK_FAIL_IMPORT"] = "1"
        result = self.run_script("setup_macos_release_credentials.sh")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(list(self.runner_temp.iterdir()), [])

    def test_cleanup_without_setup_is_safe(self):
        self.assertEqual(self.run_script("cleanup_macos_release_credentials.sh").returncode, 0)
        self.assertFalse(self.log.exists())

    def test_workflow_builds_ad_hoc_before_loading_developer_id(self):
        workflow = (SCRIPTS.parent / "workflows/release.yml").read_text(encoding="utf-8")
        for job, following_job in (("macos-arm64", "macos-x86_64"), ("macos-x86_64", "fedora-x86_64")):
            with self.subTest(job=job):
                body = workflow.split(f"\n  {job}:\n", 1)[1].split(f"\n  {following_job}:\n", 1)[0]
                before, after = body.split("      - name: Install Developer ID certificate and notarization key", 1)
                self.assertIn("-DEntropy_MACOS_CODESIGN_IDENTITY=-", before)
                self.assertNotIn("${ENTROPY_MACOS_CODESIGN_IDENTITY}", before)
                self.assertIn("Configure Developer ID packaging", after)
                self.assertIn("${ENTROPY_MACOS_CODESIGN_IDENTITY}", after)
                self.assertLess(after.index("Configure Developer ID packaging"), after.index("Build DMG and ZIP"))

    def test_cleanup_refuses_outside_runner_temp(self):
        outside = self.root / "entropy-release-credentials.outside"
        outside.mkdir()
        key = outside / "AuthKey.p8"
        key.write_bytes(b"must not be deleted")
        self.env["ENTROPY_MACOS_CREDENTIALS_DIR"] = outside.as_posix()
        result = self.run_script("cleanup_macos_release_credentials.sh")
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue(key.exists())

    def test_cleanup_refuses_runner_temp_itself(self):
        key = self.runner_temp / "AuthKey.p8"
        key.write_bytes(b"must not be deleted")
        self.env["ENTROPY_MACOS_CREDENTIALS_DIR"] = self.runner_temp.as_posix()
        result = self.run_script("cleanup_macos_release_credentials.sh")
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue(key.exists())

    def test_cleanup_refuses_unexpected_directory_name(self):
        other = self.runner_temp / "unrelated-directory"
        other.mkdir()
        key = other / "AuthKey.p8"
        key.write_bytes(b"must not be deleted")
        self.env["ENTROPY_MACOS_CREDENTIALS_DIR"] = other.as_posix()
        result = self.run_script("cleanup_macos_release_credentials.sh")
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue(key.exists())

    def test_cleanup_refuses_symlink_directory(self):
        outside = self.root / "outside"
        outside.mkdir()
        key = outside / "AuthKey.p8"
        key.write_bytes(b"must not be deleted")
        link = self.runner_temp / "entropy-release-credentials.symlink"
        try:
            link.symlink_to(outside, target_is_directory=True)
        except OSError:
            self.skipTest("Creating symlinks requires Windows Developer Mode or elevation")
        self.env["ENTROPY_MACOS_CREDENTIALS_DIR"] = link.as_posix()
        result = self.run_script("cleanup_macos_release_credentials.sh")
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue(key.exists())


if __name__ == "__main__":
    unittest.main()
