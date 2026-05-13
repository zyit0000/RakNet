#include <iostream>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <mach-o/dyld.h>

namespace Offsets {
    // Note: If these already start with 0x10... they might not need a slide!
    inline constexpr uintptr_t ProcessNetworkPacket = 0x102D85704;
    inline constexpr uintptr_t Print                = 0x1001D54B8;
}

// Fixed Perms: Only flips exactly 1 page to prevent KERN_PROTECTION_FAILURE
bool TrySafePatch(uintptr_t address, void* hook_func) {
    size_t pageSize = sysconf(_SC_PAGESIZE);
    uintptr_t pageStart = address & ~(pageSize - 1);

    // Verify the address isn't in 'outer space' (too high)
    if (address > 0x7FFFFFFFFFFF) return false;

    if (mprotect((void*)pageStart, pageSize, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        return false; // Kernel rejected the protection change
    }

    unsigned char patch[] = { 
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 
    };
    *(uintptr_t*)(&patch[6]) = (uintptr_t)hook_func;

    memcpy((void*)address, patch, sizeof(patch));
    return true;
}

int64_t __fastcall hkProcessNetworkPacket(int64_t a1, int64_t a2, int64_t a3) {
    if (a2 != 0) {
        uint8_t packetId = *(uint8_t*)a2;
        if (packetId == 0x83 || packetId == 0x85) return 0; // Desync
    }
    return 0; 
}

void* DesyncThread(void* arg) {
    // Try BOTH with and without slide to see which one the dumper likes
    intptr_t slide = _dyld_get_image_vmaddr_slide(0);
    
    // 13s wait to ensure the process is fully 'unpacked' in memory
    sleep(60);

    uintptr_t target = slide + Offsets::ProcessNetworkPacket;
    
    if (!TrySafePatch(target, (void*)hkProcessNetworkPacket)) {
        // If slide + offset failed, try just the offset
        TrySafePatch(Offsets::ProcessNetworkPacket, (void*)hkProcessNetworkPacket);
    }

    return nullptr;
}

void __attribute__((constructor)) initialize() {
    pthread_t thread;
    pthread_create(&thread, nullptr, DesyncThread, nullptr);
    pthread_detach(thread); 
}
