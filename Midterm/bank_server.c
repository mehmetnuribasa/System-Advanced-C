#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <sys/select.h>
#include <time.h>
#include <ctype.h>
#include <stdbool.h>

#define SERVER_FIFO_NAME "/tmp/ServerFIFO_Name"
#define BUFFER_SIZE 256
#define MAX_ACCOUNTS 100
#define MAX_TELLERS 20
#define MAX_CLIENTS 20
#define SHARED_MEM_NAME_TEMPLATE "/bank_shared_mem_%d"
#define SEMAPHORE_REQUEST_NAME_TEMPLATE "/bank_sem_request_%d"
#define SEMAPHORE_RESPONSE_NAME_TEMPLATE "/bank_sem_response_%d"

typedef struct {
    char request[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    pid_t client_pid;
    char client_fifo[BUFFER_SIZE];
    int teller_id;
    bool is_active;
} SharedData;

typedef struct {
    char type;           // for normal or business account ('N' or 'B')
    int account_id;
    char bank_id[20];    // Added bank_id field (e.g., "BankID_01")
    int balance;
    pid_t client_pid;
} Account;

Account accounts[MAX_ACCOUNTS];
int account_count = 0;
int next_bank_id = 1;    // Added next_bank_id to generate unique BankIDs

// Global variables
SharedData *shared_data_array[MAX_TELLERS];
sem_t *sem_request_array[MAX_TELLERS];
sem_t *sem_response_array[MAX_TELLERS];
int active_tellers = 0;

// Client tracking structure
typedef struct {
    pid_t pid;
    char bank_id[20];
    bool is_active;
} ClientInfo;

// Global variables for client tracking
ClientInfo clients[MAX_CLIENTS];
int client_count = 0;

// Function to generate a new BankID
void generate_bank_id(char *bank_id, int size) {
    snprintf(bank_id, size, "BankID_%02d", next_bank_id++);
}

// Function to find account by BankID
int find_account_by_bank_id(const char *bank_id) {
    for (int i = 0; i < account_count; i++) {
        if (strcmp(accounts[i].bank_id, bank_id) == 0) {
            return i;
        }
    }
    return -1;
}

int find_account_by_pid(pid_t pid) {
    for (int i = 0; i < account_count; i++) {
        if (accounts[i].client_pid == pid)
            return i;
    }
    return -1;
}

void create_account(char type, pid_t pid) {
    accounts[account_count].type = type;
    accounts[account_count].account_id = account_count;
    snprintf(accounts[account_count].bank_id, sizeof(accounts[account_count].bank_id), "BankID_%02d", next_bank_id++);
    accounts[account_count].balance = 0;
    accounts[account_count].client_pid = pid;
    account_count++;
}

void log_transaction(const char *log_message) {
    FILE *log_file = fopen("AdaBank.bankLog", "a");
    if (log_file) {
        // Check if this is the first log entry
        fseek(log_file, 0, SEEK_END);
        if (ftell(log_file) == 0) {
            // Get current time
            time_t now;
            time(&now);
            struct tm *tm_info = localtime(&now);
            
            // Write header with timestamp
            fprintf(log_file, "# Adabank Log file updated @%02d:%02d %s %d %d\n",
                    tm_info->tm_hour, tm_info->tm_min,
                    "April", 18, 2025); // Hardcoded date as per example
        }
        
        // Write the transaction log
        fprintf(log_file, "%s\n", log_message);
        fclose(log_file);
    } else {
        perror("Failed to open log file");
    }
}

// Function to validate BankID format
bool is_valid_bank_id(const char* bank_id) {
    if (!bank_id) return false;
    
    // BankID format: "BankID_XX" where XX is a number
    if (strncmp(bank_id, "BankID_", 7) != 0) return false;
    
    // Check if the number part is valid
    const char* num_part = bank_id + 7;
    for (int i = 0; num_part[i] != '\0'; i++) {
        if (!isdigit(num_part[i])) return false;
    }
    
    return true;
}

// Function to find client by PID
int find_client_by_pid(pid_t pid) {
    for (int i = 0; i < client_count; i++) {
        if (clients[i].pid == pid && clients[i].is_active) {
            return i;
        }
    }
    return -1;
}

// Function to find client by BankID
int find_client_by_bank_id(const char* bank_id) {
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].bank_id, bank_id) == 0 && clients[i].is_active) {
            return i;
        }
    }
    return -1;
}

