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

// Improved patcher with error reporting
bool ForcePatch(uintptr_t address, void* hook_func) {
    size_t pageSize = sysconf(_SC_PAGESIZE);
    uintptr_t pageStart = address & ~(pageSize - 1);

    // Try to force the kernel to allow writing
    if (mprotect((void*)pageStart, pageSize, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        printf("[!] Failed to set permissions at 0x%llX\n", address);
        return false;
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
        if (packetId == 0x83 || packetId == 0x85) return 0;
    }
    return 0; 
}

void* DesyncThread(void* arg) {
    intptr_t slide = _dyld_get_image_vmaddr_slide(0);
    printf("[+] ASLR Slide: 0x%lX\n", slide);

    sleep(30); // Wait for the game to fully map segments

    uintptr_t target = slide + Offsets::ProcessNetworkPacket;
    printf("[+] Attempting patch at: 0x%llX\n", target);

    if (ForcePatch(target, (void*)hkProcessNetworkPacket)) {
        printf("[!] PATCH SUCCESSFUL!\n");
        
        // Try to notify in-game console
        typedef void (*tPrint)(int type, const char* format, ...);
        tPrint rbxPrint = (tPrint)(slide + Offsets::Print);
        rbxPrint(1, "Desync on! (Ghost Mode)");
    } else {
        printf("[!] PATCH FAILED. Address likely invalid for this version.\n");
    }

    return nullptr;
}

void __attribute__((constructor)) initialize() {
    pthread_t thread;
    pthread_create(&thread, nullptr, DesyncThread, nullptr);
    pthread_detach(thread); 
}
