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

#define SERVER_FIFO_NAME "/tmp/ServerFIFO_Name"
#define BUFFER_SIZE 256
#define MAX_ACCOUNTS 100
#define SHARED_MEM_NAME "/bank_shared_mem"
#define SEMAPHORE_NAME "/bank_semaphore"

typedef struct {
    char request[BUFFER_SIZE];
    char response[BUFFER_SIZE]; // Added response field
    pid_t client_pid;
    char client_fifo[BUFFER_SIZE];
} SharedData;

typedef struct {
    char type;           // for normal or business account ('N' or 'B')
    int account_id;
    int balance;
    pid_t client_pid;
} Account;

Account accounts[MAX_ACCOUNTS];
int account_count = 0;

SharedData *shared_data;
sem_t *semaphore;

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
    accounts[account_count].balance = 0;
    accounts[account_count].client_pid = pid;
    account_count++;
}

void log_transaction(const char *log_message) {
    FILE *log_file = fopen("AdaBank.bankLog", "a");
    if (log_file) {
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
    int account_id;

    // Parse the request to extract BankID if present
    if (sscanf(request, "BankID_%d %s %d", &account_id, operation, &amount) == 3) {
        int acc_index = -1;
        for (int i = 0; i < account_count; i++) {
            if (accounts[i].account_id == account_id) {
                acc_index = i;
                break;
            }
        }

        if (acc_index == -1) {
            snprintf(response, BUFFER_SIZE, "ERROR: Account with BankID_%d not found", account_id);
            return;
        }

        // Process the operation (deposit/withdraw)
        if (strcmp(operation, "deposit") == 0) {
            accounts[acc_index].balance += amount;
            snprintf(response, BUFFER_SIZE, "OK: Deposited %d. New balance = %d", amount, accounts[acc_index].balance);
            char log_message[BUFFER_SIZE];
            snprintf(log_message, BUFFER_SIZE, "BankID_%d D %d %d", accounts[acc_index].account_id, amount, accounts[acc_index].balance);
            log_transaction(log_message);
        } else if (strcmp(operation, "withdraw") == 0) {
            if (accounts[acc_index].balance >= amount) {
                accounts[acc_index].balance -= amount;
                snprintf(response, BUFFER_SIZE, "OK: Withdrawn %d. New balance = %d", amount, accounts[acc_index].balance);
                char log_message[BUFFER_SIZE];
                snprintf(log_message, BUFFER_SIZE, "BankID_%d W %d %d", accounts[acc_index].account_id, amount, accounts[acc_index].balance);
                log_transaction(log_message);

                // Remove account if balance is zero
                if (accounts[acc_index].balance == 0) {
                    printf("Account %d closed.\n", accounts[acc_index].account_id);
                    for (int i = acc_index; i < account_count - 1; i++) {
                        accounts[i] = accounts[i + 1];
                    }
                    account_count--;
                }
            } else {
                snprintf(response, BUFFER_SIZE, "ERROR: Insufficient funds. Current balance = %d", accounts[acc_index].balance);
            }
        } else {
            snprintf(response, BUFFER_SIZE, "ERROR: Unknown operation '%s'", operation);
        }
    }
    
    // Handle new account creation
    else if (sscanf(request, "%c %s %d", &type, operation, &amount) == 3) {
        int acc_index = find_account_by_pid(client_pid);
        if (acc_index == -1) {
            create_account(type, client_pid);
            acc_index = account_count - 1;
        }

        if (strcmp(operation, "deposit") == 0) {
            accounts[acc_index].balance += amount;
            snprintf(response, BUFFER_SIZE, "OK: Deposited %d. New balance = %d", amount, accounts[acc_index].balance);
            char log_message[BUFFER_SIZE];
            snprintf(log_message, BUFFER_SIZE, "BankID_%d D %d %d", accounts[acc_index].account_id, amount, accounts[acc_index].balance);
            log_transaction(log_message);
        } else if (strcmp(operation, "withdraw") == 0) {
            if (accounts[acc_index].balance >= amount) {
                accounts[acc_index].balance -= amount;
                snprintf(response, BUFFER_SIZE, "OK: Withdrawn %d. New balance = %d", amount, accounts[acc_index].balance);
                char log_message[BUFFER_SIZE];
                snprintf(log_message, BUFFER_SIZE, "BankID_%d W %d %d", accounts[acc_index].account_id, amount, accounts[acc_index].balance);
                log_transaction(log_message);

                // Remove account if balance is zero
                if (accounts[acc_index].balance == 0) {
                    printf("Account %d closed.\n", accounts[acc_index].account_id);
                    for (int i = acc_index; i < account_count - 1; i++) {
                        accounts[i] = accounts[i + 1];
                    }
                    account_count--;
                }
            } else {
                snprintf(response, BUFFER_SIZE, "ERROR: Insufficient funds. Current balance = %d", accounts[acc_index].balance);
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
    int fifo_fd = open(shared_data->client_fifo, O_RDONLY | O_NONBLOCK);
    if (fifo_fd == -1) {
        perror("open client fifo for read in teller");
        exit(EXIT_FAILURE);
    }

    while (1) {
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
            break;
        }

        // If data is available, read it from the FIFO
        if (FD_ISSET(fifo_fd, &read_fds)) {
            ssize_t len = read(fifo_fd, request, sizeof(request) - 1);
            if (len <= 0) {
                printf("Teller %d: No data received. Exiting.\n", getpid());
                break;
            }

            request[len] = '\0';
            printf("Teller received request: %s\n", request);

            sleep(3);
            // Write request to shared memory
            strncpy(shared_data->request, request, BUFFER_SIZE);
            printf("Teller wrote request to shared memory: %s\n", shared_data->request);
            sem_post(semaphore); // Notify server of new request
            printf("Teller sent request signal to server.\n");

            sleep(1);
            // Wait for server to process the request
            printf("Teller waiting for response signal from server...\n");
            sem_wait(semaphore); // Wait for server's response
            printf("Teller received response signal from server.\n");

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
    }

    close(fifo_fd);
}

void cleanup(int sig) {
    printf("\nSignal received, shutting down server...\n");
    unlink(SERVER_FIFO_NAME);
    munmap(shared_data, sizeof(SharedData));
    shm_unlink(SHARED_MEM_NAME);
    sem_close(semaphore);
    sem_unlink(SEMAPHORE_NAME);

    // Log server shutdown
    log_transaction("## end of log.");
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

    // Initialize shared memory and semaphore
    int shm_fd = shm_open(SHARED_MEM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(SharedData));
    shared_data = mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    semaphore = sem_open(SEMAPHORE_NAME, O_CREAT, 0666, 0);

    sem_init(semaphore, 1, 0); // Ensure semaphore starts at 0
    while (1) {
        ssize_t bytes = read(server_fd, buffer, sizeof(buffer) - 1);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            sscanf(buffer, "%d %s", &shared_data->client_pid, shared_data->client_fifo);

            printf("- Received client PID: %d FIFO: %s\n", shared_data->client_pid, shared_data->client_fifo);

            // Create Teller process
            Teller(teller_func, NULL);

            
            // while(1) {
                // Wait for semaphore signal
                printf("Server waiting for request signal from teller...\n");
                sem_wait(semaphore);
                printf("Server received request signal from teller.\n");

                // Check if the teller has finished processing
                // if (strlen(shared_data->request) == 0) {
                //     printf("No more requests from teller. Exiting loop.\n");
                //     break;
                // }

                // Process shared memory request
                printf("Server processing request: %s\n", shared_data->request);
                handle_transaction(shared_data->request, shared_data->client_pid, shared_data->response);
                printf("Server processed request: %s\n", shared_data->response);
                sem_post(semaphore); // Notify Teller of response
                printf("Server sent response signal to teller.\n");
            
            // }
        }
    }

    close(server_fd);
    unlink(SERVER_FIFO_NAME);
    return 0;
}