// server/chatserver.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <time.h>
#include <signal.h>
#include <stdarg.h>

#define MAX_CLIENTS 100
#define MAX_USERNAME 17
#define MAX_ROOMS 50
#define MAX_ROOMNAME 33
#define MAX_UPLOAD_QUEUE 5
#define MAX_FILENAME 128
#define MAX_ROOM_CAPACITY 15  // Each room's maximum capacity

void *handle_client(void *arg);
void *file_transfer_worker(void *arg);

char usernames[MAX_CLIENTS][MAX_USERNAME];
char user_rooms[MAX_CLIENTS][MAX_ROOMNAME]; // Each user's room name
char room_names[MAX_ROOMS][MAX_ROOMNAME];  // Room names
int client_count = 0;
int room_count = 0;
pthread_mutex_t user_mutex = PTHREAD_MUTEX_INITIALIZER;
int client_fds[MAX_CLIENTS];

typedef struct {
    char sender[MAX_USERNAME];
    char receiver[MAX_USERNAME];
    char filename[MAX_FILENAME];
    time_t timestamp; // Add timestamp for queue wait time (for file transfer)
} FileJob;

FileJob upload_queue[MAX_UPLOAD_QUEUE];
int queue_size = 0;
sem_t upload_sem;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    int client_fd;
    struct sockaddr_in client_addr;
} client_info_t;

FILE *log_fp = NULL;
void log_event(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    // Format message (common part for both terminal and file)
    char message[512];
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);
    // Write to terminal (simple format)
    printf("%s\n", message);
    // Write to log file (timestamped and tagged format)
    if (log_fp) {
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char timebuf[32];
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_info);
        char logline[1024];
        // Separate tag (first [] part)
        const char *tag_start = strchr(message, '[');
        const char *tag_end = strchr(message, ']');
        if (tag_start && tag_end && tag_start < tag_end) {
            int tag_len = tag_end - tag_start + 1;
            char tag[32];
            strncpy(tag, tag_start, tag_len);
            tag[tag_len] = '\0';
            const char *msg_after_tag = tag_end + 2; // ] and after space
            snprintf(logline, sizeof(logline), "%s - %s %s\n", timebuf, tag, msg_after_tag);
        } else {
             // If no tag, log simple message
            snprintf(logline, sizeof(logline), "%s - %s\n", timebuf, message);
        }
        fputs(logline, log_fp); fflush(log_fp);
    }
}

// Helper function: is room name valid?
int valid_roomname(const char *room) {
    if (strlen(room) == 0 || strlen(room) > 32) return 0;
    for (int i = 0; room[i]; i++) {
        if (!(('a' <= room[i] && room[i] <= 'z') ||
              ('A' <= room[i] && room[i] <= 'Z') ||
              ('0' <= room[i] && room[i] <= '9')))
            return 0;
    }
    return 1;
}

void shutdown_server(int signo) {
    // Tüm kullanıcılara kapatma bildirimi gönder
    pthread_mutex_lock(&user_mutex);
    log_event("[SHUTDOWN] SIGINT received. Disconnecting %d clients, saving logs.", client_count);
    
    // Her kullanıcıya kapatma mesajı gönder
    for (int i = 0; i < client_count; i++) {
        const char *msg = "[Server]: Server is shutting down. Please save your work and reconnect later.\n";
        write(client_fds[i], msg, strlen(msg));
        close(client_fds[i]);
    }
    
    // Kuyruktaki dosya transferlerini temizle
    pthread_mutex_lock(&queue_mutex);
    if (queue_size > 0) {
        log_event("[SHUTDOWN] Cancelling %d pending file transfers in queue", queue_size);
        for (int i = 0; i < queue_size; i++) {
            // Sadece kuyruktaki transferleri iptal et
            if (upload_queue[i].timestamp > 0) {  // timestamp 0 ise transfer zaten tamamlanmış demektir
                log_event("[SHUTDOWN] Cancelled queued transfer: '%s' from %s to %s", 
                         upload_queue[i].filename, 
                         upload_queue[i].sender, 
                         upload_queue[i].receiver);
            }
        }
    }
    queue_size = 0;
    pthread_mutex_unlock(&queue_mutex);
    
    // Tüm mutex ve semaphore'ları temizle
    pthread_mutex_unlock(&user_mutex);
    pthread_mutex_destroy(&user_mutex);
    pthread_mutex_destroy(&queue_mutex);
    sem_destroy(&upload_sem);
    
    // Log dosyasını kapat
    if (log_fp) {
        log_event("[SHUTDOWN] Server shutdown complete. All resources cleaned up.");
        fclose(log_fp);
    }
    
    exit(0);
}

