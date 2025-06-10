#include "CPU.h"
#ifdef _WIN32
#include <conio.h>  // For _getch() on Windows
#else
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#endif
#include <sstream>
#include <regex>
#include <tuple>
#include <iomanip>

struct Instruction {
    int opcode;
    int param1;
    int param2;
};

static Instruction decode(long value) {
    // Use 6 digits for opcode, 12 for param1, 12 for param2 (signed)
    int opcode = (int)(value / 1000000000000LL);
    int param1 = (int)((value / 1000000LL) % 1000000LL);
    int param2 = (int)(value % 1000000LL);
    // Support negative values
    if (param1 >= 500000) param1 -= 1000000;
    if (param2 >= 500000) param2 -= 1000000;
    return {opcode, param1, param2};
}

static long encode(int opcode, int param1, int param2) {
    // Store as: opcode(6) param1(6) param2(6)
    if (param1 < 0) param1 += 1000000;
    if (param2 < 0) param2 += 1000000;
    return (long)opcode * 1000000000000LL + (long)param1 * 1000000LL + (long)param2;
}

CPU::CPU() : memory(11000, 0), m_isHalted(false), isKernelMode(true), debugMode(0) {
    // Initialize memory with zeros
}

void CPU::loadProgram(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return;
    }

    std::string line;
    bool inDataSection = false;
    bool inInstructionSection = false;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (line.find("Begin Data Section") != std::string::npos) {
            inDataSection = true; inInstructionSection = false; continue;
        } else if (line.find("End Data Section") != std::string::npos) {
            inDataSection = false; continue;
        } else if (line.find("Begin Instruction Section") != std::string::npos) {
            inInstructionSection = true; inDataSection = false; continue;
        } else if (line.find("End Instruction Section") != std::string::npos) {
            inInstructionSection = false; continue;
        }
        if (inDataSection) {
            std::istringstream iss(line);
            int address; long value;
            if (iss >> address >> value) memory[address] = value;
        } else if (inInstructionSection) {
            std::istringstream iss(line);
            int instructionNum; std::string opcode; int param1 = 0, param2 = 0;
            if (iss >> instructionNum >> opcode) {
                if (!(iss >> param1)) param1 = 0;
                if (!(iss >> param2)) param2 = 0;
                int op = 0;
                if (opcode == "SET") op = 1;
                else if (opcode == "CPY") op = 2;
                else if (opcode == "CPYI") op = 3;
                else if (opcode == "ADD") op = 4;
                else if (opcode == "ADDI") op = 5;
                else if (opcode == "SUBI") op = 6;
                else if (opcode == "JIF") op = 7;
                else if (opcode == "PUSH") op = 8;
                else if (opcode == "POP") op = 9;
                else if (opcode == "CALL") op = 10;
                else if (opcode == "RET") op = 11;
                else if (opcode == "HLT") op = 12;
                else if (opcode == "USER") op = 13;
                else if (opcode == "SYSCALL") op = 14;
                // Write instructions starting from address 100
                if (instructionNum + 100 < memory.size()) {
                    long encoded_instruction = encode(op, param1, param2); // Store encoded value temporarily
                    int target_address = instructionNum + 100; // Store target address
                    memory[target_address] = encoded_instruction;
                    if (debugMode > 0) {
                        std::cerr << "DEBUG: loadProgram - Writing instruction " << instructionNum << " to memory[" << target_address << "] with value: " << encoded_instruction << std::endl;
                    }
                } else {
                    std::cerr << "Error: Instruction number out of memory bounds: " << instructionNum << std::endl;
                }
            }
        }
    }
    file.close();
    // Set initial Program Counter to the start of instructions (address 100)
    memory[0] = 100;
    if (debugMode > 0) {
        std::cerr << "DEBUG: After loadProgram - memory[0] (PC): " << memory[0] << std::endl;
    }
}

void CPU::execute() {
    if (debugMode > 0) {  // Keep this for execution info
        std::cerr << "Executing instruction... PC: " << memory[0] << std::endl;
    }
    if (m_isHalted) return;

    executeInstruction();

    // Handle debug output
    if (debugMode == 1) {
        printMemoryState();
    }
    else if (debugMode == 2) {
        printMemoryState();
        waitForKeyPress();
    }
}