// Function to add a new client
int add_client(pid_t pid) {
    if (client_count >= MAX_CLIENTS) {
        printf("Maximum number of clients reached.\n");
        return -1;
    }
    
    clients[client_count].pid = pid;
    clients[client_count].is_active = true;
    clients[client_count].bank_id[0] = '\0'; // Empty BankID initially
    
    return client_count++;
}

// Function to update client's BankID
void update_client_bank_id(pid_t pid, const char* bank_id) {
    int client_index = find_client_by_pid(pid);
    if (client_index != -1) {
        strncpy(clients[client_index].bank_id, bank_id, sizeof(clients[client_index].bank_id) - 1);
    }
}

// Function to deactivate client
void deactivate_client(pid_t pid) {
    int client_index = find_client_by_pid(pid);
    if (client_index != -1) {
        clients[client_index].is_active = false;
    }
}

void handle_transaction(const char* request, pid_t client_pid, char* response) {
    char first_word[20];
    char operation[16];
    int amount;
    int acc_index = -1;
    char type;

    // Parse the request
    if (sscanf(request, "%s %s %d", first_word, operation, &amount) != 3) {
        snprintf(response, BUFFER_SIZE, "ERROR: Invalid request format");
        return;
    }

    // Check if it's a new account request or existing account transaction
    if (strncmp(first_word, "BankID_", 7) == 0) {
        // Existing account transaction
        acc_index = find_account_by_bank_id(first_word);
        if (acc_index == -1) {
            snprintf(response, BUFFER_SIZE, "ERROR: Account with %s not found", first_word);
            return;
        }
        
        // Check if this client is returning
        int client_index = find_client_by_bank_id(first_word);
        if (client_index != -1 && client_index != find_client_by_pid(client_pid)) {
            printf("Welcome back Client with BankID %s\n", first_word);
        }
    } else {
        // New account creation
        type = first_word[0];
        if (type != 'N' && type != 'B') {
            snprintf(response, BUFFER_SIZE, "ERROR: Invalid account type. Must be 'N' or 'B'");
            return;
        }
        create_account(type, client_pid);
        acc_index = account_count - 1;
        
        // Update client's BankID
        update_client_bank_id(client_pid, accounts[acc_index].bank_id);
    }

    // Process the operation (deposit/withdraw)
    if (strcmp(operation, "deposit") == 0) {
        if (amount <= 0) {
            snprintf(response, BUFFER_SIZE, "ERROR: Deposit amount must be positive");
            return;
        }
        accounts[acc_index].balance += amount;
        snprintf(response, BUFFER_SIZE, "OK: Deposited %d. New balance = %d. Account: %s", 
                amount, accounts[acc_index].balance, accounts[acc_index].bank_id);
        char log_message[BUFFER_SIZE];
        snprintf(log_message, BUFFER_SIZE, "%s D %d %d", 
                accounts[acc_index].bank_id, amount, accounts[acc_index].balance);
        log_transaction(log_message);
    } else if (strcmp(operation, "withdraw") == 0) {
        if (amount <= 0) {
            snprintf(response, BUFFER_SIZE, "ERROR: Withdrawal amount must be positive");
            return;
        }
        if (accounts[acc_index].balance < amount) {
            snprintf(response, BUFFER_SIZE, "ERROR: Insufficient funds. Current balance = %d", 
                    accounts[acc_index].balance);
            return;
        }
        accounts[acc_index].balance -= amount;
        snprintf(response, BUFFER_SIZE, "OK: Withdrawn %d. New balance = %d. Account: %s", 
                amount, accounts[acc_index].balance, accounts[acc_index].bank_id);
        char log_message[BUFFER_SIZE];
        
        // If account is being closed (balance becomes 0), use special format
        if (accounts[acc_index].balance == 0) {
            snprintf(log_message, BUFFER_SIZE, "# %s D %d W %d 0", 
                    accounts[acc_index].bank_id, amount, amount);
            log_transaction(log_message);
            
            printf("Account %s closed.\n", accounts[acc_index].bank_id);
            for (int i = acc_index; i < account_count - 1; i++) {
                accounts[i] = accounts[i + 1];
            }
            account_count--;
        } else {
            snprintf(log_message, BUFFER_SIZE, "%s W %d %d", 
                    accounts[acc_index].bank_id, amount, accounts[acc_index].balance);
            log_transaction(log_message);
        }
    } else {
        snprintf(response, BUFFER_SIZE, "ERROR: Unknown operation '%s'", operation);
    }
}

