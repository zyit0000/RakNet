#!/bin/bash

REPO="zyit0000/RakNet"
DYLIB_NAME="GhostDesync.dylib"
TARGET_BIN="/Applications/Roblox.app/Contents/MacOS/RobloxPlayer"

if [ ! -f "$TARGET_BIN" ]; then
    echo "[!] Roblox not found. Run install.sh first."
    exit 1
fi

echo "[*] Updating GhostDesync dylib..."

# Delete old dylib if it exists and download fresh
[ -f "./$DYLIB_NAME" ] && rm "./$DYLIB_NAME"
curl -L -s "https://github.com/$REPO/releases/download/latest/$DYLIB_NAME" -o "./$DYLIB_NAME"

if [ ! -f "./$DYLIB_NAME" ]; then
    echo "[!] Download failed. Check your GitHub Release tag."
    exit 1
fi

echo "[+] Injecting and Launching..."
export DYLD_INSERT_LIBRARIES="$(pwd)/$DYLIB_NAME"
"$TARGET_BIN" &
