#!/bin/bash
set -euo pipefail

app="$1"
destination="$2"
binary="$app/Contents/MacOS/Ping Pong"

# Only Apple system libraries may remain dynamically linked.
while IFS= read -r dependency; do
  case "$dependency" in
    /System/Library/*|/usr/lib/*) ;;
    *) echo "Non-system dependency in bundle: $dependency" >&2; exit 1 ;;
  esac
done < <(/usr/bin/otool -L "$binary" | /usr/bin/tail -n +2 | /usr/bin/awk '{print $1}')

mkdir -p "$destination"
/usr/bin/ditto "$app" "$destination/Ping Pong.app"
# Ad-hoc signing permits local execution; it is not Developer ID notarization.
/usr/bin/codesign --force --sign - "$destination/Ping Pong.app"
/usr/bin/codesign --verify --deep --strict "$destination/Ping Pong.app"
architecture=$(/usr/bin/uname -m)
/usr/bin/ditto -c -k --sequesterRsrc --keepParent \
  "$destination/Ping Pong.app" "$destination/Ping-Pong-macOS-$architecture.zip"
