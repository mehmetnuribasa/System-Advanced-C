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
    generate_bank_id(accounts[account_count].bank_id, sizeof(accounts[account_count].bank_id));
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

// BankID formatını kontrol eden fonksiyon
bool is_valid_bank_id(const char* bank_id) {
    if (!bank_id) return false;
    
    // BankID_XX formatını kontrol et
    if (strncmp(bank_id, "BankID_", 7) != 0) return false;
    
    // XX kısmının sayı olduğunu kontrol et
    const char* num_part = bank_id + 7;
    for (int i = 0; num_part[i] != '\0'; i++) {
        if (!isdigit(num_part[i])) return false;
    }
    
    return true;
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
    } else {
        // New account creation
        type = first_word[0];
        if (type != 'N' && type != 'B') {
            snprintf(response, BUFFER_SIZE, "ERROR: Invalid account type. Must be 'N' or 'B'");
            return;
        }
        create_account(type, client_pid);
        acc_index = account_count - 1;
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
        if (accounts[acc_index].balance >= amount) {
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
            snprintf(response, BUFFER_SIZE, "ERROR: Insufficient funds. Current balance = %d", 
                    accounts[acc_index].balance);
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
    
    // Open client FIFO for reading
    int fifo_fd = open(shared_data->client_fifo, O_RDONLY | O_NONBLOCK);
    if (fifo_fd == -1) {
        perror("open client fifo for read in teller");
        exit(EXIT_FAILURE);
    }

    char request[BUFFER_SIZE];
    
    // Use select to wait for data on the FIFO with a timeout
    fd_set read_fds;
    struct timeval timeout;

    FD_ZERO(&read_fds);
    FD_SET(fifo_fd, &read_fds);

    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    int ready = select(fifo_fd + 1, &read_fds, NULL, NULL, &timeout);
    if (ready == -1) {
        perror("select");
        close(fifo_fd);
        exit(EXIT_FAILURE);
    } else if (ready == 0) {
        printf("Teller %d: No data received within 10 seconds. Exiting.\n", getpid());
        close(fifo_fd);
        exit(EXIT_SUCCESS);
    }

    // If data is available, read it from the FIFO
    if (FD_ISSET(fifo_fd, &read_fds)) {
        ssize_t len = read(fifo_fd, request, sizeof(request) - 1);
        if (len <= 0) {
            printf("Teller %d: No data received. Exiting.\n", getpid());
            close(fifo_fd);
            exit(EXIT_SUCCESS);
        }

        request[len] = '\0';
        printf("Teller %d received request: %s\n", getpid(), request);

        // Write request to shared memory
        strncpy(shared_data->request, request, BUFFER_SIZE);
        printf("Teller %d wrote request to shared memory: %s\n", getpid(), shared_data->request);
        
        // Signal server that request is ready
        sem_post(sem_request_array[teller_id]);
        printf("Teller %d sent request signal to server.\n", getpid());

        // Wait for server's response
        printf("Teller %d waiting for response signal from server...\n", getpid());
        sem_wait(sem_response_array[teller_id]);
        printf("Teller %d received response signal from server.\n", getpid());

        // Read response from shared memory
        char response[BUFFER_SIZE];
        strncpy(response, shared_data->response, BUFFER_SIZE);

        // Send response to client
        int fifo_write_fd = open(shared_data->client_fifo, O_WRONLY);
        if (fifo_write_fd != -1) {
            write(fifo_write_fd, response, strlen(response));
            close(fifo_write_fd);
            printf("Teller %d: Response sent to client: %s\n", getpid(), response);
        } else {
            perror("Failed to open client FIFO for writing");
        }

        // Clear shared memory request
        memset(shared_data->request, 0, BUFFER_SIZE);
        printf("Teller %d: Cleared shared memory request.\n", getpid());
    }

    close(fifo_fd);
    exit(EXIT_SUCCESS);
}

void cleanup(int sig) {
    printf("\nSignal received, shutting down server...\n");
    
    // Log the end of the log file
    FILE *log_file = fopen("AdaBank.bankLog", "a");
    if (log_file) {
        fprintf(log_file, "## end of log.\n");
        fclose(log_file);
    }
    
    unlink(SERVER_FIFO_NAME);
    for (int i = 0; i < active_tellers; i++) {
        cleanup_teller_resources(i);
    }

    printf("Cleaned up resources.\n");
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

    printf("Cleaned up resources for teller %d\n", teller_id);
}

int main() {
    char buffer[BUFFER_SIZE];
    pid_t client_pid;
    char client_fifo[BUFFER_SIZE];

    signal(SIGINT, cleanup);

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

    while (1) {
        ssize_t bytes = read(server_fd, buffer, sizeof(buffer) - 1);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            sscanf(buffer, "%d %s", &client_pid, client_fifo);

            printf("- Received client PID: %d FIFO: %s\n", client_pid, client_fifo);

            // Open client FIFO for reading
            int client_fifo_fd = open(client_fifo, O_RDONLY);
            if (client_fifo_fd == -1) {
                perror("open client fifo for read");
                continue;
            }

            // Process all transactions from this client
            while (1) {
                if (active_tellers >= MAX_TELLERS) {
                    printf("Maximum number of tellers reached. Cannot process more transactions.\n");
                    break;
                }

                // Initialize shared memory and semaphores for this teller
                shared_data_array[active_tellers] = init_teller_shared_memory(active_tellers);
                if (!shared_data_array[active_tellers]) {
                    printf("Failed to initialize shared memory for teller %d\n", active_tellers);
                    break;
                }

                if (!init_teller_semaphores(active_tellers)) {
                    printf("Failed to initialize semaphores for teller %d\n", active_tellers);
                    cleanup_teller_resources(active_tellers);
                    break;
                }

                // Set up teller data
                shared_data_array[active_tellers]->teller_id = active_tellers;
                shared_data_array[active_tellers]->client_pid = client_pid;
                strncpy(shared_data_array[active_tellers]->client_fifo, client_fifo, BUFFER_SIZE);
                shared_data_array[active_tellers]->is_active = true;

                // Create teller process
                int teller_id = active_tellers;
                pid_t teller_pid = Teller(teller_func, &teller_id);
                
                // Wait for request from teller
                printf("Server waiting for request signal from teller %d...\n", teller_id);
                sem_wait(sem_request_array[teller_id]);
                printf("Server received request signal from teller %d.\n", teller_id);

                // Process request
                printf("Server processing request: %s\n", shared_data_array[teller_id]->request);
                handle_transaction(shared_data_array[teller_id]->request, 
                                 shared_data_array[teller_id]->client_pid,
                                 shared_data_array[teller_id]->response);
                printf("Server processed request: %s\n", shared_data_array[teller_id]->response);

                // Signal teller that response is ready
                sem_post(sem_response_array[teller_id]);
                printf("Server sent response signal to teller %d.\n", teller_id);

                // Wait for teller to finish
                int status;
                waitTeller(teller_pid, &status);

                // Check if there are more transactions
                fd_set read_fds;
                struct timeval timeout;

                FD_ZERO(&read_fds);
                FD_SET(client_fifo_fd, &read_fds);

                timeout.tv_sec = 2;
                timeout.tv_usec = 0;

                int ready = select(client_fifo_fd + 1, &read_fds, NULL, NULL, &timeout);
                if (ready <= 0) {
                    // No more transactions or timeout
                    break;
                }

                active_tellers++;
            }

            close(client_fifo_fd);
        }
    }

    close(server_fd);
    unlink(SERVER_FIFO_NAME);
    return 0;
}