int main(int argc, char *argv[]) {
    signal(SIGINT, shutdown_server);
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    int port = atoi(argv[1]);

    // Create TCP socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Server address configuration
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Bind socket
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // Start listening
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    log_event("[INFO] Server listening on port %d...", port);

    sem_init(&upload_sem, 0, MAX_UPLOAD_QUEUE);

    log_fp = fopen("server_log.txt", "a");
    if (!log_fp) { perror("log file"); exit(1); }

    // Infinite loop: accept incoming connections and start a thread
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        client_info_t *cinfo = malloc(sizeof(client_info_t));
        cinfo->client_fd = client_fd;
        cinfo->client_addr = client_addr;

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, (void*)cinfo) != 0) {
            perror("pthread_create");
            log_event("[ERROR] Could not create thread for client %s:%d",
                      inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            close(client_fd);
            free(cinfo);
        }
        pthread_detach(tid);  // Clean up resources automatically
    }

    close(server_fd);
    fclose(log_fp);
    return 0;
}

void *handle_client(void *arg) {
    client_info_t *cinfo = (client_info_t*)arg;
    int client_fd = cinfo->client_fd;
    struct sockaddr_in client_addr = cinfo->client_addr;
    free(cinfo);
    char username[MAX_USERNAME];
    char buffer[1024];
    int n;
    // Get username
    n = read(client_fd, username, MAX_USERNAME-1);
    if (n <= 0) { close(client_fd); return NULL; }
    username[n] = '\0';
    // Validation
    int valid = 1;
    if (strlen(username) == 0 || strlen(username) > 16) valid = 0;
    for (int i = 0; username[i]; i++) {
        if (!(('a' <= username[i] && username[i] <= 'z') ||
              ('A' <= username[i] && username[i] <= 'Z') ||
              ('0' <= username[i] && username[i] <= '9')))
            valid = 0;
    }
    pthread_mutex_lock(&user_mutex);
    for (int i = 0; i < client_count; i++) {
        if (strcmp(usernames[i], username) == 0) valid = 0;
    }
    if (!valid) {
        pthread_mutex_unlock(&user_mutex);
        const char *msg = "[ERROR] Invalid or duplicate username.\n";
        write(client_fd, msg, strlen(msg));
        log_event("[REJECTED] Duplicate username attempted: %s", username);
        close(client_fd);
        return NULL;
    }
    strcpy(usernames[client_count], username);
    client_fds[client_count] = client_fd;
    user_rooms[client_count][0] = '\0'; // Initially no room
    client_count++;
    pthread_mutex_unlock(&user_mutex);
    log_event("[CONNECT] user '%s' connected from %s", username, inet_ntoa(client_addr.sin_addr));

    while ((n = read(client_fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[n] = '\0';
        // Command processing
        if (strncmp(buffer, "/join ", 6) == 0) {
            char *room = buffer + 6;
            // Remove potential newline/spaces from room name (for room name)
            room[strcspn(room, " \n")] = 0;
            if (!valid_roomname(room)) {
                const char *msg = "[ERROR] Invalid room name.\n";
                write(client_fd, msg, strlen(msg));
                log_event("[ERROR] Join failed for %s: Invalid room name '%s'", username, room);
                continue;
            }
            pthread_mutex_lock(&user_mutex);
            // Check room capacity
            int room_user_count = 0;
            for(int i = 0; i < client_count; i++) {
                if(strcmp(user_rooms[i], room) == 0) {
                    room_user_count++;
                }
            }
            if(room_user_count >= MAX_ROOM_CAPACITY) {
                pthread_mutex_unlock(&user_mutex);
                const char *err = "[ERROR] Room is full. Max 15 users allowed.\n";
                write(client_fd, err, strlen(err));
                log_event("[ERROR] Join failed for %s: Room '%s' is full", username, room);
                continue;
            }
            // Is room exists?
            int found = 0;
            for (int i = 0; i < room_count; i++) {
                if (strcmp(room_names[i], room) == 0) found = 1;
            }
            if (!found && room_count < MAX_ROOMS) {
                strcpy(room_names[room_count++], room);
            }
            // Assign user to room
            int user_idx = -1;
            for (int i = 0; i < client_count; i++) {
                if (strcmp(usernames[i], username) == 0) user_idx = i;
            }
            if (user_idx != -1) {
                strcpy(user_rooms[user_idx], room);
            }
            pthread_mutex_unlock(&user_mutex);
            char msg[128];
            snprintf(msg, sizeof(msg), "[Server]: You joined the room '%s'\n", room);
            write(client_fd, msg, strlen(msg));
            log_event("[COMMAND] %s joined room '%s'", username, room);
            continue;
        }
        if (strncmp(buffer, "/broadcast ", 11) == 0) {
            char *msg = buffer + 11;
            pthread_mutex_lock(&user_mutex);
            int user_idx = -1;
            for (int i = 0; i < client_count; i++) {
                if (strcmp(usernames[i], username) == 0) user_idx = i;
            }
            if (user_idx == -1 || strlen(user_rooms[user_idx]) == 0) {
                pthread_mutex_unlock(&user_mutex);
                const char *err = "[ERROR] You are not in a room. Use /join <room> first.\n";
                write(client_fd, err, strlen(err));
                log_event("[ERROR] Broadcast from %s failed: not in room", username);
                continue;
            }
            char *room = user_rooms[user_idx];
            // Send confirmation message to sender
            char confirm_msg[128];
            snprintf(confirm_msg, sizeof(confirm_msg), "[Server]: Message sent to room '%s'\n", room);
            write(client_fd, confirm_msg, strlen(confirm_msg));
            // Send message to room members (except sender)
            char outmsg[1024];
            snprintf(outmsg, sizeof(outmsg), "[room '%s'] %s: %s\n", room, username, msg);
            for (int i = 0; i < client_count; i++) {
                if (i != user_idx && strcmp(user_rooms[i], room) == 0) {
                    write(client_fds[i], outmsg, strlen(outmsg));
                }
            }
            pthread_mutex_unlock(&user_mutex);
            log_event("[COMMAND] %s broadcasted to '%s'", username, room);
            continue;
        }
        if (strncmp(buffer, "/whisper ", 9) == 0) {
            char *args = buffer + 9;
            char *target = strtok(args, " ");
            char *msg = strtok(NULL, "");
            if (!target || !msg) {
                const char *err = "[ERROR] Usage: /whisper <username> <message>\n";
                write(client_fd, err, strlen(err));
                log_event("[ERROR] Whisper from %s failed: invalid usage", username);
                continue;
            }
            // Remove \n and spaces from parameters
            target[strcspn(target, " \n")] = 0;
            // Remove \n from msg (strtok NULL is used, so only \n is enough)
            if (msg) msg[strcspn(msg, "\n")] = 0;
            pthread_mutex_lock(&user_mutex);
            int found = 0;
            for (int i = 0; i < client_count; i++) {
                if (strcmp(usernames[i], target) == 0) {
                    char outmsg[1024];
                    snprintf(outmsg, sizeof(outmsg), "[whisper from %s]: %s\n", username, msg);
                    write(client_fds[i], outmsg, strlen(outmsg));
                    found = 1;
                    break;
                }
            }
            pthread_mutex_unlock(&user_mutex);
            if (found) {
                char ok_msg[128];
                snprintf(ok_msg, sizeof(ok_msg), "[Server]: Whisper sent to %s\n", target);
                write(client_fd, ok_msg, strlen(ok_msg));
                log_event("[COMMAND] %s sent whisper to %s", username, target);
            } else {
                const char *err = "[ERROR] User not found.\n";
                write(client_fd, err, strlen(err));
                log_event("[ERROR] Whisper target not found: %s to %s", username, target);
            }
            continue;
        }
        if (strcmp(buffer, "/leave") == 0) {
            pthread_mutex_lock(&user_mutex);
            int user_idx = -1;
            for (int i = 0; i < client_count; i++) {
                if (strcmp(usernames[i], username) == 0) user_idx = i;
            }
            if (user_idx == -1 || strlen(user_rooms[user_idx]) == 0) {
                pthread_mutex_unlock(&user_mutex);
                const char *err = "[ERROR] You are not in a room.\n";
                write(client_fd, err, strlen(err));
                continue;
            }
            char old_room[MAX_ROOMNAME];
            strcpy(old_room, user_rooms[user_idx]);
            user_rooms[user_idx][0] = '\0';
            pthread_mutex_unlock(&user_mutex);
            const char *ok = "[Server]: You left the room.\n";
            write(client_fd, ok, strlen(ok));
            log_event("[COMMAND] %s left room '%s'", username, old_room);
            continue;
        }
        if (strncmp(buffer, "/sendfile ", 10) == 0) {
            char *args = buffer + 10;
            char *filename = strtok(args, " ");
            char *target = strtok(NULL, "");
            if (!filename || !target) {
                const char *err = "[ERROR] Usage: /sendfile <filename> <username>\n";
                write(client_fd, err, strlen(err));
                log_event("[ERROR] Sendfile from %s failed: invalid usage", username);
                continue;
            }
            // Remove \n and spaces from parameters
            filename[strcspn(filename, " \n")] = 0;
            target[strcspn(target, " \n")] = 0;
            // Add to queue
            if (sem_trywait(&upload_sem) == 0) {
                pthread_mutex_lock(&queue_mutex);
                if (queue_size < MAX_UPLOAD_QUEUE) {
                    // Check for filename collision in queue
                    int collision = 0;
                    for(int i = 0; i < queue_size; i++) {
                        if(strcmp(upload_queue[i].sender, username) == 0 &&
                           strcmp(upload_queue[i].receiver, target) == 0 &&
                           strcmp(upload_queue[i].filename, filename) == 0) {
                            collision = 1;
                            break;
                        }
                    }
                    if(collision) {
                        pthread_mutex_unlock(&queue_mutex);
                        sem_post(&upload_sem); // Release semaphore acquired by trywait
                        const char *err = "[ERROR] A file with that name is already in the queue for this recipient.\n";
                        write(client_fd, err, strlen(err));
                        log_event("[FILE] Conflict: '%s' from %s to %s already in queue.", filename, username, target);
                        continue; // Continue the while loop in handle_client
                    }

                    // Check if target user exists and is connected
                    int target_fd = -1;
                    for(int i = 0; i < client_count; i++) {
                        if(strcmp(usernames[i], target) == 0) {
                            target_fd = client_fds[i];
                            break;
                        }
                    }
                    if(target_fd == -1) {
                         pthread_mutex_unlock(&queue_mutex);
                         sem_post(&upload_sem);
                         const char *err = "[ERROR] Target user not found or offline.\n";
                         write(client_fd, err, strlen(err));
                         log_event("[ERROR] Sendfile from %s failed: target user %s not found or offline", username, target);
                         continue; // Continue the while loop
                    }
                    // Add job to queue
                    strncpy(upload_queue[queue_size].sender, username, MAX_USERNAME);
                    strncpy(upload_queue[queue_size].receiver, target, MAX_USERNAME);
                    strncpy(upload_queue[queue_size].filename, filename, MAX_FILENAME);
                    upload_queue[queue_size].timestamp = time(NULL); // Record timestamp
                    // Process queue
                    FileJob *job = malloc(sizeof(FileJob));
                    *job = upload_queue[queue_size];
                    pthread_t tid;
                    pthread_create(&tid, NULL, file_transfer_worker, job);
                    pthread_detach(tid);
                    queue_size++;
                    pthread_mutex_unlock(&queue_mutex);
                    const char *ok = "[Server]: File added to the upload queue.\n";
                    write(client_fd, ok, strlen(ok));
                    log_event("[FILE-QUEUE] Upload '%s' from %s added to queue. Queue size: %d", filename, username, queue_size);
                    log_event("[COMMAND] %s initiated file transfer to %s", username, target);
                } else {
                    pthread_mutex_unlock(&queue_mutex);
                    sem_post(&upload_sem);
                    log_event("[ERROR] Upload queue is full for %s", username);
                    const char *err = "[ERROR] Upload queue is full. Try again later.\n";
                    write(client_fd, err, strlen(err));
                }
            } else {
                // Estimated wait time when queue is full
                double estimated_wait = queue_size * 3.0; // For each file, average 3 seconds
                
                char waitmsg[256];
                snprintf(waitmsg, sizeof(waitmsg), 
                        "[Server]: Upload queue is full. Estimated wait time: %.0f seconds...\n", 
                        estimated_wait);
                write(client_fd, waitmsg, strlen(waitmsg));
                
                log_event("[FILE-QUEUE] Upload queue full, %s waiting for %s (estimated wait: %.0f seconds)", 
                         username, filename, estimated_wait);
            }
            continue;
        }
    }

    log_event("[DISCONNECT] Client %s disconnected.", username);
    // Clear user and fd when connection is closed
    pthread_mutex_lock(&user_mutex);
    int idx = -1;
    for (int i = 0; i < client_count; i++) {
        if (strcmp(usernames[i], username) == 0) idx = i;
    }
    if (idx != -1) {
        for (int j = idx; j < client_count - 1; j++) {
            strcpy(usernames[j], usernames[j+1]);
            strcpy(user_rooms[j], user_rooms[j+1]);
            client_fds[j] = client_fds[j+1];
        }
        client_count--;
    }
    pthread_mutex_unlock(&user_mutex);
    close(client_fd);
    return NULL;
}

void *file_transfer_worker(void *arg) {
    FileJob job = *(FileJob*)arg;
    free(arg);
    
    // Simulated wait time (1-3 seconds)
    sleep(1 + (rand() % 3));
    
    // Calculate wait time
    time_t now = time(NULL);
    double wait_duration = difftime(now, job.timestamp);
    
    // Find sender and receiver fds
    int sender_fd = -1, receiver_fd = -1;
    pthread_mutex_lock(&user_mutex);
    for (int i = 0; i < client_count; i++) {
        if (strcmp(usernames[i], job.sender) == 0) sender_fd = client_fds[i];
        if (strcmp(usernames[i], job.receiver) == 0) receiver_fd = client_fds[i];
    }
    pthread_mutex_unlock(&user_mutex);
    
    if (sender_fd == -1 || receiver_fd == -1) {
        log_event("[ERROR] File transfer: user(s) not found in worker. Sender: %s, Receiver: %s", job.sender, job.receiver);
        sem_post(&upload_sem);
        return NULL;
    }

    // Log wait time and notify user
    log_event("[FILE] '%s' from user '%s' started upload after %.0f seconds in queue.", 
             job.filename, job.sender, wait_duration);
    
    // Notify sender about wait time
    char wait_msg[256];
    snprintf(wait_msg, sizeof(wait_msg), 
             "[Server]: File '%s' started uploading after %.0f seconds in queue.\n", 
             job.filename, wait_duration);
    write(sender_fd, wait_msg, strlen(wait_msg));

    // Simulated file transfer success messages
    char success_msg[256];
    snprintf(success_msg, sizeof(success_msg), 
             "[Server]: File '%s' successfully transferred to %s\n", 
             job.filename, job.receiver);
    write(sender_fd, success_msg, strlen(success_msg));
    
    char receive_msg[256];
    snprintf(receive_msg, sizeof(receive_msg), 
             "[Server]: Successfully received file '%s' from %s\n", 
             job.filename, job.sender);
    write(receiver_fd, receive_msg, strlen(receive_msg));
    
    log_event("[FILE] Successfully transferred '%s' from %s to %s", 
             job.filename, job.sender, job.receiver);
    
    // Transfer tamamlandıktan sonra kuyruktan sil
    pthread_mutex_lock(&queue_mutex);
    for (int i = 0; i < queue_size; i++) {
        if (strcmp(upload_queue[i].sender, job.sender) == 0 &&
            strcmp(upload_queue[i].receiver, job.receiver) == 0 &&
            strcmp(upload_queue[i].filename, job.filename) == 0) {
            // Kuyruktaki son elemanı bu pozisyona taşı
            if (i < queue_size - 1) {
                upload_queue[i] = upload_queue[queue_size - 1];
            }
            queue_size--;
            break;
        }
    }
    pthread_mutex_unlock(&queue_mutex);
    
    sem_post(&upload_sem);
    return NULL;
}