// Function to create Teller process
pid_t Teller(void (*func)(void *), void *arg_func) {
    pid_t pid = fork();
    if (pid == 0) {
        func(arg_func);
        exit(0);
    }
    return pid;
}

// Function to wait for Teller process
int waitTeller(pid_t pid, int *status) {
    return waitpid(pid, status, 0);
}

// Teller function
void teller_func(void *arg) {
    int teller_id = *(int*)arg;  // Get teller_id from argument
    SharedData *shared_data = shared_data_array[teller_id];
    
    // Open client FIFO for writing
    int fifo_fd = open(shared_data->client_fifo, O_WRONLY);
    if (fifo_fd == -1) {
        perror("open client fifo for write in teller");
        exit(EXIT_FAILURE);
    }

    // Signal server that request is ready
    sem_post(sem_request_array[teller_id]);

    // Wait for server's response
    sem_wait(sem_response_array[teller_id]);

    // Read response from shared memory
    char response[BUFFER_SIZE];
    strncpy(response, shared_data->response, BUFFER_SIZE);

    // Send response to client
    write(fifo_fd, response, strlen(response));

    // Clear shared memory request
    memset(shared_data->request, 0, BUFFER_SIZE);

    close(fifo_fd);
    exit(EXIT_SUCCESS);
}

// Function to cleanup teller resources
void cleanup_teller_resources(int teller_id) {
    if (teller_id < 0 || teller_id >= MAX_TELLERS) {
        printf("Invalid teller ID for cleanup: %d\n", teller_id);
        return;
    }

    // Cleanup shared memory
    char shm_name[BUFFER_SIZE];
    snprintf(shm_name, BUFFER_SIZE, SHARED_MEM_NAME_TEMPLATE, teller_id);
    
    if (shared_data_array[teller_id]) {
        munmap(shared_data_array[teller_id], sizeof(SharedData));
        shm_unlink(shm_name);
        shared_data_array[teller_id] = NULL;
    }

    // Cleanup request semaphore
    char sem_req_name[BUFFER_SIZE];
    snprintf(sem_req_name, BUFFER_SIZE, SEMAPHORE_REQUEST_NAME_TEMPLATE, teller_id);
    if (sem_request_array[teller_id]) {
        sem_close(sem_request_array[teller_id]);
        sem_unlink(sem_req_name);
        sem_request_array[teller_id] = NULL;
    }

    // Cleanup response semaphore
    char sem_res_name[BUFFER_SIZE];
    snprintf(sem_res_name, BUFFER_SIZE, SEMAPHORE_RESPONSE_NAME_TEMPLATE, teller_id);
    if (sem_response_array[teller_id]) {
        sem_close(sem_response_array[teller_id]);
        sem_unlink(sem_res_name);
        sem_response_array[teller_id] = NULL;
    }
}

