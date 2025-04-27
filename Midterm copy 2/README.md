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

## Design Decisions

1. **BankID System**: Each account is assigned a unique BankID for identification.
2. **Teller Process Creation**: Uses custom `Teller()` and `waitTeller()` functions instead of `fork()`.
3. **Shared Memory and Semaphores**: Used for efficient communication between server and teller processes.
4. **Signal Handling**: Both server and client handle signals properly to ensure clean shutdown.

## Implementation Details

### Bank Server

- Creates and manages bank accounts.
- Assigns BankIDs to new accounts.
- Processes transactions (deposit/withdraw).
- Creates teller processes for each transaction.
- Updates the log file with transaction details.

### Bank Client

- Connects to the server using a named pipe.
- Sends transaction requests to the server.
- Receives responses from the server.
- Handles signals for clean shutdown.

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
   - Withdraw all money from an account.
   - Expected: Account closed, BankID removed.

5. **Signal Handling**:
   - Interrupt client with SIGINT.
   - Expected: Client cleans up and exits gracefully.

6. **Server Shutdown**:
   - Interrupt server with SIGINT.
   - Expected: Server cleans up resources and exits gracefully.

### Test Results

All test cases passed successfully. The system correctly handles:
- Account creation and closure
- Deposits and withdrawals
- Multiple clients and transactions
- Signal interruptions
- Resource cleanup

## How to Run

1. Compile the project:
   ```
   make
   ```

2. Run the server:
   ```
   ./bank_server
   ```

3. Run clients:
   ```
   ./bank_client Client01.file
   ./bank_client Client02.file
   ```

4. Create test client files:
   ```
   make test
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

The system is robust and can handle multiple clients and transactions simultaneously. 