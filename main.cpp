#include <iostream>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <mach-o/dyld.h>

namespace Offsets {
    inline constexpr uintptr_t ProcessNetworkPacket = 0x102D85704;
    inline constexpr uintptr_t Print                = 0x1001D54B8;
    inline constexpr uintptr_t GetDataModel         = 0x1040F261A;
    inline constexpr uintptr_t ChildrenStart        = 0x78; 
    inline constexpr uintptr_t ChildrenEnd          = 0x80;
}

typedef void (*tPrint)(int type, const char* format, ...);
tPrint rbxPrint = nullptr;

void DualLog(int type, const char* msg) {
    printf("[Ghost Log] %s\n", msg);
    if (rbxPrint) {
        rbxPrint(type, msg);
    }
}

// Fixed mincore call: Cast vec to char* for macOS SDK compatibility
bool IsAddressValid(uintptr_t addr) {
    if (addr < 0x100000000 || addr > 0x7FFFFFFFFFFF) return false;
    unsigned char vec;
    return (mincore((void*)(addr & ~0xFFF), 1, (char*)&vec) == 0);
}

// Removed __fastcall as it is ignored on macOS x64
int64_t hkProcessNetworkPacket(int64_t a1, int64_t a2, int64_t a3) {
    if (a2 != 0) {
        uint8_t packetId = *(uint8_t*)a2;
        if (packetId == 0x83 || packetId == 0x85) return 0; 
    }
    return 0; 
}

uintptr_t FindWorkspaceRebased(uintptr_t dm) {
    if (!IsAddressValid(dm + Offsets::ChildrenStart)) return 0;
    
    uintptr_t listStart = *(uintptr_t*)(dm + Offsets::ChildrenStart);
    uintptr_t listEnd   = *(uintptr_t*)(dm + Offsets::ChildrenEnd);

    for (uintptr_t current = listStart; current < listEnd; current += 8) {
        if (!IsAddressValid(current)) continue;
        uintptr_t child = *(uintptr_t*)current;
        if (!child) continue;

        uintptr_t namePtr = *(uintptr_t*)(child + 0x18); 
        if (IsAddressValid(namePtr) && strcmp((char*)namePtr, "Workspace") == 0) {
            return child;
        }
    }
    return 0;
}

void* MainToolkitThread(void* arg) {
    intptr_t slide = _dyld_get_image_vmaddr_slide(0);
    rbxPrint = (tPrint)(slide + Offsets::Print);

    printf("[+] Ghost Desync Loaded. Waiting 30s for memory stability...\n");
    sleep(30); 

    typedef uintptr_t (*tGetDataModel)();
    tGetDataModel rbxGetDataModel = (tGetDataModel)(slide + Offsets::GetDataModel);
    
    uintptr_t dm = 0;
    if (IsAddressValid((uintptr_t)rbxGetDataModel)) {
        dm = rbxGetDataModel();
    }

    if (dm && IsAddressValid(dm)) {
        DualLog(1, "DataModel Found. Finding Workspace...");
        uintptr_t ws = FindWorkspaceRebased(dm);
        if (ws) {
            char ws_buf[64];
            snprintf(ws_buf, sizeof(ws_buf), "Workspace: 0x%lx", ws);
            DualLog(0, ws_buf);
        }
    }

    uintptr_t netTarget = slide + Offsets::ProcessNetworkPacket;
    size_t pageSize = sysconf(_SC_PAGESIZE);
    uintptr_t pageStart = netTarget & ~(pageSize - 1);

    if (mprotect((void*)pageStart, pageSize, PROT_READ | PROT_WRITE | PROT_EXEC) == 0) {
        unsigned char patch[] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
        *(uintptr_t*)(&patch[6]) = (uintptr_t)hkProcessNetworkPacket;
        memcpy((void*)netTarget, patch, sizeof(patch));
        DualLog(2, "Desync Patch Applied Successfully.");
    }

    return nullptr;
}

void __attribute__((constructor)) initialize() {
    pthread_t thread;
    pthread_create(&thread, nullptr, MainToolkitThread, nullptr);
    pthread_detach(thread); 
}
