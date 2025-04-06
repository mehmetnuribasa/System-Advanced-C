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
#include <poll.h>

#define FIFO1 "/tmp/fifo1"  
#define FIFO2 "/tmp/fifo2"
#define DAEMON_FIFO "/tmp/daemon_fifo"
#define STATUS_FIFO "/tmp/status_fifo"

int counter = 0;  // Counter for parent process
int signal_pipe[2];  // Pipe for signal handling [0: read end, 1: write end]

// Signal handler for SIGCHLD
void sigchld_handler(int signum) {
    (void)signum; // Mark the parameter as unused
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
        close(fd);

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
    char signal_char;
    if (sig == SIGTERM) {
        signal_char = 'T';  // SIGTERM
    } else if (sig == SIGHUP) {
        signal_char = 'H';  // SIGHUP
    } else if (sig == SIGUSR1) {
        signal_char = 'U';  // SIGUSR1
    } else {
        return;
    }

    // Write the signal to the pipe
    write(signal_pipe[1], &signal_char, 1);
}

 
void daemon_process(pid_t parent_pid) {
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

    struct pollfd pfd;
    pfd.fd = signal_pipe[0];  // Monitor the read end of the pipe
    pfd.events = POLLIN;      // Check for data to read

    int num1, num2;
    char buf[128];
    char signal_char;

    while (1) {
        // Check if the parent process is still alive
        if (getppid() != parent_pid) {
            log_file = fopen("daemon_log.txt", "a");
            if (log_file) {
                fprintf(log_file, "[%ld] Daemon: Parent process terminated. Exiting...\n", time(NULL));
                fclose(log_file);
            }
            break;
        }

        // Use poll() to wait for signals with a timeout
        int poll_result = poll(&pfd, 1, 1000);  // 1-second timeout
        if (poll_result == -1) {
            perror("Daemon: poll() failed");
            break;
        }

        // Check if there is data to read from the pipe
        if (poll_result > 0 && (pfd.revents & POLLIN)) {
            if (read(signal_pipe[0], &signal_char, 1) > 0) {
                if (signal_char == 'T') {
                    fprintf(stderr, "[%ld] Daemon: Received SIGTERM. Exiting...\n", time(NULL));
                    break;
                } else if (signal_char == 'H') {
                    fprintf(stderr, "[%ld] Daemon: Received SIGHUP. Reloading configuration...\n", time(NULL));
                } else if (signal_char == 'U') {
                    fprintf(stderr, "[%ld] Daemon: Received SIGUSR1. Performing custom action...\n", time(NULL));
                }
            }
        }

        
        // Continue processing main tasks even if no signal is received
        // Reading numbers from DAEMON_FIFO
        if (read(daemon_fd, &num1, sizeof(int)) > 0 && read(daemon_fd, &num2, sizeof(int)) > 0) {
            log_file = fopen("daemon_log.txt", "a");
            if (log_file) {
                fprintf(log_file, "[%ld] Daemon logged: Received numbers %d and %d\n", time(NULL), num1, num2);
                fclose(log_file);
            }
        }

        ssize_t n;
        // Read log messages from DAEMON_FIFO and write to daemon_log.txt
        while ((n = read(daemon_fd, buf, sizeof(buf) - 1)) > 0) {
            buf[n] = '\0';
            log_file = fopen("daemon_log.txt", "a");
            if (log_file) {
                fprintf(log_file, "%s", buf);
                fclose(log_file);
            }
        }

        // Exit statuses are logged to the log file
        while ((n = read(status_fd, buf, sizeof(buf) - 1)) > 0) {
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

        sleep(1);  // Prevent excessive CPU usage
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


    // create FIFO1
    if (mkfifo(FIFO1, 0666) == -1 && errno != EEXIST) {
        perror("FIFO1 can not be created");
        exit(EXIT_FAILURE);
    }

    // create FIFO2
    if (mkfifo(FIFO2, 0666) == -1 && errno != EEXIST) {
        perror("FIFO2 can not be created");
        exit(EXIT_FAILURE);
    }
    // create DAEMON_FIFO
    if (mkfifo(DAEMON_FIFO, 0666) == -1 && errno != EEXIST) {
        perror("DAEMON FIFO can not be created");
        exit(EXIT_FAILURE);
    }
    // create STATUS_FIFO
    if (mkfifo(STATUS_FIFO, 0666) == -1 && errno != EEXIST) {
        perror("STATUS FIFO can not be created");
        exit(EXIT_FAILURE);
    }

    // Create the signal pipe
    if (pipe(signal_pipe) == -1) {
        perror("Failed to create signal pipe");
        exit(EXIT_FAILURE);
    }


    printf("Parent started with PID: %d\n", getpid());
    pid_t parent_pid = getpid();


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
        
        daemon_process(parent_pid);
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
        printf("Cild 2 startedwith PID: %d\n", getpid());
        
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
        printf("***Larger number: %d***\n", larger);
        close(fd);

        exit(EXIT_SUCCESS);
    }


    // Parent process loop
    while (counter < 4) {
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
