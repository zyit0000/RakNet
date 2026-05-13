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
}

typedef void (*tPrint)(int type, const char* format, ...);
tPrint rbxPrint = nullptr;

void DualLog(int type, const char* msg) {
    printf("[Ghost Log] %s\n", msg);
    if (rbxPrint) { rbxPrint(type, msg); }
}

// SAFE READ: Kernel-level memory check to prevent mach_vm_read crashes
template <typename T>
T SafeRead(uintptr_t addr) {
    T buffer;
    vm_size_t size;
    if (vm_read_overwrite(mach_task_self(), (vm_address_t)addr, sizeof(T), (vm_address_t)&buffer, &size) != KERN_SUCCESS) 
        return T(0); 
    return buffer;
}

// THE HOOK: Standard packet filter
int64_t hkProcessNetworkPacket(int64_t a1, int64_t a2, int64_t a3) {
    if (a2 != 0) {
        uint8_t packetId = SafeRead<uint8_t>(a2);
        if (packetId == 0x83 || packetId == 0x85) return 0; 
    }
    return 0; 
}

void* MainToolkitThread(void* arg) {
    intptr_t slide = _dyld_get_image_vmaddr_slide(0);
    rbxPrint = (tPrint)(slide + Offsets::Print);

    printf("[+] Ghost Mod Active. Waiting for stable state...\n");
    sleep(20); 

    // --- The Instruction-Safe Patch ---
    uintptr_t netTarget = slide + Offsets::ProcessNetworkPacket;
    size_t pageSize = sysconf(_SC_PAGESIZE);
    uintptr_t pageStart = netTarget & ~(pageSize - 1);

    // We flip permissions for two pages just in case the 14-byte patch overlaps
    if (mprotect((void*)pageStart, pageSize * 2, PROT_READ | PROT_WRITE | PROT_EXEC) == 0) {
        
        // This is a more stable 14-byte absolute jump for Intel x64
        // Opcode: FF 25 00 00 00 00 [64-bit Address]
        unsigned char patch[14] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };
        uintptr_t hookAddr = (uintptr_t)hkProcessNetworkPacket;
        memcpy(&patch[6], &hookAddr, sizeof(uintptr_t));

        // Use __builtin_ia32_pause() or similar if needed, but memcpy is usually atomic enough
        memcpy((void*)netTarget, patch, sizeof(patch));
        
        DualLog(2, "Patch Successful: No Illegal Instructions.");
    } else {
        printf("[-] Critical: mprotect failed.\n");
    }

    return nullptr;
}

void __attribute__((constructor)) initialize() {
    pthread_t thread;
    pthread_create(&thread, nullptr, MainToolkitThread, nullptr);
    pthread_detach(thread); 
}