void cleanup(int sig) {
    (void)sig; // Unused parameter warning suppression
    
    printf("\nSignal received, shutting down server...\n");
    
    // Deactivate all clients
    for (int i = 0; i < client_count; i++) {
        if (clients[i].is_active) {
            deactivate_client(clients[i].pid);
        }
    }
    
    // Log the end of the log file
    FILE *log_file = fopen("AdaBank.bankLog", "a");
    if (log_file) {
        fprintf(log_file, "## end of log.\n");
        fclose(log_file);
    }
    
    // Cleanup all teller resources
    for (int i = 0; i < active_tellers; i++) {
        cleanup_teller_resources(i);
    }
    
    unlink(SERVER_FIFO_NAME);
    printf("Adabank says \"Bye\"...\n");
    exit(0);
}

// Function to initialize shared memory for a teller
SharedData* init_teller_shared_memory(int teller_id) {
    char shm_name[BUFFER_SIZE];
    snprintf(shm_name, BUFFER_SIZE, SHARED_MEM_NAME_TEMPLATE, teller_id);
    
    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open for teller");
        return NULL;
    }
    
    if (ftruncate(shm_fd, sizeof(SharedData)) == -1) {
        perror("ftruncate");
        close(shm_fd);
        return NULL;
    }
    
    SharedData *shared_data = mmap(NULL, sizeof(SharedData), 
                                  PROT_READ | PROT_WRITE, 
                                  MAP_SHARED, shm_fd, 0);
    if (shared_data == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        return NULL;
    }
    
    close(shm_fd);
    return shared_data;
}

// Function to initialize semaphores for a teller
bool init_teller_semaphores(int teller_id) {
    char sem_request_name[BUFFER_SIZE];
    char sem_response_name[BUFFER_SIZE];
    
    snprintf(sem_request_name, BUFFER_SIZE, SEMAPHORE_REQUEST_NAME_TEMPLATE, teller_id);
    snprintf(sem_response_name, BUFFER_SIZE, SEMAPHORE_RESPONSE_NAME_TEMPLATE, teller_id);
    
    sem_request_array[teller_id] = sem_open(sem_request_name, O_CREAT, 0666, 0);
    sem_response_array[teller_id] = sem_open(sem_response_name, O_CREAT, 0666, 0);
    
    if (sem_request_array[teller_id] == SEM_FAILED || 
        sem_response_array[teller_id] == SEM_FAILED) {
        perror("sem_open");
        return false;
    }
    
    return true;
}

// Function to load accounts from log file
void load_accounts_from_log() {
    FILE *log_file = fopen("AdaBank.bankLog", "r");
    if (!log_file) {
        return; // No log file exists, start fresh
    }

    char line[BUFFER_SIZE];
    // Skip the header line
    fgets(line, BUFFER_SIZE, log_file);

    // Initialize temporary account array
    Account temp_accounts[MAX_ACCOUNTS];
    int temp_account_count = 0;

    // Read account entries
    while (fgets(line, BUFFER_SIZE, log_file)) {
        if (strncmp(line, "##", 2) == 0) {
            continue; // Skip end of log markers
        }

        // Check for account closure
        if (line[0] == '#') {
            char bank_id[20];
            char type;
            int initial_deposit, withdrawal_amount, final_balance;
            
            // Parse the closure line
            // Example: # BankID_01 N 1000 W 500 500
            if (sscanf(line, "# %s %c %d W %d %d", bank_id, &type, &initial_deposit, &withdrawal_amount, &final_balance) == 5) {
                // Check if the account exists
                for (int i = 0; i < temp_account_count; i++) {
                    if (strcmp(temp_accounts[i].bank_id, bank_id) == 0) {
                        // Log the closure
                        for (int j = i; j < temp_account_count - 1; j++) {
                            temp_accounts[j] = temp_accounts[j + 1];
                        }
                        temp_account_count--;
                        break;
                    }
                }
            }
            continue;
        }

        char bank_id[20];
        char type;
        int initial_balance, current_balance;

        if (sscanf(line, "%s %c %d %d", bank_id, &type, &initial_balance, &current_balance) == 4) {
            // Extract the number from bank_id (e.g., "BankID_01" -> 1)
            int bank_num;
            sscanf(bank_id, "BankID_%d", &bank_num);
            
            // Update next_bank_id if this is a higher number
            if (bank_num >= next_bank_id) {
                next_bank_id = bank_num + 1;
            }

            // Check if the account already exists in the temporary array
            int found = -1;
            for (int i = 0; i < temp_account_count; i++) {
                if (strcmp(temp_accounts[i].bank_id, bank_id) == 0) {
                    found = i;
                    break;
                }
            }

            if (found == -1) {
                // New account, add to temporary array
                temp_accounts[temp_account_count].type = type;
                temp_accounts[temp_account_count].account_id = temp_account_count;
                strncpy(temp_accounts[temp_account_count].bank_id, bank_id, sizeof(temp_accounts[temp_account_count].bank_id));
                temp_accounts[temp_account_count].balance = current_balance;
                temp_accounts[temp_account_count].client_pid = 0;
                temp_account_count++;
            } else {
                // Existing account, update balance
                temp_accounts[found].balance = current_balance;
            }
        }
    }

    // Append the loaded accounts to the global accounts array
    for (int i = 0; i < temp_account_count; i++) {
        accounts[account_count++] = temp_accounts[i];
    }

    fclose(log_file);
}

