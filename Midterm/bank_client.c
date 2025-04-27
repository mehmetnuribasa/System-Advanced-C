#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>

#define SERVER_FIFO_NAME "/tmp/ServerFIFO_Name"
#define CLIENT_FIFO_TEMPLATE "/tmp/ClientFIFO_%d"
#define BUFFER_SIZE 256

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <client_file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    FILE *client_file = fopen(argv[1], "r");
    if (!client_file) {
        perror("Failed to open client file");
        exit(EXIT_FAILURE);
    }

    char client_fifo[BUFFER_SIZE];
    char message[BUFFER_SIZE];
    char response[BUFFER_SIZE];

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
        printf("Client waiting for response...\n"); //Êlhcşıjeioşcki
        fifo_fd = open(client_fifo, O_RDONLY);
        if (fifo_fd == -1) {
            perror("open client fifo for read");
            break;
        }

        ssize_t bytes = read(fifo_fd, response, sizeof(response) - 1);
        if (bytes > 0) {
            response[bytes] = '\0';
            printf("Client received: %s\n", response);
        }

        close(fifo_fd);
    }

    fclose(client_file);
    unlink(client_fifo);

    return 0;
}