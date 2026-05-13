#include <iostream>
#include <vector>
#include <unistd.h>
#include <libproc.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach-o/dyld_images.h>
#include <algorithm>

namespace Offsets {
    inline constexpr uintptr_t TaskScheduler = 0x106522280; 
    inline constexpr uintptr_t JobStart      = 208; 
    inline constexpr uintptr_t JobEnd        = 216; 
    inline constexpr uintptr_t JobNameBitmask = 24;
    inline constexpr uintptr_t JobNameInline  = 25;
    inline constexpr uintptr_t JobNamePtr     = 40;
}

template <typename T>
T RPM(mach_port_t task, uintptr_t addr) {
    T buffer;
    mach_vm_size_t size;
    if (mach_vm_read_overwrite(task, (mach_vm_address_t)addr, sizeof(T), (mach_vm_address_t)&buffer, &size) != KERN_SUCCESS)
        return T(0);
    return buffer;
}

bool ReadString(mach_port_t task, uintptr_t addr, char* buffer, size_t maxLen) {
    mach_vm_size_t size;
    return (mach_vm_read_overwrite(task, (mach_vm_address_t)addr, maxLen, (mach_vm_address_t)buffer, &size) == KERN_SUCCESS);
}

// BROADER PID SEARCH
pid_t GetPid() {
    int nPids = proc_listpids(PROC_ALL_PIDS, 0, NULL, 0);
    std::vector<pid_t> pids(nPids);
    proc_listpids(PROC_ALL_PIDS, 0, pids.data(), nPids * sizeof(pid_t));
    
    for (auto pid : pids) {
        if (pid <= 0) continue;
        char path[PROC_PIDPATHINFO_MAXSIZE];
        if (proc_pidpath(pid, path, sizeof(path)) > 0) {
            std::string procPath(path);
            // Convert to lowercase for case-insensitive matching
            std::transform(procPath.begin(), procPath.end(), procPath.begin(), ::tolower);
            
            // Look for any variation of Roblox
            if (procPath.find("robloxplayer") != std::string::npos || 
                (procPath.find("roblox") != std::string::npos && procPath.find("app") != std::string::npos)) {
                return pid;
            }
        }
    }
    return -1;
}

void DumpJobs(mach_port_t task, uintptr_t base) {
    uintptr_t scheduler = RPM<uintptr_t>(task, base + Offsets::TaskScheduler);
    if (!scheduler) {
        std::cout << "[!] TaskScheduler static address failed. Scanning memory near base..." << std::endl;
        return;
    }

    uintptr_t v2 = RPM<uintptr_t>(task, scheduler + Offsets::JobStart);
    uintptr_t i  = RPM<uintptr_t>(task, scheduler + Offsets::JobEnd);

    std::cout << "\n[+] --- TaskScheduler Scan ---" << std::endl;
    for (uintptr_t curr = v2; curr < i; curr += 16) {
        uintptr_t jobPtr = RPM<uintptr_t>(task, curr);
        if (!jobPtr) continue;

        char name[64] = {0};
        unsigned char mask = RPM<unsigned char>(task, jobPtr + Offsets::JobNameBitmask);
        uintptr_t nameAddr = (mask & 1) ? RPM<uintptr_t>(task, jobPtr + Offsets::JobNamePtr) : (jobPtr + Offsets::JobNameInline);
        
        if (ReadString(task, nameAddr, name, 63)) {
            std::cout << "  [Job] " << name << " | 0x" << std::hex << jobPtr << std::dec << std::endl;
        }
    }
}

int main() {
    std::cout << "[*] Ghost Watcher v2.2 (Aggressive Search)" << std::endl;
    std::cout << "[*] Searching for any Roblox-related process..." << std::endl;

    while (true) {
        pid_t pid = GetPid();
        if (pid != -1) {
            std::cout << "[+] Potential match found! PID: " << pid << ". Attempting to attach..." << std::endl;
            
            mach_port_t task;
            kern_return_t kr = task_for_pid(mach_task_self(), pid, &task);
            
            if (kr == KERN_SUCCESS) {
                std::cout << "[+] Successfully attached to Task Port." << std::endl;
                mach_vm_address_t addr = 0;
                mach_vm_size_t size = 0;
                vm_region_basic_info_data_64_t info;
                mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
                mach_port_t object;
                
                if (mach_vm_region(task, &addr, &size, VM_REGION_BASIC_INFO_64, (vm_region_info_t)&info, &count, &object) == KERN_SUCCESS) {
                    std::cout << "[+] Found Base Address: 0x" << std::hex << addr << std::dec << std::endl;
                    DumpJobs(task, (uintptr_t)addr);
                }
                
                std::cout << "[*] Monitoring Roblox. Will reset if process closes." << std::endl;
                while (GetPid() != -1) sleep(5);
                std::cout << "[!] Process lost. Resuming search..." << std::endl;
            } else {
                std::cerr << "[-] Permission denied (kr: " << kr << "). Are you running with SUDO?" << std::endl;
                sleep(3);
            }
        }
        usleep(500000); // Scan every 0.5s for faster response
    }
    return 0;
}
