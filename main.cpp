#include <iostream>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <mach-o/dyld.h>

// --- Final Verified Offsets (version-9e55b34566734c3b) ---
namespace Offsets {
    inline constexpr uintptr_t ProcessNetworkPacket = 0x102D85704;
    inline constexpr uintptr_t Print                = 0x1001D54B8;
    inline constexpr uintptr_t RaknetSend           = 0x102D85C06;
    inline constexpr uintptr_t TaskScheduler        = 0x106522280;
}

// --- Memory Permissions Utility ---
// Used to bypass macOS "Write XOR Execute" (W^X) security
void SetMemoryPerms(uintptr_t address, size_t size, bool writable) {
    size_t pageSize = sysconf(_SC_PAGESIZE);
    uintptr_t start = address & ~(pageSize - 1);
    int perms = writable ? (PROT_READ | PROT_WRITE | PROT_EXEC) : (PROT_READ | PROT_EXEC);
    mprotect((void*)start, pageSize, perms);
}

// --- The Ghost Desync Hook Function ---
int64_t __fastcall hkProcessNetworkPacket(int64_t a1, int64_t a2, int64_t a3) {
    if (a2 != 0) {
        uint8_t packetId = *(uint8_t*)a2;
        
        // Blocking Physics Cluster (0x83 / 0x85) and Reliability (0x12)
        // This stops your avatar position from being sent to the server
        if (packetId == 0x83 || packetId == 0x85 || packetId == 0x12) {
            return 0; // Drop physics/reliability packets to desync
        }
    }
    // Return 0 to indicate the packet was "handled" by our hook
    return 0; 
}

// --- Main Logic Thread ---
void* DesyncThread(void* arg) {
    // 1. Calculate the ASLR Slide (The random shift macOS adds to the base address)
    intptr_t slide = _dyld_get_image_vmaddr_slide(0);
    
    // 2. Setup the internal engine print function (Slide + Offset)
    typedef void (*tPrint)(int type, const char* format, ...);
    tPrint rbxPrint = (tPrint)(slide + Offsets::Print);

    // 3. Wait for 13 seconds to allow you to spawn in-game
    sleep(30);

    // 4. Apply the Hook to the real shifted address
    uintptr_t target = slide + Offsets::ProcessNetworkPacket;
    SetMemoryPerms(target, 14, true);

    // x64 Absolute Jump Patch (14-byte trampoline)
    unsigned char patch[] = { 
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,              // jmp [rip+0]
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00   // 64-bit destination address
    };
    *(uintptr_t*)(&patch[6]) = (uintptr_t)hkProcessNetworkPacket;

    memcpy((void*)target, patch, sizeof(patch));
    SetMemoryPerms(target, 14, false);
    
    // 5. Success Notifications (Prints directly to the in-game console)
    rbxPrint(1, "Desync on!");               // Blue info text
    rbxPrint(0, "[Ghost] RakNet Filtered");  // White standard text
    rbxPrint(2, "[Ghost] Physics Ignored");  // Yellow warning text

    return nullptr;
}

// --- Dylib Constructor ---
void __attribute__((constructor)) initialize() {
    printf("[+] Ghost Mod Injected Successfully.\n");
    
    // Create a background thread so the game doesn't hang while waiting
    pthread_t thread;
    pthread_create(&thread, nullptr, DesyncThread, nullptr);
    pthread_detach(thread); 
}
