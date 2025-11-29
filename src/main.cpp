#include <cpu/process/process.hpp>
#include <cpu/process/exec.hpp>
#include <cpu/process/scheduler.hpp>
#include <cpu/syscall/syscall.hpp>
#include <cpu/gdt/gdt.hpp>

extern GDT* gdt;

int main(){
    const char* argv[] = { "/shell.elf", nullptr };
    Process* userProc = ProcessExecutor::loadUserBinaryWithArgs("/shell.elf", 1, argv);
    
    Scheduler::get().addProcess(userProc);
    Scheduler::get().setCurrentProcess(userProc);
    userProc->setState(ProcessState::Running);
        
    gdt->setKernelStack(userProc->getKernelStack());
    Syscall::get().setKernelStack(userProc->getKernelStack());
        
    asm volatile("sti");
        
    switchContext(nullptr, userProc->getContext());

    for(;;);
    return 0;
}