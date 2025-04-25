// bank_client.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>

#define SERVER_FIFO_NAME "/tmp/ServerFIFO_Name"
#define CLIENT_FIFO_TEMPLATE "/tmp/ClientFIFO_%d"
#define BUFFER_SIZE 256

int main() {
    char client_fifo[BUFFER_SIZE];
    char message[BUFFER_SIZE];
    char response[BUFFER_SIZE];

    snprintf(client_fifo, BUFFER_SIZE, CLIENT_FIFO_TEMPLATE, getpid());

    // Client FIFO oluştur
    if (mkfifo(client_fifo, 0666) == -1) {
        perror("mkfifo client");
        exit(EXIT_FAILURE);
    }

    // Server’a kendini tanıt: PID + FIFO adı
    int server_fd = open(SERVER_FIFO_NAME, O_WRONLY);
    if (server_fd == -1) {
        perror("open server fifo");
        exit(EXIT_FAILURE);
    }

    snprintf(message, BUFFER_SIZE, "%d %s", getpid(), client_fifo);
    write(server_fd, message, strlen(message));
    close(server_fd);

    printf("Client sent: %s\n", message);

    // Teller hazır olduktan sonra, client FIFO’suna işlem gönderiyoruz
    int fifo_fd = open(client_fifo, O_WRONLY);
    if (fifo_fd == -1) {
        perror("open client fifo for write");
        exit(EXIT_FAILURE);
    }

    // Örnek işlem: “N deposit 300”
    strcpy(message, "N deposit 300");
    write(fifo_fd, message, strlen(message));
    close(fifo_fd);
    printf("Client sent transaction: %s\n", message);

    // Teller'dan cevabı oku
    fifo_fd = open(client_fifo, O_RDONLY);
    if (fifo_fd == -1) {
        perror("open client fifo for read");
        exit(EXIT_FAILURE);
    }

    ssize_t bytes = read(fifo_fd, response, sizeof(response) - 1);
    if (bytes > 0) {
        response[bytes] = '\0';
        printf("Client received: %s\n", response);
    }

    close(fifo_fd);
    unlink(client_fifo); // FIFO'yu temizle

    return 0;
}
