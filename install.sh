#!/bin/bash

REPO="ZYiT0/YourRepoName"
DYLIB_NAME="GhostDesync.dylib"
TARGET_DIR="/Applications/Roblox.app/Contents/MacOS"

echo "[*] Starting Full Installation..."

# 1. Install/Update Roblox
echo "[*] Fetching latest Roblox..."
json=$(curl -s "https://clientsettingscdn.roblox.com/v2/client-version/MacPlayer")
version=$(echo "$json" | grep -o '"clientVersionUpload":"[^"]*' | grep -o '[^"]*$')

# Clean old Roblox install
[ -d "/Applications/Roblox.app" ] && sudo rm -rf "/Applications/Roblox.app"

curl -L "http://setup.rbxcdn.com/mac/$version-RobloxPlayer.zip" -o "./RobloxPlayer.zip"
unzip -o -q "./RobloxPlayer.zip"
mv "./RobloxPlayer.app" "/Applications/Roblox.app"
rm "./RobloxPlayer.zip"

# 2. Strip Security
echo "[*] Patching Roblox binary..."
xattr -cr /Applications/Roblox.app
codesign --remove-signature "$TARGET_DIR/RobloxPlayer" 

# 3. Download Dylib (Logic: Delete old, get new)
echo "[*] Downloading latest dylib..."
[ -f "./$DYLIB_NAME" ] && rm "./$DYLIB_NAME"
curl -L -s "https://github.com/$REPO/releases/download/latest/$DYLIB_NAME" -o "./$DYLIB_NAME"

# 4. Launch
echo "[+] Done. Launching..."
DYLD_INSERT_LIBRARIES="$(pwd)/$DYLIB_NAME" "$TARGET_DIR/RobloxPlayer" &
