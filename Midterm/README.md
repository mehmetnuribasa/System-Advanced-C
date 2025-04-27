# Bank Simulator - Midterm Project

## System Architecture

This project implements a banking simulator using a client-server architecture with inter-process communication. The system consists of the following components:

1. **Bank Server**: The main server process that manages bank accounts and transactions.
2. **Bank Client**: Client processes that connect to the server and perform transactions.
3. **Teller Processes**: Created by the server for each client transaction, responsible for processing client requests.

### Communication Mechanisms

- **Server-Client Communication**: Uses named pipes (FIFOs) for initial connection.
- **Teller-Client Communication**: Uses client-generated named pipes.
- **Server-Teller Communication**: Uses shared memory and semaphores for synchronization.

### Data Structures

- **Account**: Stores account information including type, ID, BankID, balance, and client PID.
- **SharedData**: Used for communication between server and teller processes.
- **ClientInfo**: Tracks client information including PID, BankID, and active status.

## Design Decisions

1. **BankID System**: Each account is assigned a unique BankID for identification.
2. **Teller Process Creation**: Uses custom `Teller()` and `waitTeller()` functions instead of `fork()`.
3. **Shared Memory and Semaphores**: Used for efficient communication between server and teller processes.
4. **Signal Handling**: Both server and client handle signals properly to ensure clean shutdown.
5. **Log File Management**: Transactions are logged to `AdaBank.bankLog` file.
6. **Account Loading**: Server loads existing accounts from log file on startup.

## Implementation Details

### Bank Server

- Creates and manages bank accounts.
- Assigns BankIDs to new accounts.
- Processes transactions (deposit/withdraw).
- Creates teller processes for each transaction.
- Updates the log file with transaction details.
- Loads existing accounts from log file on startup.
- Handles multiple clients simultaneously.

### Bank Client

- Connects to the server using a named pipe.
- Sends transaction requests to the server.
- Receives responses from the server.
- Handles signals for clean shutdown.
- Processes transactions from a client file.

### Teller Process

- Receives client requests.
- Forwards requests to the server.
- Receives server responses.
- Sends responses back to the client.

## Test Plan

### Test Cases

1. **Single Client, Single Transaction**:
   - Create a new account and deposit money.
   - Expected: Account created with BankID, balance updated.

2. **Single Client, Multiple Transactions**:
   - Create an account, deposit money, withdraw money.
   - Expected: Account balance updated correctly.

3. **Multiple Clients, Multiple Transactions**:
   - Run multiple clients simultaneously with various transactions.
   - Expected: All transactions processed correctly.

4. **Account Closure**:
   - Withdraw all money from a non-existent account.
   - Expected: Error message returned.

5. **Signal Handling**:
   - Interrupt client with SIGINT.
   - Expected: Client cleans up and exits gracefully.

6. **Server Shutdown**:
   - Interrupt server with SIGINT.
   - Expected: Server cleans up resources and exits gracefully.

7. **Log File Loading**:
   - Start server after previous transactions.
   - Expected: Server loads existing accounts from log file.

### Test Results

All test cases passed successfully. The system correctly handles:
- Account creation and closure
- Deposits and withdrawals
- Multiple clients and transactions
- Signal interruptions
- Resource cleanup
- Log file management

## How to Run

1. Compile the project:
   ```
   make
   ```

2. Run the server and test clients in one command:
   ```
   make run
   ```

   This will compile the project and run the test_multiple_clients.sh script.

3. To run the server manually:
   ```
   ./bank_server
   ```

4. To run a client manually:
   ```
   ./bank_client Client01.file
   ```

5. To clean up compiled files:
   ```
   make clean
   ```

## Conclusion

The Bank Simulator successfully implements all required functionality, including:
- Client-server architecture
- Teller process creation
- BankID system
- Transaction processing
- Log file management
- Signal handling
- Resource cleanup
- Multiple client handling

The system is robust and can handle multiple clients and transactions simultaneously. The server loads existing accounts from the log file on startup, ensuring continuity between server restarts. 