void CPU::executeInstruction() {
    // Read the current Program Counter address from memory[0]
    long pc_address = memory[0];
    bool pc_was_set_manually_in_this_instruction = false;  // Declare at the start of function

    if (debugMode > 0) {  // Keep this for execution info
        std::cerr << "--- Execute Start --- PC Address: " << pc_address << std::endl;
    }

    // Added debug check for corrupted PC
    if (pc_address == 1000021000007LL) { // Check if PC is the encoded value of SET 21 7
        std::cerr << "FATAL ERROR: Program Counter corrupted! PC is the encoded value of the first instruction." << std::endl;
        m_isHalted = true; // Halt the simulator
        return; // Stop execution immediately
    }

    // Check if the PC address is valid
    if (pc_address < 0 || pc_address >= (long)memory.size()) {
        m_isHalted = true;
        std::cerr << "Invalid PC Address: " << pc_address << std::endl;
        return;
    }
    
    // Read the raw instruction from the memory address indicated by the PC
    long raw = memory[pc_address]; // Talimatı PC adresinden oku (Örn: memory[100])
    if (debugMode > 1) {  // This is a debug message
        std::cerr << "DEBUG: Raw instruction at PC Address " << pc_address << ": " << raw << std::endl;
    }

    memory[3]++; // Increment instruction counter

    // Decode the raw instruction
    Instruction inst = decode(raw);
    int opcode = inst.opcode, param1 = inst.param1, param2 = inst.param2;
    if (debugMode > 0) {  // Keep this for execution info
        std::cerr << "PC Address=" << pc_address << ": opcode=" << opcode << ", p1=" << param1 << ", p2=" << param2 << std::endl;
    }
    if (debugMode > 1) {  // These are debug messages
        if (pc_address == 109) { // Existing debug for checking counts before final HLT check
            std::cerr << "DEBUG: Before OS HLT check (PC 109) - memory[64] (terminated): " << memory[64] << ", memory[65] (total): " << memory[65] << std::endl;
        }
        if (pc_address == 147) { // Added debug for checking values before JIF 64 106
            std::cerr << "DEBUG: Before JIF 64 106 (PC 147) - memory[64]: " << memory[64] << ", memory[15] (syscall type): " << memory[15] << std::endl;
        }
    }

    // Execute the instruction
    switch (opcode) {
        case 1: if (isMemoryAccessValid(param2)) memory[param2] = param1; break;
        case 2: if (isMemoryAccessValid(param1) && isMemoryAccessValid(param2)) memory[param2] = memory[param1]; break;
        case 3: if (isMemoryAccessValid(param1) && isMemoryAccessValid(param2)) { long ind = memory[param1]; if (isMemoryAccessValid(ind)) memory[param2] = memory[ind]; } break;
        case 4: if (isMemoryAccessValid(param1)) memory[param1] += param2; break;
        case 5: if (isMemoryAccessValid(param1) && isMemoryAccessValid(param2)) memory[param1] += memory[param2]; break;
        case 6: if (isMemoryAccessValid(param1) && isMemoryAccessValid(param2)) memory[param2] = memory[param1] - memory[param2]; break;
        case 7: {
            if (debugMode > 1) {
                std::cerr << "DEBUG: JIF instruction at PC=" << memory[0] << std::endl;
                std::cerr << "DEBUG: Checking memory[" << param1 << "] for condition" << std::endl;
            }
            
            // First check if memory access is valid
            if (!isMemoryAccessValid(param1)) {
                if (debugMode > 1) {
                    std::cerr << "DEBUG: JIF - Invalid memory access at address: " << param1 << std::endl;
                }
                break;
            }
            
            // Get the condition value
            long condition = memory[param1];
            if (debugMode > 1) {
                std::cerr << "DEBUG: JIF - Condition value at memory[" << param1 << "] = " << condition << std::endl;
                std::cerr << "DEBUG: JIF - Target address if condition met: " << param2 << std::endl;
            }
            
            // Check if target address is valid (must be in instruction section, >= 100)
            if (param2 < 100) {
                if (debugMode > 1) {
                    std::cerr << "DEBUG: JIF - Invalid target address: " << param2 << " (must be >= 100)" << std::endl;
                }
                break;
            }
            
            // Check if target address is within loaded instruction bounds
            if (param2 >= memory.size()) {
                if (debugMode > 1) {
                    std::cerr << "DEBUG: JIF - Target address out of memory bounds: " << param2 << std::endl;
                }
                break;
            }
            
            // Check if target address contains a valid instruction (opcode != 0)
            Instruction target_inst = decode(memory[param2]);
            if (target_inst.opcode == 0) {
                std::cerr << "DEBUG: JIF - Target address (" << param2 << ") does not contain a valid instruction!" << std::endl;
                break;
            }
            
            if (condition <= 0) {
                std::cerr << "DEBUG: JIF - Condition met (value <= 0). Jumping to address " << param2 << std::endl;
                memory[0] = param2;  // Set PC to target address
                pc_was_set_manually_in_this_instruction = true;
                std::cerr << "DEBUG: JIF - PC set to " << memory[0] << std::endl;
                return;  // Exit immediately after setting PC
            } else {
                std::cerr << "DEBUG: JIF - Condition not met (value > 0). Continuing to next instruction." << std::endl;
            }
            break;
        }
        case 8: if (isMemoryAccessValid(param1)) { memory[1]--; if (isMemoryAccessValid(memory[1])) memory[memory[1]] = memory[param1]; } break; // PC increment below
        case 9: if (isMemoryAccessValid(param1) && isMemoryAccessValid(memory[1])) { memory[param1] = memory[memory[1]]; memory[1]++; } break; // PC increment below
        case 10: handleCall(param1); break;  // CALL
        case 11: handleRet(); break;         // RET
        case 12: m_isHalted = true; std::cerr << "HLT instruction encountered." << std::endl; break; // HLT sets isHalted, preventing PC increment below
        case 13: isKernelMode = false; std::cerr << "Switched to User Mode" << std::endl; break;
        case 14: {
            handleSyscall(param1, param2);
            break;
        }
        default: {
            if (debugMode > 1) {
                std::cerr << "Unknown instruction: " << opcode << std::endl;
            }
            m_isHalted = true;
            break;
        }
    }

    // Increment PC unless it was set manually in this instruction
    if (!pc_was_set_manually_in_this_instruction) {
        memory[0]++;
    }
}