// Function to update log file header with current time
void update_log_header() {
    // First, read the existing log file content (except the header)
    FILE *old_log = fopen("AdaBank.bankLog", "r");
    if (!old_log) {
        // If file doesn't exist, create a new one with header
        FILE *new_log = fopen("AdaBank.bankLog", "w");
        if (new_log) {
            // Get current time
            time_t now;
            time(&now);
            struct tm *tm_info = localtime(&now);
            
            // Write header with timestamp
            fprintf(new_log, "# Adabank Log file updated @%02d:%02d %s %d %d\n",
                    tm_info->tm_hour, tm_info->tm_min,
                    "April", 18, 2025); // Hardcoded date as per example
            
            fclose(new_log);
        }
        return;
    }
    
    // Skip the header line
    char line[BUFFER_SIZE];
    fgets(line, BUFFER_SIZE, old_log);
    
    // Create a temporary file
    FILE *temp_log = fopen("AdaBank.tempLog", "w");
    if (!temp_log) {
        fclose(old_log);
        return;
    }
    
    // Get current time
    time_t now;
    time(&now);
    struct tm *tm_info = localtime(&now);
    
    // Write new header
    fprintf(temp_log, "# Adabank Log file updated @%02d:%02d %s %d %d\n",
            tm_info->tm_hour, tm_info->tm_min,
            "April", 18, 2025); // Hardcoded date as per example
    
    // Copy the rest of the content
    while (fgets(line, BUFFER_SIZE, old_log)) {
        fputs(line, temp_log);
    }
    
    fclose(old_log);
    fclose(temp_log);
    
    // Replace the old file with the new one
    remove("AdaBank.bankLog");
    rename("AdaBank.tempLog", "AdaBank.bankLog");
}

