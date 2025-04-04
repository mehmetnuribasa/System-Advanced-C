#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#define FIFO1 "/tmp/fifo1"
#define FIFO2 "/tmp/fifo2"
#define DAEMON_FIFO "/tmp/daemon_fifo"

volatile sig_atomic_t terminate_daemon = 0;
int counter = 0;



void sigchld_handler(int signum) {
    printf("SIGCHLD received!\n");
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // Zombie protection
        if (WIFEXITED(status)) {
            printf("Child process %d exited with status %d.\n", pid, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("Child process %d was terminated by signal %d.\n", pid, WTERMSIG(status));
        } else if (WIFSTOPPED(status)) {
            printf("Child process %d was stopped by signal %d.\n", pid, WSTOPSIG(status));
        } else {
            printf("Child process %d terminated in an unknown way.\n", pid);
        }
        counter += 2; // Increase counter by 2
    }
}

void handle_signal(int sig) {
    FILE *log_file = fopen("daemon_log.txt", "a");
    if (!log_file) return;

    time_t now = time(NULL);
    if (sig == SIGTERM) {
        fprintf(log_file, "[%ld] Daemon received SIGTERM, terminating...\n", now);
        terminate_daemon = 1;
    } else if (sig == SIGHUP) {
        fprintf(log_file, "[%ld] Daemon received SIGHUP, reloading...\n", now);
    } else if (sig == SIGUSR1) {
        fprintf(log_file, "[%ld] Daemon received SIGUSR1\n", now);
    }
    fclose(log_file);
}


void daemon_process() {
    // Redirect stdout and stderr to log files
    freopen("daemon_output.log", "a", stdout);
    freopen("daemon_error.log", "a", stderr);

    // Signal handlers
    signal(SIGTERM, handle_signal);
    signal(SIGHUP, handle_signal);
    signal(SIGUSR1, handle_signal);



    FILE *log_file = fopen("daemon_log.txt", "a");  // Log dosyası (append mode)
    if (!log_file) {
        perror("Daemon: Failed to open log file");
        exit(EXIT_FAILURE);
    }

    time_t start_time = time(NULL);
    fprintf(log_file, "[%ld] Daemon started with PID: %d\n", start_time, getpid());
    fclose(log_file);



    int daemon_fd = open(DAEMON_FIFO, O_RDONLY);
    if (daemon_fd == -1) {
        perror("Daemon: Error opening FIFO");
        exit(EXIT_FAILURE);
    }

    int num1, num2;
    while (read(daemon_fd, &num1, sizeof(int)) > 0 && read(daemon_fd, &num2, sizeof(int)) > 0) {
        
        if(!terminate_daemon) {     //UI'hejfiğeq BURAYA BAKILACAK
            log_file = fopen("daemon_log.txt", "a");
            if (log_file) {
                fprintf(log_file, "[%ld] Daemon logged: Received numbers %d and %d\n", time(NULL), num1, num2);
                fclose(log_file);
            }
        }
        
        sleep(1);  // Daemon fazla CPU harcamasın
    }


    log_file = fopen("daemon_log.txt", "a");
    if (log_file) {
        fprintf(log_file, "[%ld] Daemon exiting. PID: %d\n", time(NULL), getpid());
        fflush(log_file);
        fclose(log_file);
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

    // FIFO'yu açmaya çalışmadan önce FIFO'nun doğru şekilde oluşturulduğundan emin olalım.
    if (access(FIFO1, F_OK) != 0) {
        perror("FIFO1 mevcut değil");
        exit(EXIT_FAILURE);
    }



    printf("Parent started with PID: %d\n", getpid()); //E'UIHOIŞE'jceınjkrvb





    // Create signal handler for SIGCHLD
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);


    

     // Daemon process oluştur
     pid_t daemon_pid = fork();
     if (daemon_pid == 0) {
        // Yeni oturum başlat
        if (setsid() == -1) {
            perror("setsid failed");
            exit(EXIT_FAILURE);
        }
        
        // if (chdir("/") == -1) {
        //     perror("chdir failed");
        //     exit(EXIT_FAILURE);
        // }
    
        umask(0);
    
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        
        daemon_process();
    }
    
    // sleep(2);
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

        printf("Child1: num1: %d, num2: %d\n", received_num1, received_num2);

        int larger = (received_num1 > received_num2) ? received_num1 : received_num2;

           // **Child 1, FIFO2'ye yazmadan önce biraz bekleyelim**
        sleep(2);
        // Write the larger number to FIFO2
        int fifo2_fd = open(FIFO2, O_WRONLY);
        if (fifo2_fd == -1) {
            perror("Child 1: Error opening FIFO2");
            exit(EXIT_FAILURE);
        }
        write(fifo2_fd, &larger, sizeof(int));
        close(fifo2_fd);

        printf("Child 1: Wrote larger number %d to FIFO2\n", larger);   //kee2ıofşo2jofciwv

        for (int i = 0; i < 5; i++) {
            printf("Child 1 running... (Iteration %d)\n", i);
            sleep(1);
        }

        // Log the comparison result
        printf("Child 1 exiting...\n");
        sleep(10);
        exit(EXIT_SUCCESS);
    }




    
    // **2. Parent -> FIFO1'e yazmadan önce Child 1'in başlamasını bekle**
    sleep(2);
     // First, Parent -> "FIFO1"  'e veri gönderme
    int fifo1_fd = open(FIFO1, O_WRONLY);
    if (fifo1_fd == -1) {
        perror("Parent: Error opening FIFO1");
        exit(EXIT_FAILURE);
    }

    write(fifo1_fd, &num1, sizeof(int));
    write(fifo1_fd, &num2, sizeof(int));
    close(fifo1_fd);
    printf("Parent: Sent numbers %d and %d to FIFO1\n", num1, num2);








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

        for (int i = 0; i < 5; i++) {
            printf("Child 2 running... (Iteration %d)\n", i);
            sleep(1);
        }
        
        printf("Child 2 exiting...\n"); //e2uıfhşoıejı2fıoejpği
        sleep(10);
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
