#include <iostream>
#include <vector>
#include <unistd.h>
#include <libproc.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach-o/dyld_images.h>

namespace Offsets {
    // New Offsets from your sub_1044F207E analysis
    inline constexpr uintptr_t TaskScheduler = 0x106522280; 
    inline constexpr uintptr_t JobStart      = 208; 
    inline constexpr uintptr_t JobEnd        = 216; 
    
    // String handling constants
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

// --- Engine Logic ---

void ScanForDataModel(mach_port_t task, uintptr_t jobPtr, const char* jobName) {
    // Based on your sub_1040AEA94: "DataModel::get invoked"
    // We scan the job memory for pointers that look like a DataModel.
    // Usually, WaitingHybrid or Heartbeat jobs hold a pointer to the DataModel.
    
    for (int offset = 0x100; offset < 0x250; offset += 8) {
        uintptr_t potentialDM = RPM<uintptr_t>(task, jobPtr + offset);
        
        // A valid DM pointer on Intel Mac usually starts with 0x1 or 0x7
        if (potentialDM > 0x100000000 && potentialDM < 0x7FFFFFFFFFFF) {
            // If we find something at +456 (0x1C8), that's a strong match for your pseudocode hint
            if (offset == 0x1C8) { 
                std::cout << "    [!] HIGH PROBABILITY DataModel (Offset +456): 0x" << std::hex << potentialDM << std::dec << std::endl;
            } else {
                // Potential alternative pointer
                // std::cout << "    [?] Pointer at +" << offset << ": 0x" << std::hex << potentialDM << std::dec << std::endl;
            }
        }
    }
}

void DumpJobs(mach_port_t task, uintptr_t base) {
    uintptr_t scheduler = RPM<uintptr_t>(task, base + Offsets::TaskScheduler);
    if (!scheduler) {
        std::cerr << "[-] TaskScheduler base is NULL. Game might still be loading." << std::endl;
        return;
    }

    uintptr_t v2 = RPM<uintptr_t>(task, scheduler + Offsets::JobStart);
    uintptr_t i  = RPM<uintptr_t>(task, scheduler + Offsets::JobEnd);

    std::cout << "\n[+] --- TaskScheduler Scan (sub_1044F207E) ---" << std::endl;

    for (uintptr_t curr = v2; curr < i; curr += 16) {
        uintptr_t jobPtr = RPM<uintptr_t>(task, curr);
        if (!jobPtr) continue;

        char name[64] = {0};
        unsigned char mask = RPM<unsigned char>(task, jobPtr + Offsets::JobNameBitmask);
        uintptr_t nameAddr = (mask & 1) ? RPM<uintptr_t>(task, jobPtr + Offsets::JobNamePtr) : (jobPtr + Offsets::JobNameInline);
        
        if (ReadString(task, nameAddr, name, 63)) {
            std::cout << "  [Job] " << name << " | 0x" << std::hex << jobPtr << std::dec << std::endl;
            
            // These jobs are the most likely to contain the DataModel pointer
            if (strcmp(name, "WaitingHybrid") == 0 || strcmp(name, "Heartbeat") == 0) {
                ScanForDataModel(task, jobPtr, name);
            }
        }
    }
    std::cout << "[+] --- Scan Complete ---\n" << std::endl;
}

// --- Process Logic ---

pid_t GetPid() {
    int nPids = proc_listpids(PROC_ALL_PIDS, 0, NULL, 0);
    std::vector<pid_t> pids(nPids);
    proc_listpids(PROC_ALL_PIDS, 0, pids.data(), nPids * sizeof(pid_t));
    for (auto pid : pids) {
        if (pid == 0) continue;
        char path[PROC_PIDPATHINFO_MAXSIZE];
        proc_pidpath(pid, path, sizeof(path));
        if (strstr(path, "RobloxPlayer")) return pid;
    }
    return -1;
}

int main() {
    std::cout << "[*] Ghost Watcher v2 (TaskScheduler Strategy)" << std::endl;
    
    while (true) {
        pid_t pid = GetPid();
        if (pid != -1) {
            mach_port_t task;
            if (task_for_pid(mach_task_self(), pid, &task) == KERN_SUCCESS) {
                vm_address_t addr = 0;
                vm_size_t size = 0;
                vm_region_basic_info_data_64_t info;
                mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
                mach_port_t object;
                
                if (mach_vm_region(task, &addr, &size, VM_REGION_BASIC_INFO_64, (char*)&info, &count, &object) == KERN_SUCCESS) {
                    DumpJobs(task, (uintptr_t)addr);
                }
                while (GetPid() != -1) sleep(10);
            } else {
                std::cerr << "[!] sudo required for task_for_pid." << std::endl;
                sleep(5);
            }
        }
        usleep(1000000);
    }
    return 0;
}
