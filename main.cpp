#include <iostream>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <mach-o/dyld.h>
#include <mach/mach.h>

namespace Offsets {
    inline constexpr uintptr_t ProcessNetworkPacket = 0x102D85704;
    inline constexpr uintptr_t Print                = 0x1001D54B8;
    inline constexpr uintptr_t GetDataModel         = 0x1040F261A;
    inline constexpr uintptr_t ChildrenStart        = 0x78; 
    inline constexpr uintptr_t ChildrenEnd          = 0x80;
}

typedef void (*tPrint)(int type, const char* format, ...);
tPrint rbxPrint = nullptr;

// DUAL LOGGING: Terminal + In-Game
void DualLog(int type, const char* msg) {
    printf("[Ghost Log] %s\n", msg);
    if (rbxPrint) {
        rbxPrint(type, msg);
    }
}

// SAFE READ: This is the magic fix for mach_vm_read crashes
template <typename T>
T SafeRead(uintptr_t addr) {
    T buffer;
    vm_size_t size;
    // vm_read_overwrite checks if the address is valid without crashing
    kern_return_t kr = vm_read_overwrite(mach_task_self(), (vm_address_t)addr, sizeof(T), (vm_address_t)&buffer, &size);
    if (kr != KERN_SUCCESS) return T(0); 
    return buffer;
}

int64_t hkProcessNetworkPacket(int64_t a1, int64_t a2, int64_t a3) {
    if (a2 != 0) {
        uint8_t packetId = SafeRead<uint8_t>(a2);
        if (packetId == 0x83 || packetId == 0x85) return 0; 
    }
    return 0; 
}

uintptr_t FindWorkspaceRebased(uintptr_t dm) {
    uintptr_t listStart = SafeRead<uintptr_t>(dm + Offsets::ChildrenStart);
    uintptr_t listEnd   = SafeRead<uintptr_t>(dm + Offsets::ChildrenEnd);

    if (!listStart || !listEnd) return 0;

    for (uintptr_t current = listStart; current < listEnd; current += 8) {
        uintptr_t child = SafeRead<uintptr_t>(current);
        if (!child) continue;

        uintptr_t namePtr = SafeRead<uintptr_t>(child + 0x18); 
        if (namePtr) {
            char name[32];
            vm_size_t sz;
            // Safely read the string to avoid invalid address errors
            if (vm_read_overwrite(mach_task_self(), (vm_address_t)namePtr, 32, (vm_address_t)&name, &sz) == KERN_SUCCESS) {
                if (strcmp(name, "Workspace") == 0) return child;
            }
        }
    }
    return 0;
}

void* MainToolkitThread(void* arg) {
    intptr_t slide = _dyld_get_image_vmaddr_slide(0);
    rbxPrint = (tPrint)(slide + Offsets::Print);

    printf("[+] ASLR Fix Applied. Waiting 40s...\n");
    sleep(40); 

    typedef uintptr_t (*tGetDataModel)();
    tGetDataModel rbxGetDataModel = (tGetDataModel)(slide + Offsets::GetDataModel);
    
    // Call GetDataModel safely
    uintptr_t dm = rbxGetDataModel();

    if (dm) {
        DualLog(1, "DataModel Found. Finding Workspace...");
        uintptr_t ws = FindWorkspaceRebased(dm);
        if (ws) {
            char ws_buf[64];
            snprintf(ws_buf, sizeof(ws_buf), "Workspace: 0x%lx", ws);
            DualLog(0, ws_buf);
        } else {
            printf("[-] Failed to safely resolve Workspace child.\n");
        }
    }

    // Applying Patch with standard protection bypass
    uintptr_t netTarget = slide + Offsets::ProcessNetworkPacket;
    size_t pageSize = sysconf(_SC_PAGESIZE);
    uintptr_t pageStart = netTarget & ~(pageSize - 1);

    if (mprotect((void*)pageStart, pageSize, PROT_READ | PROT_WRITE | PROT_EXEC) == 0) {
        unsigned char patch[] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
        *(uintptr_t*)(&patch[6]) = (uintptr_t)hkProcessNetworkPacket;
        memcpy((void*)netTarget, patch, sizeof(patch));
        DualLog(2, "Desync Active.");
    }

    return nullptr;
}

void __attribute__((constructor)) initialize() {
    pthread_t thread;
    pthread_create(&thread, nullptr, MainToolkitThread, nullptr);
    pthread_detach(thread); 
}
