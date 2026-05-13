#!/bin/bash

APP_NAME="GhostWatcher"
REPO_OWNER="zyit0000"
REPO_NAME="RakNet"

echo "[*] Removing old $APP_NAME..."
rm -f "$APP_NAME"

echo "[*] Downloading latest build..."
curl -L -O "https://github.com/$REPO_OWNER/$REPO_NAME/releases/latest/download/$APP_NAME"

if [ -f "$APP_NAME" ]; then
    echo "[*] Fixing permissions and bypassing Gatekeeper..."
    # Mark as executable
    chmod 755 "$APP_NAME"
    # Strip the quarantine flag so it can run with sudo
    sudo xattr -rd com.apple.quarantine "$APP_NAME" 2>/dev/null
    
    echo "[+] Done. You can now run it by just typing: ./$APP_NAME"
    ./$APP_NAME
else
    echo "[-] Error: Download failed."
fi
