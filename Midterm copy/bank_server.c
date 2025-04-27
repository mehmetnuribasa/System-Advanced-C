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

#define SERVER_FIFO_NAME "/tmp/ServerFIFO_Name"
#define BUFFER_SIZE 256
#define MAX_ACCOUNTS 100
#define SHARED_MEM_NAME "/bank_shared_mem"
#define SEMAPHORE_NAME_TEMPLATE "/bank_semaphore_%d"

typedef struct {
    char request[BUFFER_SIZE];
    char response[BUFFER_SIZE]; // Added response field
    pid_t client_pid;
    char client_fifo[BUFFER_SIZE];
    int teller_id; // Added teller_id field
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

SharedData *shared_data;
sem_t *semaphores[MAX_ACCOUNTS]; // Array of semaphores for each teller
int teller_count = 0;

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

void handle_transaction(const char* request, pid_t client_pid, char* response) {
    char type;
    char operation[16];
    int amount;
    char bank_id[20];
    int acc_index = -1;

    // Parse the request to extract BankID if present
    if (sscanf(request, "%s %s %d", bank_id, operation, &amount) == 3) {
        // Check if it's a BankID request
        if (strncmp(bank_id, "BankID_", 7) == 0) {
            acc_index = find_account_by_bank_id(bank_id);
            if (acc_index == -1) {
                snprintf(response, BUFFER_SIZE, "ERROR: Account with %s not found", bank_id);
                return;
            }
        } else {
            // It's a new account request
            type = bank_id[0]; // First character is the account type
            acc_index = find_account_by_pid(client_pid);
            if (acc_index == -1) {
                create_account(type, client_pid);
                acc_index = account_count - 1;
            }
        }

        // Process the operation (deposit/withdraw)
        if (strcmp(operation, "deposit") == 0) {
            accounts[acc_index].balance += amount;
            snprintf(response, BUFFER_SIZE, "OK: Deposited %d. New balance = %d. Account: %s", 
                    amount, accounts[acc_index].balance, accounts[acc_index].bank_id);
            char log_message[BUFFER_SIZE];
            snprintf(log_message, BUFFER_SIZE, "%s D %d %d", 
                    accounts[acc_index].bank_id, amount, accounts[acc_index].balance);
            log_transaction(log_message);
        } else if (strcmp(operation, "withdraw") == 0) {
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
    } else {
        snprintf(response, BUFFER_SIZE, "ERROR: Invalid request format");
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
    int teller_id = shared_data->teller_id;
    char semaphore_name[BUFFER_SIZE];
    snprintf(semaphore_name, BUFFER_SIZE, SEMAPHORE_NAME_TEMPLATE, teller_id);
    
    // Open the semaphore for this teller
    sem_t *teller_semaphore = sem_open(semaphore_name, O_CREAT, 0666, 0);
    if (teller_semaphore == SEM_FAILED) {
        perror("sem_open for teller");
        exit(EXIT_FAILURE);
    }
    
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

    timeout.tv_sec = 5; // 5 seconds timeout
    timeout.tv_usec = 0;

    int ready = select(fifo_fd + 1, &read_fds, NULL, NULL, &timeout);
    if (ready == -1) {
        perror("select");
        close(fifo_fd);
        exit(EXIT_FAILURE);
    } else if (ready == 0) {
        printf("Teller %d: No data received within 5 seconds. Exiting.\n", getpid());
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

        sleep(3);
        // Write request to shared memory
        strncpy(shared_data->request, request, BUFFER_SIZE);
        printf("Teller %d wrote request to shared memory: %s\n", getpid(), shared_data->request);
        sem_post(teller_semaphore); // Notify server of new request
        printf("Teller %d sent request signal to server.\n", getpid());

        sleep(1);
        // Wait for server to process the request
        printf("Teller %d waiting for response signal from server...\n", getpid());
        sem_wait(teller_semaphore); // Wait for server's response
        printf("Teller %d received response signal from server.\n", getpid());

        // Read response from shared memory
        char response[BUFFER_SIZE];
        strncpy(response, shared_data->response, BUFFER_SIZE);

        // Send response to client - Düzeltilmiş kısım
        // Client FIFO'yu blocking modda açıyoruz
        int fifo_write_fd = open(shared_data->client_fifo, O_WRONLY);
        if (fifo_write_fd != -1) {
            write(fifo_write_fd, response, strlen(response));
            close(fifo_write_fd);
            printf("Teller %d: Response sent to client: %s\n", getpid(), response);
        } else {
            perror("Failed to open client FIFO for writing");
        }

        // Clear shared_data->request to signal completion
        memset(shared_data->request, 0, BUFFER_SIZE);
        printf("Teller %d: Cleared shared_data->request.\n", getpid());
    }

    close(fifo_fd);
    sem_close(teller_semaphore);
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
    munmap(shared_data, sizeof(SharedData));
    shm_unlink(SHARED_MEM_NAME);
    
    // Close and unlink all semaphores
    for (int i = 0; i < teller_count; i++) {
        char semaphore_name[BUFFER_SIZE];
        snprintf(semaphore_name, BUFFER_SIZE, SEMAPHORE_NAME_TEMPLATE, i);
        sem_close(semaphores[i]);
        sem_unlink(semaphore_name);
    }

    printf("Cleaned up resources.\n");
    exit(0);
}

int main() {
    char buffer[BUFFER_SIZE];

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

    // Initialize shared memory
    int shm_fd = shm_open(SHARED_MEM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(SharedData));
    shared_data = mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    while (1) {
        ssize_t bytes = read(server_fd, buffer, sizeof(buffer) - 1);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            sscanf(buffer, "%d %s", &shared_data->client_pid, shared_data->client_fifo);

            printf("- Received client PID: %d FIFO: %s\n", shared_data->client_pid, shared_data->client_fifo);

            // Open client FIFO for reading
            int client_fifo_fd = open(shared_data->client_fifo, O_RDONLY);
            if (client_fifo_fd == -1) {
                perror("open client fifo for read");
                continue;
            }

            // Process all transactions from this client
            while (1) {
                // Create a new teller for each transaction
                shared_data->teller_id = teller_count;
                char semaphore_name[BUFFER_SIZE];
                snprintf(semaphore_name, BUFFER_SIZE, SEMAPHORE_NAME_TEMPLATE, teller_count);
                semaphores[teller_count] = sem_open(semaphore_name, O_CREAT, 0666, 0);
                
                pid_t teller_pid = Teller(teller_func, NULL);
                
                // Wait for semaphore signal
                printf("Server waiting for request signal from teller %d...\n", teller_count);
                sem_wait(semaphores[teller_count]);
                printf("Server received request signal from teller %d.\n", teller_count);

                // Process shared memory request
                printf("Server processing request: %s\n", shared_data->request);
                handle_transaction(shared_data->request, shared_data->client_pid, shared_data->response);
                printf("Server processed request: %s\n", shared_data->response);
                sem_post(semaphores[teller_count]); // Notify Teller of response
                printf("Server sent response signal to teller %d.\n", teller_count);
                
                // Wait for teller to finish
                int status;
                waitTeller(teller_pid, &status);
                
                // Increment teller count for next transaction
                teller_count++;

                // Check if there are more transactions
                fd_set read_fds;
                struct timeval timeout;

                FD_ZERO(&read_fds);
                FD_SET(client_fifo_fd, &read_fds);

                timeout.tv_sec = 1; // 1 second timeout
                timeout.tv_usec = 0;

                int ready = select(client_fifo_fd + 1, &read_fds, NULL, NULL, &timeout);
                if (ready <= 0) {
                    // No more transactions or timeout
                    break;
                }
            }

            close(client_fifo_fd);
        }
    }

    close(server_fd);
    unlink(SERVER_FIFO_NAME);
    return 0;
}