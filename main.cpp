#include <iostream>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <mach-o/dyld.h>

// --- Offsets provided by zyit0 ---
namespace Offsets {
    inline constexpr uintptr_t ProcessNetworkPacket = 0x102D85704;
    inline constexpr uintptr_t Print                = 0x1001D54B8;
    inline constexpr uintptr_t GetDataModel         = 0x1040F261A; 
}

typedef uintptr_t (*tGetDataModel)();

void* EngineScannerThread(void* arg) {
    intptr_t slide = _dyld_get_image_vmaddr_slide(0);
    
    // Internal Print Setup
    typedef void (*tPrint)(int type, const char* format, ...);
    tPrint rbxPrint = (tPrint)(slide + Offsets::Print);

    // Wait 13s for the game state to be ready
    sleep(30);

    // 1. Call GetDataModel and check for failure
    tGetDataModel rbxGetDataModel = (tGetDataModel)(slide + Offsets::GetDataModel);
    uintptr_t DataModel = rbxGetDataModel();

    if (DataModel == 0) {
        rbxPrint(3, "[!] ERROR: GetDataModel returned NULL. Is the offset stale?");
        return nullptr;
    }

    rbxPrint(1, "[+] DataModel found at 0x%lx. Starting scan...", DataModel);

    // --- 2. Dynamic Workspace Discovery ---
    uintptr_t WorkspacePtr = 0;
    bool foundWorkspace = false;

    for (int i = 0; i < 0x500; i += 8) {
        uintptr_t potentialInstance = *(uintptr_t*)(DataModel + i);
        // Validating pointer range for macOS x64
        if (potentialInstance > 0x100000000 && potentialInstance < 0x7FFFFFFFFFFF) {
            WorkspacePtr = potentialInstance;
            rbxPrint(0, "[Ghost] Found Workspace at offset: 0x%X", i);
            foundWorkspace = true;
            break;
        }
    }

    if (!foundWorkspace) {
        rbxPrint(3, "[!] ERROR: Could not find Workspace in DataModel memory.");
        return nullptr;
    }

    // --- 3. Property Scanning (WalkSpeed/JumpPower) ---
    bool foundWS = false;
    for (int j = 0; j < 0x1000; j += 4) {
        float val = *(float*)(WorkspacePtr + j);
        if (val == 16.0f) {
            rbxPrint(0, "[Ghost] WalkSpeed Offset Located: 0x%X", j);
            foundWS = true;
            break; 
        }
    }

    if (!foundWS) {
        rbxPrint(3, "[!] ERROR: WalkSpeed (16.0) not found in Workspace memory.");
    }

    return nullptr;
}

void __attribute__((constructor)) initialize() {
    printf("[+] Ghost Mod Injected. Watching for engine errors...\n");
    
    pthread_t thread;
    pthread_create(&thread, nullptr, EngineScannerThread, nullptr);
    pthread_detach(thread); 
}
