#include <iostream>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <mach-o/dyld.h>

namespace Offsets {
    // Verified Offsets for version-9e55b34566734c3b
    inline constexpr uintptr_t ProcessNetworkPacket = 0x102D85704;
    inline constexpr uintptr_t Print                = 0x1001D54B8;
    inline constexpr uintptr_t GetDataModel         = 0x1040F261A;
    
    // Your provided rebased children offsets
    inline constexpr uintptr_t ChildrenStart        = 0x78; 
    inline constexpr uintptr_t ChildrenEnd          = 0x80;
}

// Global Print Function for Dual Output
typedef void (*tPrint)(int type, const char* format, ...);
tPrint rbxPrint;

void DualLog(int type, const char* msg) {
    // 1. Output to Terminal (setup.sh console)
    printf("[Ghost Log] %s\n", msg);
    
    // 2. Output to Roblox In-Game Console
    if (rbxPrint) {
        rbxPrint(type, msg);
    }
}

// Desync Hook Logic
int64_t __fastcall hkProcessNetworkPacket(int64_t a1, int64_t a2, int64_t a3) {
    if (a2 != 0) {
        uint8_t packetId = *(uint8_t*)a2;
        if (packetId == 0x83 || packetId == 0x85) return 0; 
    }
    return 0; 
}

// Your Rebased Workspace Finder
uintptr_t FindWorkspaceRebased(uintptr_t dm) {
    uintptr_t listStart = *(uintptr_t*)(dm + Offsets::ChildrenStart);
    uintptr_t listEnd   = *(uintptr_t*)(dm + Offsets::ChildrenEnd);

    for (uintptr_t current = listStart; current < listEnd; current += 8) {
        uintptr_t child = *(uintptr_t*)current;
        if (!child) continue;

        // ClassName pointer at child + 0x18 for Intel Mac
        uintptr_t namePtr = *(uintptr_t*)(child + 0x18); 
        if (namePtr && strcmp((char*)namePtr, "Workspace") == 0) {
            return child;
        }
    }
    return 0;
}

void* MainToolkitThread(void* arg) {
    // 1. ASLR Slide Calculation
    intptr_t slide = _dyld_get_image_vmaddr_slide(0);
    rbxPrint = (tPrint)(slide + Offsets::Print);

    DualLog(1, "Dylib Active. Waiting 13s for Game Load...");
    sleep(13);

    // 2. Locate DataModel
    typedef uintptr_t (*tGetDataModel)();
    tGetDataModel rbxGetDataModel = (tGetDataModel)(slide + Offsets::GetDataModel);
    uintptr_t dm = rbxGetDataModel();

    if (dm) {
        DualLog(1, "DataModel Found. Scanning Children...");
        
        // 3. Find Workspace with your logic
        uintptr_t ws = FindWorkspaceRebased(dm);
        if (ws) {
            char ws_msg[64];
            snprintf(ws_msg, sizeof(ws_msg), "Workspace Located: 0x%lx", ws);
            DualLog(0, ws_msg);
        } else {
            DualLog(3, "Error: Could not find Workspace in Children List.");
        }
    } else {
        DualLog(3, "Error: GetDataModel returned NULL.");
    }

    // 4. Final Desync Patch
    uintptr_t netTarget = slide + Offsets::ProcessNetworkPacket;
    size_t pageSize = sysconf(_SC_PAGESIZE);
    uintptr_t pageStart = netTarget & ~(pageSize - 1);
    mprotect((void*)pageStart, pageSize, PROT_READ | PROT_WRITE | PROT_EXEC);

    unsigned char patch[] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    *(uintptr_t*)(&patch[6]) = (uintptr_t)hkProcessNetworkPacket;
    memcpy((void*)netTarget, patch, sizeof(patch));

    DualLog(2, "Desync On! Physics Filtered.");
    return nullptr;
}

void __attribute__((constructor)) initialize() {
    pthread_t thread;
    pthread_create(&thread, nullptr, MainToolkitThread, nullptr);
    pthread_detach(thread); 
}
