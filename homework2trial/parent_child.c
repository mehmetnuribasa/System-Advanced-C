#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#define FIFO1 "/tmp/fifo1"
#define FIFO2 "/tmp/fifo2"

int counter = 0;

// Signal handler for SIGCHLD to reap child processes
// void sigchld_handler(int signum) {
//     int status;
//     while (waitpid(-1, &status, WNOHANG) > 0) {
//         printf("Child process exited.\n");
//         counter += 2; // Increase counter by 2
//     }
// }

void sigchld_handler(int signum) {
    printf("SIGCHLD received!\n");
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("Child process %d exited.\n", pid);
        counter += 2; // Increase counter by 2
    }
}



int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <int1> <int2>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int num1 = atoi(argv[1]);
    int num2 = atoi(argv[2]);

    // FIFO'ları oluştur
    if (mkfifo(FIFO1, 0666) == -1 && errno != EEXIST) {
        perror("FIFO1 oluşturulamadı");
        exit(EXIT_FAILURE);
    }
    if (mkfifo(FIFO2, 0666) == -1 && errno != EEXIST) {
        perror("FIFO2 oluşturulamadı");
        exit(EXIT_FAILURE);
    }

    printf("FIFO'lar başarıyla oluşturuldu.\n");

    // SIGCHLD sinyalini ayarla
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;  // SA_NOCLDWAIT kaldırıldı
    sigaction(SIGCHLD, &sa, NULL);

    printf("Parent started with PID: %d\n", getpid());

    // FIFO'yu aç ve hemen kapat ki süreçler bloklanmasın
    int fd_write = open(FIFO1, O_WRONLY | O_NONBLOCK);
    if (fd_write == -1) {
        perror("Parent: Error opening FIFO1 for writing");
    } else {
        close(fd_write);
    }

    // Child 1 başlat
    pid_t child1 = fork();
    if (child1 == 0) {
        printf("Child 1 started with PID: %d\n", getpid());

        int fd = open(FIFO1, O_WRONLY);
        if (fd == -1) {
            perror("Child 1: Error opening FIFO1");
            exit(EXIT_FAILURE);
        }
        write(fd, &num1, sizeof(int));
        write(fd, &num2, sizeof(int));
        close(fd);

        printf("Child 1 exiting...\n");
        exit(EXIT_SUCCESS);
    }

    // Child 2 başlat
    pid_t child2 = fork();
    if (child2 == 0) {
        printf("Child 2 started with PID: %d\n", getpid());

        int fd = open(FIFO1, O_RDONLY);
        if (fd == -1) {
            perror("Child 2: Error opening FIFO1");
            exit(EXIT_FAILURE);
        }
        int larger;
        read(fd, &larger, sizeof(int));
        printf("Larger number: %d\n", larger);
        close(fd);

        printf("Child 2 exiting...\n");
        exit(EXIT_SUCCESS);
    }

    // Parent loop
    while (counter < 4) {
        printf("Parent proceeding...\n");
        printf("Counter: %d\n", counter);
        sleep(2);
    }

    printf("Parent process exiting.\n");
    return 0;
}