void CPU::handleSyscall(int syscallType, long param) {
    switch (syscallType) {
        case 1: { // PRN
            if (debugMode > 1) {  // This is a debug message
                std::cerr << "DEBUG: SYSCALL PRN (C++ part) executed." << std::endl;
            }
            std::cout << param << std::endl;
            break;
        }
        case 2: { // HLT
            if (debugMode > 0) {  // Keep this for execution info
                std::cerr << "HLT instruction encountered." << std::endl;
            }
            m_isHalted = true;
            break;
        }
        case 3: { // YIELD
            if (debugMode > 1) {  // This is a debug message
                std::cerr << "DEBUG: SYSCALL YIELD (C++ part) called." << std::endl;
            }
            // YIELD implementation
            break;
        }
        default: {
            if (debugMode > 1) {  // This is a debug message
                std::cerr << "DEBUG: handleSyscall called with unknown type: " << syscallType << std::endl;
            }
            break;
        }
    }
}

bool CPU::isMemoryAccessValid(long address) {
    if (address < 0 || address >= memory.size()) return false;
    if (!isKernelMode && address < 1000) return false;
    return true;
}

void CPU::printMemoryState() const {
    // Program sonuçlarını sadece CPU durduğunda ve combined.txt çalıştırıldığında göster
    if ((debugMode == 0 || memory[0] == 0) && memory[100] == 1000021000007LL) {  // memory[100] == 1000021000007LL means combined.txt is running
        std::cout << "\nProgram Results:" << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        
        // Sort thread output (addresses 1000-1012)
        std::cout << "Sort Thread Result (Sorted Array):" << std::endl;
        std::cout << "[";
        for (int i = 1004; i <= 1012; i++) {
            std::cout << memory[i];
            if (i < 1012) std::cout << ", ";
        }
        std::cout << "]" << std::endl;
        
        // Search thread output (addresses 2000-2014)
        std::cout << "\nSearch Thread Result:" << std::endl;
        std::cout << "Array: [";
        for (int i = 2004; i <= 2012; i++) {
            std::cout << memory[i];
            if (i < 2012) std::cout << ", ";
        }
        std::cout << "]" << std::endl;
        std::cout << "Search Result: " << memory[2014] << std::endl;
        
        // Custom thread output (addresses 3000-3005)
        std::cout << "\nCustom Thread Result:" << std::endl;
        std::cout << "Array Size: " << memory[3000] << std::endl;
        std::cout << "Result: " << memory[3002] << std::endl;
    }
    
    std::cout << "\nMemory State:" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    for (size_t i = 0; i < memory.size(); i++) {
        if (memory[i] != 0) {
            std::cout << "Address " << std::setw(4) << i << ": " << std::setw(15) << memory[i];
            if (i == 0) std::cout << " (Program Counter)";
            else if (i == 1) std::cout << " (Stack Pointer)";
            else if (i == 2) std::cout << " (System Call Result)";
            else if (i == 3) std::cout << " (Instruction Counter)";
            else if (i >= 100 && i < 200) {
                std::cout << " (OS Code)";
            } else if (i >= 1000 && i < 2000) {
                std::cout << " (Sort Thread)";
            } else if (i >= 2000 && i < 3000) {
                std::cout << " (Search Thread)";
            } else if (i >= 3000 && i < 4000) {
                std::cout << " (Custom Thread)";
            }
            std::cout << std::endl;
        }
    }
    std::cout << "----------------------------------------" << std::endl;
}

