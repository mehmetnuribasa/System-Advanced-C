#ifndef CPU_H
#define CPU_H

#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <iomanip>

enum ThreadState {
    READY,
    RUNNING,
    BLOCKED,
    TERMINATED
};

// Memory-mapped registers (0-20)
enum Register {
    PC = 0,      // Program Counter
    SP = 1,      // Stack Pointer
    RESULT = 2,  // System call result
    INSTR_CNT = 3, // Number of instructions executed
    SYSCALL_TYPE = 15, // System call type
    SYSCALL_PARAM = 16, // System call parameter
    SYSCALL_RESULT = 17, // System call result
    BLOCKED_THREAD = 18, // Currently blocked thread
    TIMER = 19,  // Timer for blocked threads
    OS_STATE = 20 // OS state (0: IDLE, 1: RUNNING)
};

struct Thread {
    int id;
    long startTime;
    long executionCount;
    ThreadState state;
    long pc;
    long sp;
    long baseAddress;  // Base address for thread's memory space
};

class CPU {
public:
    CPU();
    void loadProgram(const std::string& filename);
    void execute();
    bool isHalted() const { return m_isHalted; }
    void setDebugMode(int mode) { debugMode = mode; }
    const std::vector<long>& getMemory() const { return memory; }
    void printMemoryState() const;
    void waitForKeyPress() const;
    long getMemoryValue(int address) const;
    void setMemoryValue(int address, long value);

private:
    std::vector<long> memory;  // Memory space
    bool m_isHalted;
    bool isKernelMode;
    int debugMode;
    std::vector<Thread> threadTable;
    int currentThreadId;
    long instructionCount;

    // Helper functions
    void executeInstruction();
    void handleSyscall(int syscallType, long param);
    bool isMemoryAccessValid(long address);
    void scheduleNextThread();
    void initializeThreadTable();
    void switchToUserMode();
    void switchToKernelMode();
    void handleCall(long target);
    void handleRet();
    void printMemoryTrace() const;
};

#endif // CPU_H 