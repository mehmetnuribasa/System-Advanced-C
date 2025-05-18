#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include "buffer.h"

#define MAX_LINE_LENGTH 1024
#define MAX_WORKERS 100

int buffer_size;
int num_workers;
char* log_file_path;
char* search_term;
pthread_t workers[MAX_WORKERS];
pthread_barrier_t barrier;

// Each worker keeps track of how many matches it found here
int* match_counts;

// For SIGINT
void sigint_handler(int signo) {
    printf("\nSIGINT received. Cleaning up...\n");
    destroy_buffer(); // Frees all lines in the buffer
    free(match_counts); // Frees the worker match count array
    pthread_barrier_destroy(&barrier); // Destroys the barrier
    exit(EXIT_FAILURE);
}

// Worker thread function
void* worker_function(void* arg) {
    int id = *(int*)arg;
    int local_count = 0;

    while (1) {
        char* line = get_line_from_buffer();

        if (line == NULL) break; // EOF marker

        if (strstr(line, search_term) != NULL) {
            local_count++;
        }

        free(line); // Free the line since it came from the heap
    }

    match_counts[id] = local_count;

    pthread_barrier_wait(&barrier); // Synchronization
    pthread_exit(NULL);
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <buffer_size> <num_workers> <log_file> <search_term>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Parse arguments
    buffer_size = atoi(argv[1]);
    num_workers = atoi(argv[2]);
    log_file_path = argv[3];
    search_term = argv[4];

    if (buffer_size <= 0 || num_workers <= 0 || num_workers > MAX_WORKERS) {
        fprintf(stderr, "Invalid buffer size or number of workers.\n");
        exit(EXIT_FAILURE);
    }

    // SIGINT handler
    signal(SIGINT, sigint_handler);

    // Initialize the buffer
    init_buffer(buffer_size);

    // Allocate the match count array
    match_counts = malloc(sizeof(int) * num_workers);
    if (match_counts == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    // Initialize the barrier
    pthread_barrier_init(&barrier, NULL, num_workers);

    // Array to assign IDs to workers
    int* ids = malloc(sizeof(int) * num_workers);

    // Create worker threads
    for (int i = 0; i < num_workers; ++i) {
        ids[i] = i;
        pthread_create(&workers[i], NULL, worker_function, &ids[i]);
    }

    // Read the log file and write lines to the buffer
    FILE* fp = fopen(log_file_path, "r");
    if (!fp) {
        perror("fopen");
        destroy_buffer(); // Clean up the buffer in case of error
        free(match_counts);
        pthread_barrier_destroy(&barrier);
        exit(EXIT_FAILURE);
    }

    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), fp)) {
        char* copy = strdup(line); // Copy to the heap
        if (copy == NULL) {
            perror("strdup");
            fclose(fp);
            destroy_buffer(); // Clean up the buffer in case of error
            free(match_counts);
            pthread_barrier_destroy(&barrier);
            exit(EXIT_FAILURE);
        }
        put_line_to_buffer(copy);
        free(copy); // Free after calling `put_line_to_buffer`
    }

    fclose(fp);

    // Send EOF signal to workers (NULL for each)
    for (int i = 0; i < num_workers; ++i) {
        put_line_to_buffer(NULL);
    }

    // Join workers
    for (int i = 0; i < num_workers; ++i) {
        pthread_join(workers[i], NULL);
    }

    // Summary report (only one can write after the barrier)
    printf("\n--- Match Report ---\n");
    int total = 0;
    for (int i = 0; i < num_workers; ++i) {
        printf("Worker %d found %d matches.\n", i, match_counts[i]);
        total += match_counts[i];
    }
    printf("Total matches: %d\n", total);

    // Cleanup
    destroy_buffer(); // Frees all lines in the buffer
    pthread_barrier_destroy(&barrier); // Destroys the barrier
    free(match_counts); // Frees the worker match count array
    free(ids); // Frees the worker ID array

    return 0;
}
