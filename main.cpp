#include <iostream>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <mach-o/dyld.h>
#include <mach/mach.h>
#include <mach/vm_map.h>

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
    if (rbxPrint) { rbxPrint(type, msg); }
}

template <typename T>
T SafeRead(uintptr_t addr) {
    T buffer;
    vm_size_t size;
    if (vm_read_overwrite(mach_task_self(), (vm_address_t)addr, sizeof(T), (vm_address_t)&buffer, &size) != KERN_SUCCESS) 
        return T(0); 
    return buffer;
}

// Fixed Hook logic for Intel x64 Stability
int64_t hkProcessNetworkPacket(int64_t a1, int64_t a2, int64_t a3) {
    if (a2 != 0) {
        uint8_t packetId = SafeRead<uint8_t>(a2);
        if (packetId == 0x83 || packetId == 0x85) return 0; 
    }
    return 0; 
}

// FORCE PATCH: The only way to stop 0x2000 / Signal 4 crashes on Intel
bool ForceMemoryWrite(uintptr_t address, void* data, size_t size) {
    mach_port_t self = mach_task_self();
    vm_address_t page_start = (vm_address_t)address & ~((vm_address_t)sysconf(_SC_PAGESIZE) - 1);
    vm_size_t page_size = sysconf(_SC_PAGESIZE);

    // Force the page to be writable at the kernel level
    kern_return_t kr = vm_protect(self, page_start, page_size, FALSE, VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY);
    if (kr != KERN_SUCCESS) return false;

    memcpy((void*)address, data, size);

    // Restore to Read/Execute
    vm_protect(self, page_start, page_size, FALSE, VM_PROT_READ | VM_PROT_EXEC);
    return true;
}

void* MainToolkitThread(void* arg) {
    intptr_t slide = _dyld_get_image_vmaddr_slide(0);
    rbxPrint = (tPrint)(slide + Offsets::Print);

    printf("[+] Ghost Mod Injected. 25s Stability Delay...\n");
    sleep(25); 

    uintptr_t netTarget = slide + Offsets::ProcessNetworkPacket;
    
    // 14-byte Absolute Jump with a 2-byte NOP prefix for alignment safety
    unsigned char patch[14] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };
    uintptr_t hookAddr = (uintptr_t)hkProcessNetworkPacket;
    memcpy(&patch[6], &hookAddr, sizeof(uintptr_t));

    if (ForceMemoryWrite(netTarget, patch, sizeof(patch))) {
        __builtin___clear_cache((char*)netTarget, (char*)netTarget + sizeof(patch));
        DualLog(2, "Ghost Desync Live. (Kernel-Level Write Success)");
    } else {
        printf("[-] FAILED: Kernel rejected vm_protect on 0x%lx\n", netTarget);
    }

    return nullptr;
}

void __attribute__((constructor)) initialize() {
    pthread_t thread;
    pthread_create(&thread, nullptr, MainToolkitThread, nullptr);
    pthread_detach(thread); 
}
