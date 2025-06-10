# GTU-C312 CPU Simulator

This project implements a simple CPU simulator (GTU-C312) with a custom instruction set and a basic operating system that runs on this CPU.

## Project Structure

- `CPU.cpp` and `CPU.h`: CPU implementation with instruction set
- `main.cpp`: Main program that runs the simulation
- `os.txt`: Operating system code in GTU-C312 assembly
- `sort_thread.txt`: Thread that implements bubble sort
- `search_thread.txt`: Thread that implements linear search
- `custom_thread.txt`: Custom thread implementation
- `combined.txt`: Combined OS and thread programs
- `sample.txt`: Sample program
- `test.txt`: Test program

## Building the Project

```bash
./build.sh
```

## Running the Simulator

**IMPORTANT**: All simulation commands must be run from the `build` directory!

### Method 1: From build directory
```bash
cd build
./simulate <program_file> -D <debug_mode>
```

### Method 2: From project root directory
```bash
cd build && ./simulate <program_file> -D <debug_mode>
```

### Debug Modes

1. **Mode 0**: Shows memory contents only after the CPU halts
   ```bash
   # From build directory
   ./simulate ../combined.txt -D 0
   
   # From project root
   cd build && ./simulate ../combined.txt -D 0
   ```

2. **Mode 1**: Shows memory contents after each instruction
   ```bash
   # From build directory
   ./simulate ../combined.txt -D 1
   
   # From project root
   cd build && ./simulate ../combined.txt -D 1
   ```

3. **Mode 2**: Shows memory contents after each instruction and waits for keypress
   ```bash
   # From build directory
   ./simulate ../combined.txt -D 2
   
   # From project root
   cd build && ./simulate ../combined.txt -D 2
   ```

### Example Programs

1. **Sample Program**: Simple program that adds numbers from 1 to 10
   ```bash
   # From build directory
   ./simulate ../sample.txt -D 1
   
   # From project root
   cd build && ./simulate ../sample.txt -D 1
   ```

2. **Test Program**: Program to test CPU functionality
   ```bash
   # From build directory
   ./simulate ../test.txt -D 2
   
   # From project root
   cd build && ./simulate ../test.txt -D 2
   ```

3. **Combined Program**: OS with all threads
   ```bash
   # From build directory
   ./simulate ../combined.txt -D 1
   
   # From project root
   cd build && ./simulate ../combined.txt -D 1
   ```

## Debug Output

- Debug output is sent to the standard error stream
- Memory contents are shown in the format: `address: value`
- In debug mode 2, press any key to continue execution

## Thread Programs

1. **Sort Thread**: Implements bubble sort algorithm
   - Sorts N numbers in increasing order
   - Prints sorted numbers at the end

2. **Search Thread**: Implements linear search
   - Searches for a key in N numbers
   - Prints the found position

3. **Custom Thread**: Custom implementation
   - Contains loops and print calls
   - Demonstrates thread functionality

## Memory Layout

- 0-20: Registers
- 21-999: OS Data and Code
- 1000-1999: Thread 1
- 2000-2999: Thread 2
- 3000-3999: Thread 3
- 4000-4999: Thread 4
- 5000-5999: Thread 5
- 6000-6999: Thread 6
- 7000-7999: Thread 7
- 8000-8999: Thread 8
- 9000-9999: Thread 9
- 10000-10999: Thread 10

## Thread Table Structure

Each thread entry in the table contains:
- Thread ID
- Start Time (instruction count at start)
- Execution Count
- State (READY, RUNNING, BLOCKED, TERMINATED)
- Program Counter (PC)
- Stack Pointer (SP)
- Base Address

## Notes

- The program file must contain both OS code and thread programs
- In user mode, threads can only access memory addresses 1000 and above
- System calls automatically switch to kernel mode
- The OS implements round-robin scheduling

## Program File Format

Program files should follow this format:

```
Begin Data Section
<memory_address> <value>
...
End Data Section

Begin Instruction Section
<instruction_number> <instruction>
...
End Instruction Section
```

## Instruction Set

The GTU-C312 CPU supports the following instructions:

1. SET B A - Set memory location A with value B
2. CPY A1 A2 - Copy memory location A1 to A2
3. CPYI A1 A2 - Indirect copy
4. ADD A B - Add B to memory location A
5. ADDI A1 A2 - Indirect add
6. SUBI A1 A2 - Indirect subtraction
7. JIF A C - Jump to C if memory location A <= 0
8. PUSH A - Push memory A onto stack
9. POP A - Pop value from stack into memory A
10. CALL C - Call subroutine at C
11. RET - Return from subroutine
12. HLT - Halt CPU
13. USER - Switch to user mode
14. SYSCALL - System call

## System Calls

1. PRN A - Print contents of memory location A
2. HLT - Halt thread
3. YIELD - Yield CPU to next thread

## Memory Layout

- 0-20: CPU Registers
- 0-999: OS area (kernel mode only)
- 1000+: User space 