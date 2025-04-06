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
#include <string.h>

#define FIFO1 "/tmp/fifo1"  
#define FIFO2 "/tmp/fifo2"
#define DAEMON_FIFO "/tmp/daemon_fifo"
#define STATUS_FIFO "/tmp/status_fifo"

#define TIMEOUT_SECONDS 10

typedef struct {
    volatile pid_t pid;
    volatile time_t last_active;
} child_info;

volatile child_info children[2];  // 0: child1, 1: child2

volatile sig_atomic_t terminate_daemon = 0;
int counter = 0;
volatile pid_t parent_pid;


void sigchld_handler(int signum) {
    

    printf("SIGCHLD received!\n");  //JBLCejşwvjwilrjbmlkem
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {

        // Write exit statuses to the daemon via STATUS_FIFO
        int fd = open(STATUS_FIFO, O_WRONLY | O_NONBLOCK);
        if (fd == -1) {
            perror("sigchld_handler: Failed to open STATUS_FIFO");
            return;
        }
        dprintf(fd, "%d %d\n", pid, WEXITSTATUS(status));
        close(fd); // Close STATUS_FIFO after writing

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

//BU KSIIMD DAEMON KENDİNİ KAPATIYOR; ACABA TÜM PROGRMI MI KAPATACAK
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



    FILE *log_file = fopen("daemon_log.txt", "a");  // Log file (append mode)
    if (!log_file) {
        perror("Daemon: Failed to open log file");
        exit(EXIT_FAILURE);
    }

    time_t start_time = time(NULL);
    fprintf(log_file, "[%ld] Daemon started with PID: %d\n", start_time, getpid());
    fclose(log_file);


    
    int daemon_fd = open(DAEMON_FIFO, O_RDONLY | O_NONBLOCK);
    // int daemon_fd = open(DAEMON_FIFO, O_RDONLY);
    if (daemon_fd == -1) {
        perror("Daemon: Error opening FIFO");
        exit(EXIT_FAILURE);
    }

    int status_fd = open(STATUS_FIFO, O_RDONLY);  // Use blocking mode
    if (status_fd == -1) {
        perror("Failed to open status FIFO");
        exit(EXIT_FAILURE);
    }

    int num1, num2;
    char buf[128];
    while (!terminate_daemon) {

        // Check if the parent process is still alive
        if(getppid() != parent_pid) {
            break;
        }

        //reading numbers are writen to the log file
        if(read(daemon_fd, &num1, sizeof(int)) > 0 && read(daemon_fd, &num2, sizeof(int)) > 0) {
            log_file = fopen("daemon_log.txt", "a");
            if (log_file) {
                fprintf(log_file, "[%ld] Daemon logged: Received numbers %d and %d\n", time(NULL), num1, num2);
                fclose(log_file);
            }
        }

        ssize_t n;
        //ADDED
        // Read log messages from DAEMON_FIFO and write to daemon_log.txt
        while ((n = read(daemon_fd, buf, sizeof(buf) - 1)) > 0) {
            buf[n] = '\0';
            log_file = fopen("daemon_log.txt", "a");
            if (log_file) {
                fprintf(log_file, "%s", buf);
                fclose(log_file);
            }
        }
        

        //exit status are logged to the log file.
        while ((n = read(status_fd, buf, sizeof(buf) - 1)) > 0) {  // Read all available data
            buf[n] = '\0';
            char *line = strtok(buf, "\n");
            while (line != NULL) {
                int pid, status;
                if (sscanf(line, "%d %d", &pid, &status) == 2) {
                    log_file = fopen("daemon_log.txt", "a");
                    if (log_file) {
                        fprintf(log_file, "[%ld] Child process %d exited with status %d\n", time(NULL), pid, status);
                        fclose(log_file);
                    }
                } else {
                    fprintf(stderr, "Daemon: Failed to parse status message: %s\n", line);
                }
                line = strtok(NULL, "\n");
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
    close(status_fd);
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
        perror("FIFO1 can not be created");
        exit(EXIT_FAILURE);
    }

    // FIFO2 oluştur
    if (mkfifo(FIFO2, 0666) == -1 && errno != EEXIST) {
        perror("FIFO2 can not be created");
        exit(EXIT_FAILURE);
    }

    if (mkfifo(DAEMON_FIFO, 0666) == -1 && errno != EEXIST) {
        perror("DAEMON FIFO can not be created");
        exit(EXIT_FAILURE);
    }

    if (mkfifo(STATUS_FIFO, 0666) == -1 && errno != EEXIST) {
        perror("STATUS FIFO can not be created");
        exit(EXIT_FAILURE);
    }


    printf("Parent started with PID: %d\n", getpid()); //E'UIHOIŞE'jceınjkrvb
    parent_pid = getpid();


    // Create signal handler for SIGCHLD
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);


    // Create Daemon process
    pid_t daemon_pid = fork();
    if (daemon_pid == 0) {
        // Create a new session and process group 
        if (setsid() == -1) {
            perror("setsid failed");
            exit(EXIT_FAILURE);
        }
        umask(0);

        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        
        daemon_process();
    }
    
    // sleep(2);
    // Send the numbers from Parent to Daemon
    int daemon_fd = open(DAEMON_FIFO, O_WRONLY);
    if (daemon_fd != -1) {
        write(daemon_fd, &num1, sizeof(int));
        write(daemon_fd, &num2, sizeof(int));
        close(daemon_fd);
    }


    // Fork first child process (compares and writes the larger number to FIFO2)
    pid_t child1 = fork();
    if (child1 == 0) {
        sleep(10);
        children[0].pid = getpid();
        children[0].last_active = time(NULL);

        printf("Child 1 started with PID: %d\n", getpid());
        //ADDED
        // Log the message to daemon_log.txt
        int log_fd = open(DAEMON_FIFO, O_WRONLY | O_NONBLOCK);
        if (log_fd != -1) {
            char log_message[128];
            snprintf(log_message, sizeof(log_message), "[%ld] Child 1 started with PID: %d\n", time(NULL), getpid());
            write(log_fd, log_message, strlen(log_message));
            close(log_fd);
        }

        // Read the  numbers from FIFO1
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

        // Write the larger number to FIFO2
        int fifo2_fd = open(FIFO2, O_WRONLY);
        if (fifo2_fd == -1) {
            perror("Child 1: Error opening FIFO2");
            exit(EXIT_FAILURE);
        }
        write(fifo2_fd, &larger, sizeof(int));
        close(fifo2_fd);

        // Log the comparison result
        printf("Child 1 exiting...\n");
        exit(EXIT_SUCCESS);
    }

    // First, Parent process sends the numbers to the FIFO1
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
        sleep(10);
        
        children[1].pid = getpid();
        children[1].last_active = time(NULL);

        printf("Child 2 started with PID: %d\n", getpid());
        //ADDED
        // Log the message to daemon_log.txt
        int log_fd = open(DAEMON_FIFO, O_WRONLY | O_NONBLOCK);
        if (log_fd != -1) {
            char log_message[128];
            snprintf(log_message, sizeof(log_message), "[%ld] Child 2 started with PID: %d\n", time(NULL), getpid());
            write(log_fd, log_message, strlen(log_message));
            close(log_fd);
        }

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

        
        printf("Child 2 exiting...\n"); //e2uıfhşoıejı2fıoejpği
        exit(EXIT_SUCCESS);
    }


    // Parent process loop
    while (counter < 4 && !terminate_daemon) {
        printf("Parent proceeding...\n");
        sleep(2);
    }

    unlink(FIFO1);
    unlink(FIFO2);
    unlink(DAEMON_FIFO);
    unlink(STATUS_FIFO);

    printf("Parent process exiting.\n");
    return 0;
}
