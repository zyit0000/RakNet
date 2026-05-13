#!/bin/bash

# Define the app name and GitHub info
APP_NAME="GhostWatcher"
REPO_OWNER="zyit0000"
REPO_NAME="RakNet"

echo "[*] Cleaning up existing $APP_NAME..."
rm -f "$APP_NAME"

echo "[*] Downloading latest executable from GitHub..."
# Downloads the asset named GhostWatcher from the latest release
curl -L -O "https://github.com/$REPO_OWNER/$REPO_NAME/releases/latest/download/$APP_NAME"

if [ -f "$APP_NAME" ]; then
    echo "[+] Download successful."
    chmod +x "$APP_NAME"
    echo "[!] Run with: sudo ./$APP_NAME"
else
    echo "[-] Error: Failed to download the executable."
fi
