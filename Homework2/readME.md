# Homework 2: Parent-Child Process Communication

## Description
This program demonstrates inter-process communication between a parent process, two child processes, and a daemon process using FIFOs (named pipes) and signals.

## Requirements
- GCC (GNU Compiler Collection)
- Linux environment (for FIFOs and signals)

## How to Build
1. Run the `make` command to compile the program:
   ```bash
   make
   ```

## How to Run
1. After building, run the program with two integer arguments:
   ```bash
   ./parent <int1> <int2>
   ```

   For example:
   ```bash
   ./parent 10 902
   ```

2. The program will:
   - Create FIFOs for communication.
   - Spawn a daemon process and two child processes.
   - Perform inter-process communication to compare and log numbers.

## Cleaning Up
To remove the compiled executable and any temporary files, run:
```bash
make clean
```

## Notes
- Ensure you have write permissions for `/tmp` to create the FIFOs.
- Log files (`daemon_log.txt`, `daemon_output.log`, `daemon_error.log`) will be created in the project directory.