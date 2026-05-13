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
}

typedef void (*tPrint)(int type, const char* format, ...);
tPrint rbxPrint = nullptr;

void DualLog(int type, const char* msg) {
    printf("[Ghost Log] %s\n", msg);
    if (rbxPrint) { rbxPrint(type, msg); }
}

// Global hook to keep it in a safe memory segment
int64_t hkProcessNetworkPacket(int64_t a1, int64_t a2, int64_t a3) {
    if (a2 != 0) {
        // Safe check for the packet ID
        uint8_t packetId = 0;
        vm_size_t sz;
        if (vm_read_overwrite(mach_task_self(), (vm_address_t)a2, 1, (vm_address_t)&packetId, &sz) == KERN_SUCCESS) {
            if (packetId == 0x83 || packetId == 0x85) return 0; 
        }
    }
    return 0; 
}

void* MainToolkitThread(void* arg) {
    intptr_t slide = _dyld_get_image_vmaddr_slide(0);
    rbxPrint = (tPrint)(slide + Offsets::Print);

    // 25s for absolute stability on Intel Macs
    sleep(25); 

    uintptr_t netTarget = slide + Offsets::ProcessNetworkPacket;
    size_t pageSize = sysconf(_SC_PAGESIZE);
    uintptr_t pageStart = netTarget & ~(pageSize - 1);

    // Apply patch with Double-Page protection
    if (mprotect((void*)pageStart, pageSize * 2, PROT_READ | PROT_WRITE | PROT_EXEC) == 0) {
        
        unsigned char patch[14] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };
        uintptr_t hookAddr = (uintptr_t)hkProcessNetworkPacket;
        memcpy(&patch[6], &hookAddr, sizeof(uintptr_t));

        memcpy((void*)netTarget, patch, sizeof(patch));

        // CRITICAL: Flush Instruction Cache to prevent "Illegal Instruction" kicks
        sys_icache_invalidate((void*)netTarget, 14);
        
        DualLog(2, "Ghost Desync fully synchronized and active.");
    }

    return nullptr;
}

void __attribute__((constructor)) initialize() {
    pthread_t thread;
    pthread_create(&thread, nullptr, MainToolkitThread, nullptr);
    pthread_detach(thread); 
}
