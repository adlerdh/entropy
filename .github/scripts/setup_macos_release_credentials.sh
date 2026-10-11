#!/usr/bin/env bash
set -euo pipefail

: "${ENTROPY_MACOS_CERTIFICATE_P12_BASE64:?Missing Developer ID Application certificate secret}"
: "${ENTROPY_MACOS_CERTIFICATE_PASSWORD:?Missing certificate export password}"
: "${ENTROPY_MACOS_CODESIGN_IDENTITY:?Missing Developer ID Application identity}"
: "${ENTROPY_NOTARY_API_KEY_BASE64:?Missing App Store Connect API key secret}"
: "${ENTROPY_NOTARY_API_KEY_ID:?Missing App Store Connect API key ID}"
: "${ENTROPY_NOTARY_API_ISSUER_ID:?Missing App Store Connect team API issuer ID}"
: "${RUNNER_TEMP:?This script must run on a GitHub Actions macOS runner}"
: "${GITHUB_ENV:?This script must run on a GitHub Actions runner}"

umask 077
credentials_dir="$(mktemp -d "$RUNNER_TEMP/entropy-release-credentials.XXXXXX")"
export ENTROPY_MACOS_CREDENTIALS_DIR="$credentials_dir"
# Export the cleanup target before doing anything that might fail.
echo "ENTROPY_MACOS_CREDENTIALS_DIR=$credentials_dir" >> "$GITHUB_ENV"
trap 'bash "$(dirname "$0")/cleanup_macos_release_credentials.sh"' ERR

certificate_path="$credentials_dir/entropy-developer-id.p12"
keychain_path="$credentials_dir/entropy-release.keychain-db"
api_key_path="$credentials_dir/AuthKey.p8"
keychain_password="$(openssl rand -hex 24)"
echo "::add-mask::$keychain_password"
security list-keychains -d user > "$credentials_dir/original-keychains.txt"

printf '%s' "$ENTROPY_MACOS_CERTIFICATE_P12_BASE64" | base64 -D > "$certificate_path"
printf '%s' "$ENTROPY_NOTARY_API_KEY_BASE64" | base64 -D > "$api_key_path"
chmod 600 "$certificate_path" "$api_key_path"

security create-keychain -p "$keychain_password" "$keychain_path"
security set-keychain-settings -lut 21600 "$keychain_path"
security unlock-keychain -p "$keychain_password" "$keychain_path"
security import "$certificate_path" -k "$keychain_path" \
  -P "$ENTROPY_MACOS_CERTIFICATE_PASSWORD" -T /usr/bin/codesign
rm -f -- "$certificate_path"
security set-key-partition-list -S apple-tool:,apple:,codesign: \
  -s -k "$keychain_password" "$keychain_path"
security list-keychains -d user -s "$keychain_path"

if ! security find-identity -v -p codesigning "$keychain_path" | grep -Fq "$ENTROPY_MACOS_CODESIGN_IDENTITY"; then
  echo "The imported certificate does not contain the configured Developer ID Application identity and private key" >&2
  exit 1
fi

echo "ENTROPY_NOTARY_API_KEY_PATH=$api_key_path" >> "$GITHUB_ENV"
echo "Developer ID Application certificate and notarization API key are ready"
