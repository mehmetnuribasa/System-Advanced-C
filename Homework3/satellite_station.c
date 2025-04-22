#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

#define NUM_ENGINEERS 3
#define NUM_SATELLITES 5
#define MAX_QUEUE_SIZE 100

// Struct for satellite requests
typedef struct {
    int id;
    int priority;
    int timeout;         // in seconds
    time_t arrivalTime;
} SatelliteRequest;

// Global Variables
int availableEngineers = NUM_ENGINEERS;
int satellitesCompleted = 0; // Global counter for completed satellite threads

// Request queue
SatelliteRequest requestQueue[MAX_QUEUE_SIZE];
int queueSize = 0;

// Mutex and Semaphores
pthread_mutex_t engineerMutex;
sem_t newRequest;
sem_t requestHandled;

// Thread functions
void* satellite(void* arg);
void* engineer(void* arg);

// Priority Queue functions
void enqueue_request(SatelliteRequest req) {
    pthread_mutex_lock(&engineerMutex);
    if (queueSize < MAX_QUEUE_SIZE) {
        // Insert request based on priority (lower priority value first)
        int i = queueSize - 1;
        while (i >= 0 && requestQueue[i].priority > req.priority) {
            requestQueue[i + 1] = requestQueue[i];
            i--;
        }
        requestQueue[i + 1] = req;
        queueSize++;
    }
    pthread_mutex_unlock(&engineerMutex);
}

int dequeue_highest_priority(SatelliteRequest* outRequest) {
    pthread_mutex_lock(&engineerMutex);

    if (queueSize == 0) {
        pthread_mutex_unlock(&engineerMutex);
        return 0;
    }

    // Dequeue the first request (highest priority due to sorted order)
    *outRequest = requestQueue[0];
    for (int i = 0; i < queueSize - 1; i++) {
        requestQueue[i] = requestQueue[i + 1];
    }
    queueSize--;

    pthread_mutex_unlock(&engineerMutex);
    return 1;
}

// Main Function
int main() {
    srand(time(NULL));

    // Initialize mutex and semaphores
    pthread_mutex_init(&engineerMutex, NULL);
    sem_init(&newRequest, 0, 0);
    sem_init(&requestHandled, 0, 0);

    // Create engineer threads
    pthread_t engineers[NUM_ENGINEERS];
    for (int i = 0; i < NUM_ENGINEERS; i++) {
        pthread_create(&engineers[i], NULL, engineer, (void*)(long)i);
    }

    // Create satellite threads
    pthread_t satellites[NUM_SATELLITES];
    for (int i = 0; i < NUM_SATELLITES; i++) {
        pthread_create(&satellites[i], NULL, satellite, (void*)(long)i);
    }

    // Wait for satellite threads to finish
    for (int i = 0; i < NUM_SATELLITES; i++) {
        pthread_join(satellites[i], NULL);
    }

    // Signal that all satellites are done
    pthread_mutex_lock(&engineerMutex);
    satellitesCompleted = NUM_SATELLITES; // Directly set to total satellites
    pthread_mutex_unlock(&engineerMutex);

    // Post to newRequest semaphore to wake up engineers
    for (int i = 0; i < NUM_ENGINEERS; i++) {
        sem_post(&newRequest);
    }

    // Wait for engineer threads to finish
    for (int i = 0; i < NUM_ENGINEERS; i++) {
        pthread_join(engineers[i], NULL);
    }

    // Cleanup
    pthread_mutex_destroy(&engineerMutex);
    sem_destroy(&newRequest);
    sem_destroy(&requestHandled);

    return 0;
}

// Satellite thread function
void* satellite(void* arg) {
    int id = (int)(long)arg;

    sleep(rand() % 10);  // simulate random arrival

    SatelliteRequest req;
    req.id = id;
    req.priority = rand() % 3 + 1;  // 1 to 3 (1 is highest priority)
    req.timeout = 5;               // Fixed timeout of 5 seconds
    req.arrivalTime = time(NULL);

    printf("[SATELLITE] Satellite %d requesting (Priority: %d)\n", id, req.priority);

    enqueue_request(req);
    sem_post(&newRequest); // notify engineers

    // Wait for response with timeout
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += req.timeout;

    if (sem_timedwait(&requestHandled, &ts) == -1) {
        printf("[TIMEOUT] Satellite %d timeout %d seconds.\n", id, req.timeout);
    }

    pthread_exit(NULL);
}

// Engineer thread function
void* engineer(void* arg) {
    int id = (int)(long)arg;

    while (1) {
        sem_wait(&newRequest); // Wait for a new request

        pthread_mutex_lock(&engineerMutex);

        // Check if all satellites are done and queue is empty
        if (satellitesCompleted == NUM_SATELLITES && queueSize == 0) {
            pthread_mutex_unlock(&engineerMutex);
            printf("[ENGINEER %d] Exiting...\n", id);
            pthread_exit(NULL);
        }

        pthread_mutex_unlock(&engineerMutex);

        SatelliteRequest req;
        if (dequeue_highest_priority(&req)) {
            pthread_mutex_lock(&engineerMutex);
            availableEngineers--;
            pthread_mutex_unlock(&engineerMutex);

            printf("[ENGINEER %d] Handling satellite %d (Priority %d)\n", id, req.id, req.priority);
            sleep(4); // Simulate handling time

            pthread_mutex_lock(&engineerMutex);
            availableEngineers++;
            pthread_mutex_unlock(&engineerMutex);

            sem_post(&requestHandled); // Signal request handled
            printf("[ENGINEER %d] Finished satellite %d.\n", id, req.id);
        }
    }
}
