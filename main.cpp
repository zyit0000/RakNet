#include <iostream>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <mach-o/dyld.h>
#include <mach/mach.h>

namespace Offsets {
    // Verified Intel x64 Offsets
    inline constexpr uintptr_t ProcessNetworkPacket = 0x102D85704;
    inline constexpr uintptr_t Print                = 0x1001D54B8;
    inline constexpr uintptr_t GetDataModel         = 0x1040F261A;
    inline constexpr uintptr_t ChildrenStart        = 0x78; 
    inline constexpr uintptr_t ChildrenEnd          = 0x80;
}

typedef void (*tPrint)(int type, const char* format, ...);
tPrint rbxPrint = nullptr;

// DUAL LOGGING: Terminal + In-Game Console
void DualLog(int type, const char* msg) {
    printf("[Ghost Log] %s\n", msg);
    if (rbxPrint) {
        rbxPrint(type, msg);
    }
}

// SAFE READ: Prevents mach_vm_read invalid address crashes
template <typename T>
T SafeRead(uintptr_t addr) {
    T buffer;
    vm_size_t size;
    kern_return_t kr = vm_read_overwrite(mach_task_self(), (vm_address_t)addr, sizeof(T), (vm_address_t)&buffer, &size);
    if (kr != KERN_SUCCESS) return T(0); 
    return buffer;
}

// THE HOOK: Filters Physics (0x83) and Cluster (0x85)
int64_t hkProcessNetworkPacket(int64_t a1, int64_t a2, int64_t a3) {
    if (a2 != 0) {
        uint8_t packetId = SafeRead<uint8_t>(a2);
        if (packetId == 0x83 || packetId == 0x85) return 0; 
    }
    return 0; 
}

// WORKSPACE FINDER: Uses your rebased children logic
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

    printf("[+] Ghost Mod Injected. Cooling down for 25s...\n");
    sleep(25); 

    // 1. DataModel Setup
    typedef uintptr_t (*tGetDataModel)();
    tGetDataModel rbxGetDataModel = (tGetDataModel)(slide + Offsets::GetDataModel);
    uintptr_t dm = rbxGetDataModel();

    if (dm) {
        DualLog(1, "DataModel Found. Searching for Workspace...");
        uintptr_t ws = FindWorkspaceRebased(dm);
        if (ws) {
            char ws_buf[64];
            snprintf(ws_buf, sizeof(ws_buf), "Workspace Found: 0x%lx", ws);
            DualLog(0, ws_buf);
        } else {
            DualLog(3, "Error: Could not resolve Workspace from Children list.");
        }
    }

    // 2. Memory Patch with Cache Flush
    uintptr_t netTarget = slide + Offsets::ProcessNetworkPacket;
    size_t pageSize = sysconf(_SC_PAGESIZE);
    uintptr_t pageStart = netTarget & ~(pageSize - 1);

    if (mprotect((void*)pageStart, pageSize * 2, PROT_READ | PROT_WRITE | PROT_EXEC) == 0) {
        unsigned char patch[14] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };
        uintptr_t hookAddr = (uintptr_t)hkProcessNetworkPacket;
        memcpy(&patch[6], &hookAddr, sizeof(uintptr_t));

        memcpy((void*)netTarget, patch, sizeof(patch));

        // Fix for "Illegal Instruction" and Kicks
        char* begin = (char*)netTarget;
        char* end = begin + 14;
        __builtin___clear_cache(begin, end); 
        
        DualLog(2, "Ghost Desync Active. Physics Filtered.");
    } else {
        printf("[-] mprotect failed at 0x%lx\n", pageStart);
    }

    return nullptr;
}

void __attribute__((constructor)) initialize() {
    pthread_t thread;
    pthread_create(&thread, nullptr, MainToolkitThread, nullptr);
    pthread_detach(thread); 
}
