#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "Usage: $0 path/to/Entropy-<version>-macOS-<arch>.zip path/to/Entropy-<version>-macOS-<arch>.dmg" >&2
  exit 2
fi

zip_path="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
dmg_path="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"
[[ -f "$zip_path" && -f "$dmg_path" ]] || { echo "Both CPack packages must exist" >&2; exit 1; }

notary_args=()
if [[ -n "${ENTROPY_NOTARY_KEYCHAIN_PROFILE:-}" ]]; then
  notary_args=(--keychain-profile "$ENTROPY_NOTARY_KEYCHAIN_PROFILE")
elif [[ -n "${ENTROPY_NOTARY_API_KEY_PATH:-}" && -n "${ENTROPY_NOTARY_API_KEY_ID:-}" && -n "${ENTROPY_NOTARY_API_ISSUER_ID:-}" ]]; then
  notary_args=(--key "$ENTROPY_NOTARY_API_KEY_PATH" --key-id "$ENTROPY_NOTARY_API_KEY_ID" --issuer "$ENTROPY_NOTARY_API_ISSUER_ID")
else
  echo "Set ENTROPY_NOTARY_KEYCHAIN_PROFILE or an App Store Connect team API key path, key ID, and issuer ID" >&2
  exit 1
fi

work_dir="$(mktemp -d)"
trap 'rm -rf "$work_dir"' EXIT
archive_root="${zip_path%.zip}"
archive_name="$(basename "$archive_root")"
ditto -x -k "$zip_path" "$work_dir/extracted"
app_path="$work_dir/extracted/$archive_name/Entropy.app"
[[ -d "$app_path" ]] || { echo "Expected $archive_name/Entropy.app in ZIP" >&2; exit 1; }

codesign --verify --deep --strict --verbose=2 "$app_path"
signing_details="$(codesign --display --verbose=4 "$app_path" 2>&1)"
[[ "$signing_details" == *"runtime"* && "$signing_details" == *"TeamIdentifier="* && "$signing_details" != *"TeamIdentifier=not set"* ]] || {
  echo "The app must have a Developer ID signature and hardened runtime" >&2
  exit 1
}

submit_and_check() {
  local archive="$1" response status submission_id
  response="$(xcrun notarytool submit "$archive" "${notary_args[@]}" --wait --output-format json)"
  status="$(printf '%s' "$response" | /usr/bin/python3 -c 'import json,sys; print(json.load(sys.stdin)["status"])')"
  submission_id="$(printf '%s' "$response" | /usr/bin/python3 -c 'import json,sys; print(json.load(sys.stdin)["id"])')"
  echo "Notarization of $(basename "$archive"): $status ($submission_id)"
  xcrun notarytool log "$submission_id" "${notary_args[@]}" "$work_dir/$(basename "$archive").notary-log.json"
  cat "$work_dir/$(basename "$archive").notary-log.json"
  if [[ "$status" != "Accepted" ]]; then
    exit 1
  fi
}

# The ZIP is a transport for notarizing the app. ZIP files cannot be stapled.
submit_and_check "$zip_path"
xcrun stapler staple "$app_path"
xcrun stapler validate "$app_path"
codesign --verify --deep --strict --verbose=2 "$app_path"
spctl --assess --type execute --verbose=4 "$app_path"

stapled_zip="$work_dir/$archive_name.zip"
ditto -c -k --keepParent "$work_dir/extracted/$archive_name" "$stapled_zip"
ditto -x -k "$stapled_zip" "$work_dir/final-zip-check"
zip_app="$work_dir/final-zip-check/$archive_name/Entropy.app"
codesign --verify --deep --strict --verbose=2 "$zip_app"
xcrun stapler validate "$zip_app"

# Build the DMG from that same stapled app, preserving CPack's drag-to-Applications layout.
volume_root="$work_dir/extracted/$archive_name"
if [[ ! -e "$volume_root/Applications" && ! -L "$volume_root/Applications" ]]; then
  ln -s /Applications "$volume_root/Applications"
fi
short_version="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$app_path/Contents/Info.plist")"
stapled_dmg="$work_dir/$archive_name.dmg"
hdiutil create -volname "Entropy $short_version" -srcfolder "$volume_root" -format UDZO "$stapled_dmg"
submit_and_check "$stapled_dmg"
xcrun stapler staple "$stapled_dmg"
xcrun stapler validate "$stapled_dmg"
hdiutil verify "$stapled_dmg"

mv -f "$stapled_zip" "$zip_path"
mv -f "$stapled_dmg" "$dmg_path"
echo "Ready for release: $zip_path and $dmg_path"