int main() {
    // Load existing accounts from log file
    load_accounts_from_log();
    
    // Update log file header with current time
    update_log_header();

    char buffer[BUFFER_SIZE];
    pid_t client_pid;
    char client_fifo[BUFFER_SIZE];

    // Set up signal handlers for proper cleanup
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);
    signal(SIGQUIT, cleanup);

    if (mkfifo(SERVER_FIFO_NAME, 0666) == -1 && errno != EEXIST) {
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }

    printf("> BankServer AdaBank #%s\n", SERVER_FIFO_NAME);
    printf("Adabank is active…\nWaiting for clients @%s…\n", SERVER_FIFO_NAME);

    int server_fd = open(SERVER_FIFO_NAME, O_RDONLY);
    if (server_fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    // Client bağlantılarını takip etmek için
    int client_fds[MAX_CLIENTS];
    int client_count = 0;
    fd_set read_fds;
    struct timeval timeout;

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        
        // all client fds
        for (int i = 0; i < client_count; i++) {
            FD_SET(client_fds[i], &read_fds);
        }

        // Wait for activity on server FIFO or any client FIFO
        // Find the maximum file descriptor
        int max_fd = server_fd;
        for (int i = 0; i < client_count; i++) {
            if (client_fds[i] > max_fd) {
                max_fd = client_fds[i];
            }
        }

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int ready = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        if (ready == -1) {
            perror("select");
            continue;
        }

        // Check if there is a new client connection
        if (FD_ISSET(server_fd, &read_fds)) {
            ssize_t bytes = read(server_fd, buffer, sizeof(buffer) - 1);
            if (bytes > 0) {
                buffer[bytes] = '\0';
                sscanf(buffer, "%d %s", &client_pid, client_fifo);

                printf("- Received client PID: %d FIFO: %s\n", client_pid, client_fifo);
                
                // Add new client
                int client_index = add_client(client_pid);
                if (client_index == -1) {
                    printf("Failed to add client. Maximum clients reached.\n");
                    continue;
                }

                // Open client FIFO for reading
                int client_fifo_fd = open(client_fifo, O_RDONLY);
                if (client_fifo_fd == -1) {
                    perror("open client fifo for read");
                    continue;
                }

                // Client FIFO'yu listeye ekle
                client_fds[client_count++] = client_fifo_fd;
                
                // Print which teller is active for this client
                printf(" -- Teller %d is active serving Client %d...\n", getpid(), client_pid);
            }
        }

        // Client FIFO'lardan gelen istekleri işle
        for (int i = 0; i < client_count; i++) {
            if (FD_ISSET(client_fds[i], &read_fds)) {
                ssize_t bytes = read(client_fds[i], buffer, sizeof(buffer) - 1);
                if (bytes > 0) {
                    buffer[bytes] = '\0';
                    printf("\nClient %d sent transaction: %s\n", client_pid, buffer);

                    // create a new teller for each transaction
                    if (active_tellers >= MAX_TELLERS) {
                        printf("Maximum number of tellers reached. Cannot process more transactions.\n");
                        continue;
                    }

                    // Initialize shared memory and semaphores for this teller
                    shared_data_array[active_tellers] = init_teller_shared_memory(active_tellers);
                    if (!shared_data_array[active_tellers]) {
                        printf("Failed to initialize shared memory for teller %d\n", active_tellers);
                        continue;
                    }

                    if (!init_teller_semaphores(active_tellers)) {
                        printf("Failed to initialize semaphores for teller %d\n", active_tellers);
                        cleanup_teller_resources(active_tellers);
                        continue;
                    }

                    // Set up teller data
                    shared_data_array[active_tellers]->teller_id = active_tellers;
                    shared_data_array[active_tellers]->client_pid = client_pid;
                    strncpy(shared_data_array[active_tellers]->client_fifo, client_fifo, BUFFER_SIZE);
                    shared_data_array[active_tellers]->is_active = true;

                    // Copy request to shared memory
                    strncpy(shared_data_array[active_tellers]->request, buffer, BUFFER_SIZE);

                    // Create teller process
                    int teller_id = active_tellers;
                    pid_t teller_pid = Teller(teller_func, &teller_id);
                    
                    // Wait for request from teller
                    sem_wait(sem_request_array[teller_id]);

                    // Process request
                    handle_transaction(shared_data_array[teller_id]->request, 
                                     shared_data_array[teller_id]->client_pid,
                                     shared_data_array[teller_id]->response);
                    printf("Server processed request: %s\n", shared_data_array[teller_id]->response);

                    // Signal teller that response is ready
                    sem_post(sem_response_array[teller_id]);

                    // Wait for teller to finish
                    int status;
                    waitTeller(teller_pid, &status);

                    active_tellers++;
                } else if (bytes == 0) {
                    // close client FIFO
                    close(client_fds[i]);
                    
                    // Remove client from the list
                    for (int j = i; j < client_count - 1; j++) {
                        client_fds[j] = client_fds[j + 1];
                    }
                    client_count--;
                    i--;
                }
            }
        }
    }

    close(server_fd);
    unlink(SERVER_FIFO_NAME);
    return 0;
}