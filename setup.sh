#!/bin/bash

APP_NAME="GhostWatcher"
REPO_OWNER="zyit0000"
REPO_NAME="RakNet"

echo "[*] Cleaning up old files..."
rm -f "$APP_NAME"
rm -f debug.entitlements

echo "[*] Downloading latest $APP_NAME..."
curl -L -O "https://github.com/$REPO_OWNER/$REPO_NAME/releases/latest/download/$APP_NAME"

if [ -f "$APP_NAME" ]; then
    echo "[*] Generating Developer Entitlements..."
    
    # Create the XML required to bypass task_for_pid restrictions
    cat <<EOF > debug.entitlements
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>com.apple.security.cs.disable-library-validation</key>
    <true/>
    <key>com.apple.security.cs.allow-unsigned-executable-memory</key>
    <true/>
    <key>com.apple.security.get-task-allow</key>
    <true/>
</dict>
</plist>
EOF

    echo "[*] Applying Signatures and Bypassing Gatekeeper..."
    chmod 755 "$APP_NAME"
    
    # Force the entitlement signature onto the binary
    codesign --entitlements debug.entitlements -f -s - "$APP_NAME"
    
    # Strip quarantine so sudo actually works
    sudo xattr -rd com.apple.quarantine "$APP_NAME" 2>/dev/null
    
    echo "[+] Done. Starting GhostWatcher..."
    rm debug.entitlements # Clean up the XML file
    
    ./$APP_NAME
else
    echo "[-] Error: Download failed. Check your GitHub Release tab."
fi
