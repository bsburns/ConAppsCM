#pragma once
/*------------------------------------------------------------------
 * debug_process.h
 *
 * Class to handle waiting for debug session to attach to a process 
 * before proceceeding with normal execution.  This is useful for 
 * debugging child processes that are spawned by a parent process.
 *
 * June 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */

#ifdef _WIN32
#include <processthreadsapi.h>
#include <process.h>
#else
#include <unistd.h>
#endif

#include "watchdog.h"
#include "threadManager.h"
#include "PacketHeader/PacketHeader.h"


class Debugger {
public:
    Debugger() {}
    static void Launch(uint32_t WaitDebugAttachIterations) {
        // No-op stub for cross-platform compatibility.
        // On Windows, you could add: __debugbreak(); or DebugBreak();
        // On other platforms, you might use raise(SIGTRAP);
        // For now, just print a message.
        if (WaitDebugAttachIterations == 0) {
            std::cout << "\n[Debugger]: Launch called, but wait set to 0!!";
            return;
        }
        std::cout << "\n[Debugger]: Launch called (no-op stub).";
        uint32_t pid = 1234;
#ifdef _WIN32
        DWORD dpid = GetCurrentProcessId();
        pid = dpid;
#else
        pid = getpid();
#endif

        ThreadManager& TM = ThreadManager::GetInstance();
        for (uint32_t i = 0; i < WaitDebugAttachIterations && !TM.force_stop.load(); i++) {
            // Note this is a delay loop to allow time to Attach Debugger to this process
            // Once attached, you can set i=101 in debugger to exit delay loop


            // Pause execution for 1 second
            std::cout << "\n[Debugger]: Waiting for debugger to attach:" << " pid=" << pid << " iter=" << i;
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }
};


