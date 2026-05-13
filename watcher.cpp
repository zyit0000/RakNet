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

// --- Memory Utilities ---
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

// --- Search Logic ---
pid_t GetPid() {
    int nPids = proc_listpids(PROC_ALL_PIDS, 0, NULL, 0);
    std::vector<pid_t> pids(nPids);
    proc_listpids(PROC_ALL_PIDS, 0, pids.data(), nPids * sizeof(pid_t));
    for (auto pid : pids) {
        if (pid <= 0) continue;
        char path[PROC_PIDPATHINFO_MAXSIZE];
        if (proc_pidpath(pid, path, sizeof(path)) > 0) {
            std::string procPath(path);
            std::transform(procPath.begin(), procPath.end(), procPath.begin(), ::tolower);
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
        std::cout << "[-] TaskScheduler is NULL at 0x" << std::hex << (base + Offsets::TaskScheduler) << std::dec << std::endl;
        return;
    }

    uintptr_t v2 = RPM<uintptr_t>(task, scheduler + Offsets::JobStart);
    uintptr_t i  = RPM<uintptr_t>(task, scheduler + Offsets::JobEnd);

    std::cout << "[*] Starting Job Scan..." << std::endl;

    for (uintptr_t curr = v2; curr < i; curr += 16) {
        uintptr_t jobPtr = RPM<uintptr_t>(task, curr);
        if (!jobPtr) continue;

        char name[64] = {0};
        unsigned char mask = RPM<unsigned char>(task, jobPtr + Offsets::JobNameBitmask);
        uintptr_t nameAddr = (mask & 1) ? RPM<uintptr_t>(task, jobPtr + Offsets::JobNamePtr) : (jobPtr + Offsets::JobNameInline);
        
        if (ReadString(task, nameAddr, name, 63)) {
            // Find DataModel via specific Jobs
            if (strcmp(name, "WaitingHybrid") == 0 || strcmp(name, "Heartbeat") == 0) {
                // Offset +456 (0x1C8) from your IDA analysis
                uintptr_t dm = RPM<uintptr_t>(task, jobPtr + 0x1C8);
                if (dm) {
                    std::cout << "[+] Found DataModel (via " << name << "): 0x" << std::hex << dm << std::dec << std::endl;
                }
            }
        }
    }
    std::cout << "[+] Scan complete." << std::endl;
}

int main(int argc, char** argv) {
    if (geteuid() != 0) {
        std::cout << "[*] System: Elevating to Root..." << std::endl;
        char* args[argc + 2];
        args[0] = (char*)"/usr/bin/sudo";
        args[1] = argv[0];
        for (int i = 1; i < argc; i++) args[i + 1] = argv[i];
        args[argc + 1] = NULL;
        execv("/usr/bin/sudo", args);
        return 1;
    }

    std::cout << "[*] Ghost Watcher v2.5 (Verbose Output)" << std::endl;

    while (true) {
        pid_t pid = GetPid();
        if (pid != -1) {
            std::cout << "[+] Found Roblox (PID: " << pid << "). Attaching..." << std::endl;
            
            mach_port_t task;
            if (task_for_pid(mach_task_self(), pid, &task) == KERN_SUCCESS) {
                std::cout << "[+] Attached. Fetching Base..." << std::endl;
                
                mach_vm_address_t addr = 0;
                mach_vm_size_t size = 0;
                vm_region_basic_info_data_64_t info;
                mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
                mach_port_t object;
                
                if (mach_vm_region(task, &addr, &size, VM_REGION_BASIC_INFO_64, (vm_region_info_t)&info, &count, &object) == KERN_SUCCESS) {
                    std::cout << "[+] Roblox Base: 0x" << std::hex << addr << std::dec << std::endl;
                    
                    // Call the scan
                    DumpJobs(task, (uintptr_t)addr);
                } else {
                    std::cout << "[-] Failed to resolve Base Address." << std::endl;
                }
                
                std::cout << "[*] Waiting for process exit..." << std::endl;
                while (GetPid() != -1) sleep(10);
                std::cout << "[!] Resetting..." << std::endl;
            } else {
                std::cout << "[-] Failed to get task port. Is SIP blocking me?" << std::endl;
                sleep(2);
            }
        }
        usleep(1000000);
    }
    return 0;
}
