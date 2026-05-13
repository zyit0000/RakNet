#!/bin/bash

REPO="zyit0000/RakNet"
DYLIB_NAME="GhostDesync.dylib"
# Moving to /tmp helps bypass the App Sandbox "stat() errno=1" errors
SAFE_PATH="/tmp/$DYLIB_NAME"
TARGET_BIN="/Applications/Roblox.app/Contents/MacOS/RobloxPlayer"

if [ ! -f "$TARGET_BIN" ]; then
    echo "[!] Roblox not found. Run install.sh first."
    exit 1
fi

echo "[*] Updating GhostDesync dylib..."

# Delete old local and tmp dylibs
[ -f "./$DYLIB_NAME" ] && rm "./$DYLIB_NAME"
[ -f "$SAFE_PATH" ] && rm "$SAFE_PATH"

# Download fresh dylib
curl -L -s "https://github.com/$REPO/releases/download/latest/$DYLIB_NAME" -o "./$DYLIB_NAME"

if [ ! -f "./$DYLIB_NAME" ]; then
    echo "[!] Download failed. Check your GitHub Release tag."
    exit 1
fi

# --- FIXES START HERE ---

echo "[*] Applying macOS Security Fixes..."

# 1. Move to /tmp so the Roblox Sandbox can actually see the file
cp "./$DYLIB_NAME" "$SAFE_PATH"

# 2. Remove the Quarantine flag so macOS doesn't block it as "Downloaded"
xattr -d com.apple.quarantine "$SAFE_PATH" 2>/dev/null

# 3. Ad-Hoc sign the dylib so the system trusts the image
codesign --force --deep --sign - "$SAFE_PATH" 2>/dev/null

echo "[+] Injecting and Launching..."

# Point DYLD_INSERT_LIBRARIES to the safe /tmp path
export DYLD_INSERT_LIBRARIES="$SAFE_PATH"
"$TARGET_BIN" &

echo "[!] Done. Wait 13 seconds in-game for desync to activate."
