#!/bin/bash
echo "Building GTU-C312 CPU Simulator..."

# Create build directory if it doesn't exist
mkdir -p build

# Navigate to build directory
cd build

# Run CMake and make
cmake ..
make

# Display usage instructions
echo
echo "GTU-C312 CPU Simulator Usage Instructions"
echo "========================================"
echo
echo "IMPORTANT: All commands must be run from the build directory!"
echo "Current directory: $(pwd)"
echo
echo "Command Format: ./simulate <program_file> -D <debug_mode>"
echo
echo "Debug Modes:"
echo "  0: Show memory contents after CPU halts"
echo "  1: Show memory contents after each instruction"
echo "  2: Show memory contents after each instruction and wait for keypress"
echo
echo "Example Commands (run these from the build directory):"
echo "  1. Run with debug mode 0 (show memory only at end):"
echo "     ./simulate ../combined.txt -D 0"
echo
echo "  2. Run with debug mode 1 (show memory after each instruction):"
echo "     ./simulate ../combined.txt -D 1"
echo
echo "  3. Run with debug mode 2 (show memory and wait for keypress):"
echo "     ./simulate ../combined.txt -D 2"
echo
echo "  4. Run sample program with debug mode 1:"
echo "     ./simulate ../sample.txt -D 1"
echo
echo "  5. Run test program with debug mode 2:"
echo "     ./simulate ../test.txt -D 2"
echo
echo "Note: The program file must contain both OS code and thread programs."
echo "      Debug output will be sent to standard error stream."
echo
echo "To run commands from the project root directory, use:"
echo "  cd build && ./simulate ../combined.txt -D 0"
echo

cd .. 