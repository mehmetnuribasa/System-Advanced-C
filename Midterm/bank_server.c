// bank_server.c (Teller içinde FIFO işlemi)
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>

#define SERVER_FIFO_NAME "/tmp/ServerFIFO_Name"
#define BUFFER_SIZE 256
#define MAX_ACCOUNTS 100

typedef struct {
    char type;           // 'N' or 'B'
    int account_id;
    int balance;
    pid_t client_pid;
} Account;

Account accounts[MAX_ACCOUNTS];
int account_count = 0;

int find_account_by_pid(pid_t pid) {
    for (int i = 0; i < account_count; i++) {
        if (accounts[i].client_pid == pid)
            return i;
    }
    return -1;
}

void create_account(char type, pid_t pid) {
    accounts[account_count].type = type;
    accounts[account_count].account_id = 1000 + account_count;
    accounts[account_count].balance = 0;
    accounts[account_count].client_pid = pid;
    account_count++;
}

void handle_transaction(const char* request, pid_t client_pid, char* response) {
    char type;
    char operation[16];
    int amount;

    int acc_index = find_account_by_pid(client_pid);
    if (acc_index == -1) {
        // İlk defa bağlanan kullanıcı için hesap oluştur
        sscanf(request, "%c %s %d", &type, operation, &amount);
        create_account(type, client_pid);
        acc_index = account_count - 1;
    } else {
        sscanf(request, "%*c %s %d", operation, &amount);
    }

    if (strcmp(operation, "deposit") == 0) {
        accounts[acc_index].balance += amount;
        snprintf(response, BUFFER_SIZE, "OK: Deposited %d. New balance = %d", amount, accounts[acc_index].balance);
    } else if (strcmp(operation, "withdraw") == 0) {
        if (accounts[acc_index].balance >= amount) {
            accounts[acc_index].balance -= amount;
            snprintf(response, BUFFER_SIZE, "OK: Withdrawn %d. New balance = %d", accounts[acc_index].balance);
        } else {
            snprintf(response, BUFFER_SIZE, "ERROR: Insufficient funds. Current balance = %d", accounts[acc_index].balance);
        }
    } else {
        snprintf(response, BUFFER_SIZE, "ERROR: Unknown operation '%s'", operation);
    }
}

void cleanup(int sig) {
    printf("\nSignal received, shutting down server...\n");
    unlink(SERVER_FIFO_NAME);
    printf("Removed %s\n", SERVER_FIFO_NAME);
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

    while (1) {
        ssize_t bytes = read(server_fd, buffer, sizeof(buffer) - 1);
        if (bytes > 0) {
            buffer[bytes] = '\0';

            int client_pid;
            char client_fifo[BUFFER_SIZE];
            sscanf(buffer, "%d %s", &client_pid, client_fifo);

            printf("- Received client PID: %d FIFO: %s\n", client_pid, client_fifo);

            pid_t teller_pid = fork();
            if (teller_pid == 0) {
                // Child process (Teller)
                printf("-- Teller PID %d is serving Client %d…\n", getpid(), client_pid);

                char request[BUFFER_SIZE];
                int fifo_fd = open(client_fifo, O_RDONLY);
                if (fifo_fd == -1) {
                    perror("open client fifo for read in teller");
                    exit(EXIT_FAILURE);
                }

                ssize_t len = read(fifo_fd, request, sizeof(request) - 1);
                close(fifo_fd);

                if (len <= 0) {
                    printf("Teller %d: No data received.\n", getpid());
                    exit(1);
                }

                request[len] = '\0';
                printf("Teller received request: %s\n", request);

                char response[BUFFER_SIZE];
                handle_transaction(request, client_pid, response);

                // Send response to client
                fifo_fd = open(client_fifo, O_WRONLY);
                if (fifo_fd != -1) {
                    write(fifo_fd, response, strlen(response));
                    close(fifo_fd);
                }

                printf("Teller %d: Response sent.\n", getpid());
                exit(0);
            }
        }
    }

    close(server_fd);
    unlink(SERVER_FIFO_NAME);
    return 0;
}
