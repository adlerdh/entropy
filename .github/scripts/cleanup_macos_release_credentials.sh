#!/usr/bin/env bash
set -euo pipefail

# Cleanup is also called if setup fails or the job is cancelled.
if [[ -z "${ENTROPY_MACOS_CREDENTIALS_DIR:-}" ]]; then
  exit 0
fi
: "${RUNNER_TEMP:?This script must run on a GitHub Actions macOS runner}"
runner_temp="$(cd "$RUNNER_TEMP" && pwd -P)"
credentials_dir="$ENTROPY_MACOS_CREDENTIALS_DIR"
[[ -e "$credentials_dir" || -L "$credentials_dir" ]] || exit 0

if [[ -L "$credentials_dir" || ! -d "$credentials_dir" ]]; then
  echo "Refusing to clean up a symlink or non-directory credential path" >&2
  exit 1
fi
credentials_dir="$(cd "$credentials_dir" && pwd -P)"
if [[ "$(dirname "$credentials_dir")" != "$runner_temp" ||
      "$(basename "$credentials_dir")" != entropy-release-credentials.* ]]; then
  echo "Refusing to clean up a credential directory outside the expected runner-temporary location" >&2
  exit 1
fi

cleanup_status=0
original_keychains=()
if [[ -f "$credentials_dir/original-keychains.txt" ]]; then
  while IFS= read -r entry; do
    entry="${entry#"${entry%%[![:space:]]*}"}"
    entry="${entry%"${entry##*[![:space:]]}"}"
    entry="${entry#\"}"
    entry="${entry%\"}"
    [[ -z "$entry" ]] || original_keychains+=("$entry")
  done < "$credentials_dir/original-keychains.txt"
  if ! security list-keychains -d user -s "${original_keychains[@]}"; then
    cleanup_status=1
  fi
fi

keychain_path="$credentials_dir/entropy-release.keychain-db"
if [[ -e "$keychain_path" ]]; then
  security lock-keychain "$keychain_path" || cleanup_status=1
  security delete-keychain "$keychain_path" || cleanup_status=1
fi
# Delete only our fixed filenames; do not recursively delete an environment-provided path.
rm -f -- "$credentials_dir/AuthKey.p8" "$credentials_dir/entropy-developer-id.p12" \
  "$credentials_dir/entropy-release.keychain-db" "$credentials_dir/entropy-release.keychain" \
  "$credentials_dir/original-keychains.txt"
rmdir -- "$credentials_dir" || cleanup_status=1

if [[ -n "${GITHUB_ENV:-}" ]]; then
  {
    echo "ENTROPY_NOTARY_API_KEY_PATH="
    echo "ENTROPY_MACOS_CREDENTIALS_DIR="
  } >> "$GITHUB_ENV"
fi
echo "Removed temporary macOS signing and notarization credentials"
exit "$cleanup_status"
