// client/chatclient.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/stat.h>

// ANSI Color Codes
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define BUFFER_SIZE 1024
#define MAX_FILENAME 128
#define MAX_FILESIZE (3*1024*1024)

int sockfd;
pthread_t recv_thread;

void *receive_handler(void *arg) {
    char buffer[BUFFER_SIZE];
    int n;
    while ((n = read(sockfd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[n] = '\0';
        // Remove potential extra newline from server messages
        buffer[strcspn(buffer, "\n")] = 0;

        // Determine color based on message content
        char *color = ANSI_COLOR_RESET;
        if (strncmp(buffer, "[Server]:", 9) == 0) {
            color = ANSI_COLOR_GREEN; // Server info/success
        } else if (strncmp(buffer, "[ERROR]", 7) == 0) {
            color = ANSI_COLOR_RED; // Errors
        } else if (strncmp(buffer, "[room ']", 6) == 0) {
            color = ANSI_COLOR_CYAN; // Room messages
        } else if (strncmp(buffer, "[whisper from ']", 14) == 0) {
            color = ANSI_COLOR_MAGENTA; // Whisper messages
        } else if (strncmp(buffer, "[DISCONNECTED]", 14) == 0) {
             color = ANSI_COLOR_YELLOW; // Disconnect message
        }

        printf("\r%s%s%s\n> ", color, buffer, ANSI_COLOR_RESET); // Apply color
        fflush(stdout);
    }
    printf("[DISCONNECTED] Disconnected. Goodbye!\n"); // This already has the tag
    exit(0);
}

int valid_extension(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return 0;
    return strcmp(ext, ".txt") == 0 || strcmp(ext, ".pdf") == 0 || strcmp(ext, ".jpg") == 0 || strcmp(ext, ".png") == 0;
}

int main(int argc, char *argv[]) {
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char username[32];

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Server information
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[2]));

    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    // Connect to server
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    printf("Connected to server %s:%s\n", argv[1], argv[2]);

    // Get username
    while (1) {
        printf("Enter your username (max 16 chars, alphanumeric): ");
        fflush(stdout);
        if (fgets(username, sizeof(username), stdin) == NULL) exit(1);
        username[strcspn(username, "\n")] = 0;
        int valid = 1;
        if (strlen(username) == 0 || strlen(username) > 16) valid = 0;
        for (int i = 0; username[i]; i++) {
            if (!(('a' <= username[i] && username[i] <= 'z') ||
                  ('A' <= username[i] && username[i] <= 'Z') ||
                  ('0' <= username[i] && username[i] <= '9')))
                valid = 0;
        }
        if (!valid) {
            printf("Invalid username. Try again.\n");
            continue;
        }
        break;
    }
    // Send username to server
    if (write(sockfd, username, strlen(username)) < 0) {
        perror("write");
        exit(1);
    }

    // Start receiver thread
    if (pthread_create(&recv_thread, NULL, receive_handler, NULL) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    // Get message from user and send to server
    while (1) {
        printf("> ");
        fflush(stdout);
        if (fgets(buffer, sizeof(buffer), stdin) == NULL)
            break;

        if (strncmp(buffer, "/exit", 5) == 0) {
            break;
        }

        // Remove newline
        buffer[strcspn(buffer, "\n")] = 0;

        // /sendfile command check
        if (strncmp(buffer, "/sendfile ", 10) == 0) {
            char *args = buffer + 10;
            char *filename = strtok(args, " ");
            char *target = strtok(NULL, "");
            if (!filename || !target) {
                printf("Usage: /sendfile <filename> <username>\n");
                continue;
            }
            if (!valid_extension(filename)) {
                printf("[ERROR] Invalid file extension. Allowed: .txt, .pdf, .jpg, .png\n");
                continue;
            }
            struct stat st;
            if (stat(filename, &st) != 0) {
                printf("[ERROR] File not found.\n");
                continue;
            }
            if (st.st_size > MAX_FILESIZE) {
                printf("[ERROR] File size exceeds 3MB.\n");
                continue;
            }
            
            // Send command to server
            char cmd[256];
            snprintf(cmd, sizeof(cmd), "/sendfile %s %s", filename, target);
            if (write(sockfd, cmd, strlen(cmd)) < 0) {
                perror("write");
                continue;
            }
            
            // Simulated file transfer wait
            sleep(1);
            continue;
        }

        if (write(sockfd, buffer, strlen(buffer)) < 0) {
            perror("write");
            break;
        }
    }

    printf("[DISCONNECTED] Disconnected. Goodbye!\n"); // Add goodbye message here
    close(sockfd);
    return 0;
}