void CPU::waitForKeyPress() const {
    std::cerr << "Press any key to continue..." << std::endl;
#ifdef _WIN32
    _getch();
#else
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    char c;
    while (read(STDIN_FILENO, &c, 1) <= 0);
    fcntl(STDIN_FILENO, F_SETFL, flags);
#endif
}

long CPU::getMemoryValue(int address) const {
    if (address >= 0 && address < memory.size()) {
        return memory[address];
    } else {
        // Return a default value or handle error for invalid address
        std::cerr << "Warning: Attempted to access invalid memory address " << address << std::endl;
        return 0; // Or throw an exception, depending on desired behavior
    }
}

void CPU::setMemoryValue(int address, long value) {
    if (address >= 0 && address < memory.size()) {
        memory[address] = value;
    } else {
        std::cerr << "Warning: Attempted to write to invalid memory address " << address << std::endl;
    }
}

void CPU::switchToUserMode() {
    isKernelMode = false;
    std::cerr << "Switched to User Mode" << std::endl;
}

void CPU::switchToKernelMode() {
    isKernelMode = true;
    std::cerr << "Switched to Kernel Mode" << std::endl;
}

void CPU::handleCall(long target) {
    if (!isMemoryAccessValid(memory[SP])) {
        std::cerr << "Error: Stack overflow in CALL" << std::endl;
        m_isHalted = true;
        return;
    }
    memory[memory[SP]] = memory[PC] + 1;  // Save return address
    memory[SP]--;  // Decrement stack pointer
    memory[PC] = target;  // Set PC to target
}

void CPU::handleRet() {
    if (!isMemoryAccessValid(memory[SP] + 1)) {
        std::cerr << "Error: Stack underflow in RET" << std::endl;
        m_isHalted = true;
        return;
    }
    memory[SP]++;  // Increment stack pointer
    memory[PC] = memory[memory[SP]];  // Restore return address
}

void CPU::printMemoryTrace() const {
    std::cerr << "\nMemory Trace:" << std::endl;
    std::cerr << "PC: " << memory[PC] << std::endl;
    std::cerr << "SP: " << memory[SP] << std::endl;
    std::cerr << "Instruction Count: " << memory[INSTR_CNT] << std::endl;
    std::cerr << "Kernel Mode: " << (isKernelMode ? "Yes" : "No") << std::endl;
    std::cerr << "Current Thread: " << currentThreadId << std::endl;
    
    // Print thread table
    std::cerr << "\nThread Table:" << std::endl;
    for (const auto& thread : threadTable) {
        std::cerr << "Thread " << thread.id << ": ";
        std::cerr << "State=" << (thread.state == READY ? "READY" : 
                                thread.state == RUNNING ? "RUNNING" :
                                thread.state == BLOCKED ? "BLOCKED" : "TERMINATED");
        std::cerr << ", PC=" << thread.pc;
        std::cerr << ", SP=" << thread.sp;
        std::cerr << ", Base=" << thread.baseAddress;
        std::cerr << ", Start=" << thread.startTime;
        std::cerr << ", Exec=" << thread.executionCount << std::endl;
    }
} 