#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <stdbool.h>

#define SERVER_FIFO_NAME "/tmp/ServerFIFO_Name"
#define CLIENT_FIFO_TEMPLATE "/tmp/ClientFIFO_%d"
#define BUFFER_SIZE 256

// Global variables for cleanup
char client_fifo[BUFFER_SIZE];
FILE *client_file = NULL;

// Signal handler function
void cleanup_handler(int sig) {
    printf("\nSignal received, cleaning up...\n");
    
    // Close client file if open
    if (client_file) {
        fclose(client_file);
    }
    
    // Remove client FIFO
    unlink(client_fifo);
    
    printf("Cleanup completed. Exiting.\n");
    exit(EXIT_SUCCESS);
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

void process_transaction(const char* bank_id, const char* type, double amount) {
    // BankID kontrolü
    if (!is_valid_bank_id(bank_id)) {
        printf("ERROR: Invalid BankID format. Must be 'BankID_XX' where XX is a number\n");
        return;
    }
    
    // İşlem tipi kontrolü
    if (strcmp(type, "deposit") != 0 && strcmp(type, "withdraw") != 0) {
        printf("ERROR: Invalid transaction type. Must be 'deposit' or 'withdraw'\n");
        return;
    }
    
    // Miktar kontrolü
    if (amount <= 0) {
        printf("ERROR: Amount must be greater than 0\n");
        return;
    }
    
    // ... existing code ...
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <client_file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Set up signal handlers
    signal(SIGINT, cleanup_handler);
    signal(SIGTERM, cleanup_handler);
    signal(SIGQUIT, cleanup_handler);

    client_file = fopen(argv[1], "r");
    if (!client_file) {
        perror("Failed to open client file");
        exit(EXIT_FAILURE);
    }

    char message[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    char bank_id[20] = "";  // Store the BankID received from server

    // Create FIFO for client
    snprintf(client_fifo, BUFFER_SIZE, CLIENT_FIFO_TEMPLATE, getpid());
    if (mkfifo(client_fifo, 0666) == -1) {
        perror("mkfifo client");
        fclose(client_file);
        exit(EXIT_FAILURE);
    }

    // Notify server about the client FIFO
    int server_fd = open(SERVER_FIFO_NAME, O_WRONLY);
    if (server_fd == -1) {
        perror("open server fifo");
        fclose(client_file);
        unlink(client_fifo);
        exit(EXIT_FAILURE);
    }

    snprintf(message, BUFFER_SIZE, "%d %s", getpid(), client_fifo);
    write(server_fd, message, strlen(message));
    close(server_fd);

    printf("Client sent: %s\n", message);

    // Process each request in the client file
    while (fgets(message, BUFFER_SIZE, client_file)) {
        message[strcspn(message, "\n")] = '\0'; // Remove newline character

        // If we have a BankID, use it for the transaction
        if (strlen(bank_id) > 0) {
            char temp[BUFFER_SIZE];
            char operation[16];
            int amount;
            
            // Parse the original message
            if (sscanf(message, "%c %s %d", &temp[0], operation, &amount) == 3) {
                // Format the message with BankID
                snprintf(message, BUFFER_SIZE, "%s %s %d", bank_id, operation, amount);
            }
        }

        // Open client FIFO for writing request
        int fifo_fd = open(client_fifo, O_WRONLY);
        if (fifo_fd == -1) {
            perror("open client fifo for write");
            break;
        }

        write(fifo_fd, message, strlen(message));
        close(fifo_fd);
        printf("Client sent transaction: %s\n", message);

        // Read response from server
        printf("Client waiting for response...\n");
        fifo_fd = open(client_fifo, O_RDONLY);
        if (fifo_fd == -1) {
            perror("open client fifo for read");
            break;
        }

        ssize_t bytes = read(fifo_fd, response, sizeof(response) - 1);
        if (bytes > 0) {
            response[bytes] = '\0';
            printf("Client received: %s\n", response);

            // Extract BankID from response if it's a new account
            if (strstr(response, "Account: BankID_") != NULL) {
                char *bank_id_start = strstr(response, "Account: ");
                if (bank_id_start) {
                    bank_id_start += 9; // Skip "Account: "
                    char *bank_id_end = strchr(bank_id_start, ' ');
                    if (bank_id_end) {
                        int len = bank_id_end - bank_id_start;
                        strncpy(bank_id, bank_id_start, len);
                        bank_id[len] = '\0';
                        printf("Client received BankID: %s\n", bank_id);
                    }
                }
            }
        }

        close(fifo_fd);
        
        // Add a small delay between transactions to allow tellers to process
        usleep(100000); // 100ms delay
    }

    fclose(client_file);
    unlink(client_fifo);

    return 0;
}