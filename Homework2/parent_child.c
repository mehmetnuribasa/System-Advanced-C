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
#define DAEMON_FIFO "/tmp/daemon_fifo"

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

// Daemon process function
void daemon_process() {
    printf("Daemon started with PID: %d\n", getpid());

    // Daemon için FIFO'ları aç
    int daemon_fd = open(DAEMON_FIFO, O_RDONLY);
    if (daemon_fd == -1) {
        perror("Daemon: Error opening FIFO");
        exit(EXIT_FAILURE);
    }

    // FIFO'dan gelen verileri sadece logla
    int num1, num2;
    while (read(daemon_fd, &num1, sizeof(int)) > 0 && read(daemon_fd, &num2, sizeof(int)) > 0) {
        printf("Daemon logged: Received numbers %d and %d\n", num1, num2);
    }

    close(daemon_fd);
    exit(EXIT_SUCCESS);
}


int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <int1> <int2>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int num1 = atoi(argv[1]);
    int num2 = atoi(argv[2]);





    // FIFO1 oluştur
    if (mkfifo(FIFO1, 0666) == -1 && errno != EEXIST) {
        perror("FIFO1 oluşturulamadı");
        exit(EXIT_FAILURE);
    }

    // FIFO2 oluştur
    if (mkfifo(FIFO2, 0666) == -1 && errno != EEXIST) {
        perror("FIFO2 oluşturulamadı");
        exit(EXIT_FAILURE);
    }

    if (mkfifo(DAEMON_FIFO, 0666) == -1 && errno != EEXIST) {
        perror("DAEMON oluşturulamadı");
        exit(EXIT_FAILURE);
    }

    printf("FIFO'lar başarıyla oluşturuldu.\n");




    // Create signal handler for SIGCHLD
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);


    
    printf("Parent started with PID: %d\n", getpid()); //E'UIHOIŞE'jceınjkrvb

    // FIFO'yu aç ve hemen kapat ki süreçler bloklanmasın
    // int fd_write = open(FIFO1, O_WRONLY | O_NONBLOCK);
    // if (fd_write == -1) {
    //     perror("Parent: Error opening FIFO1 for writing");
    // } else {
    //     close(fd_write);
    // }




     // Daemon process oluştur
     pid_t daemon_pid = fork();
     if (daemon_pid == 0) {
         // Yeni oturum başlat
         setsid();
         chdir("/");
         umask(0);
         close(STDIN_FILENO);
         close(STDOUT_FILENO);
         close(STDERR_FILENO);
 
         daemon_process();
     }
 
     // Parent -> Daemon'a veri gönder
     int daemon_fd = open(DAEMON_FIFO, O_WRONLY);
     if (daemon_fd != -1) {
         write(daemon_fd, &num1, sizeof(int));
         write(daemon_fd, &num2, sizeof(int));
         close(daemon_fd);
     }



    // Fork first child process
    pid_t child1 = fork();
    if (child1 == 0) {
        printf("Child 1 started with PID: %d\n", getpid());

        // First child process (compares and writes the larger number to FIFO2)
        int fd = open(FIFO1, O_RDONLY);
        if (fd == -1) {
            perror("Child 1: Error opening FIFO1");
            exit(EXIT_FAILURE);
        }

        int received_num1, received_num2;
        read(fd, &received_num1, sizeof(int));
        read(fd, &received_num2, sizeof(int));
        close(fd);

        int larger = (received_num1 > received_num2) ? received_num1 : received_num2;
        int smaller = (received_num1 < received_num2) ? received_num1 : received_num2;

        // Write the larger number to FIFO2
        int fifo2_fd = open(FIFO2, O_WRONLY);
        if (fifo2_fd == -1) {
            perror("Child 1: Error opening FIFO2");
            exit(EXIT_FAILURE);
        }
        write(fifo2_fd, &larger, sizeof(int));
        close(fifo2_fd);

        // Log the comparison result
        printf("Child 1: Larger number is %d\n", larger);
        printf("Child 1 exiting...\n");
        exit(EXIT_SUCCESS);
    }





    // Fork second child process
    pid_t child2 = fork();
    if (child2 == 0) {
        printf("Child 2 started with PID: %d\n", getpid()); //CEuıhuoşf2eqjkvbjeqnnvıeqnvjknrşwjvnşwlk

        // Second child process (reads larger number from FIFO2)
        int fd = open(FIFO2, O_RDONLY);

        if (fd == -1) {
            perror("Child 2: Error opening FIFO2");
            exit(EXIT_FAILURE);
        }
        int larger;
        read(fd, &larger, sizeof(int));
        printf("Larger number: %d\n", larger);
        close(fd);
        // sleep(2); // Simulate processing delay
        
        printf("Child 2 exiting...\n"); //e2uıfhşoıejı2fıoejpği
        exit(EXIT_SUCCESS);
    }


    // Parent process loop
    while (counter < 4) {
        printf("Parent proceeding...\n");
        printf("Counter: %d\n", counter);   // deneme için eklendi
        sleep(2);
    }

    printf("Parent process exiting.\n");
    return 0;
}
