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
}

// Function to safely set memory permissions without hitting the 0x2000 error
void SafeSetPerms(uintptr_t address) {
    size_t pageSize = sysconf(_SC_PAGESIZE);
    uintptr_t start = address & ~(pageSize - 1);
    mprotect((void*)start, pageSize, PROT_READ | PROT_WRITE | PROT_EXEC);
}

// The actual hook that drops physics packets
int64_t __fastcall hkProcessNetworkPacket(int64_t a1, int64_t a2, int64_t a3) {
    if (a2 != 0) {
        uint8_t packetId = *(uint8_t*)a2;
        // 0x83 = Physics, 0x85 = Cluster, 0x12 = Reliability
        if (packetId == 0x83 || packetId == 0x85 || packetId == 0x12) {
            return 0; // This is the Desync
        }
    }
    return 0; 
}

void* DesyncThread(void* arg) {
    intptr_t slide = _dyld_get_image_vmaddr_slide(0);
    typedef void (*tPrint)(int type, const char* format, ...);
    tPrint rbxPrint = (tPrint)(slide + Offsets::Print);

    sleep(60); // Wait for game to stabilize

    uintptr_t target = slide + Offsets::ProcessNetworkPacket;
    SafeSetPerms(target); // Fixes the mach_vm_read crash

    unsigned char patch[] = { 
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 
    };
    *(uintptr_t*)(&patch[6]) = (uintptr_t)hkProcessNetworkPacket;

    memcpy((void*)target, patch, sizeof(patch));
    
    rbxPrint(1, "Desync on!"); // Blue confirmation text
    return nullptr;
}

void __attribute__((constructor)) initialize() {
    pthread_t thread;
    pthread_create(&thread, nullptr, DesyncThread, nullptr);
    pthread_detach(thread); 
}
